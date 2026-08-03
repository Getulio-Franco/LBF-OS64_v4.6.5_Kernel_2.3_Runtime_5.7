#include "ehci_storage.h"
#include "ehci_core.h"
#include "../video.h"
#include "../../util/string.h"
#include "../../mem/pmm.h"
#include "../../mem/vmm.h"

static uint32_t global_cbw_tag = 0x99000001;

// Variáveis dinâmicas de endpoint
static uint8_t usb_bulk_in_ep = 0x81;  
static uint8_t usb_bulk_out_ep = 0x02; 

// Sistema anti-XactErr: Rastreadores dinâmicos de Data Toggle
static uint8_t toggle_out = 0;
static uint8_t toggle_in = 0;

// Flag interna para controle de Lazy Initialization unificado
static bool ehci_hardware_inicializado = false;

extern ehci_qh_t* async_head_virt;

static uint32_t gerar_proximo_tag(void) {
    return global_cbw_tag++;
}

/**
 * PONTO 1: FUNÇÃO AUXILIAR DE MAPEAMENTO INTERNO
 * Traduz o ID lógico da unidade do SO (D:=1, E:=2) para o endereço físico USB (geralmente 1)
 */
/*static uint8_t obter_endereco_usb_por_dev_id(uint8_t dev_id) {
    if (dev_id == 1 || dev_id == 2) {
        return 1; // O pendrive físico é mapeado como dispositivo 1 no barramento
    }
    return 0;
}*/

// 1. Corrija a tradução para que cada dev_id tenha seu endereço USB correspondente
static uint8_t obter_endereco_usb_por_dev_id(uint8_t dev_id) {
    if (dev_id == 1) return 1; // Unidade D: -> Endereço USB 1
    if (dev_id == 2) return 2; // Unidade E: -> Endereço USB 2
    return 0;
}

/**
 * Motor de transporte Bulk do EHCI (Com rastreador de DATA0/DATA1)
 * MANTIDO INTOCADO: Continua esperando o 'addr' físico do hardware.
 */
int ehci_enviar_bulk(uint8_t addr, uint8_t endpoint, void* data_buf, uint32_t data_len, int direction_in) {
    if (!async_head_virt) return 0; 
    
    if (data_len > 3500) {
        terminal_print("EHCI_BULK: Erro - buffer excedido!\n");
        return 0;
    }
    
    void* bloco_phys_ptr = pmm_alloc_block();
    if (!bloco_phys_ptr) return 0;
    
    uint64_t bloco_phys_64 = (uint64_t)bloco_phys_ptr;
    uint32_t bloco_phys = (uint32_t)bloco_phys_64;
    
    vmm_map_area(bloco_phys_64, bloco_phys_64, 4096, PAGE_PRESENT | PAGE_WRITE | PAGE_PCD | PAGE_PWT);
    void* bloco_virt = (void*)(uintptr_t)bloco_phys_64;
    memset(bloco_virt, 0, 4096);
    
    ehci_qh_t* qh = (ehci_qh_t*)bloco_virt;
    ehci_qtd_t* qtd_bulk = (ehci_qtd_t*)(bloco_virt + 0x80);
    uint32_t buffer_phys = bloco_phys + 0x100;
    void* buffer_virt = bloco_virt + 0x100;
    
    uint32_t ep_num = endpoint & 0x0F;
    
    // Configura QH (DTC=1 para assumir o controle manual do Data Toggle)
    uint32_t ep_char = (addr & 0x7F) | (ep_num << 8) | (2 << 12) | (512 << 16) | (1 << 14);
    qh->horizontal_link = 1; 
    qh->endpoint_char = ep_char;
    qh->endpoint_caps = (1 << 30);
    qh->overlay.next_qtd = 1;
    qh->overlay.alt_next_qtd = 1;
    qh->overlay.token = 0;
    
    uint8_t pid_data = (direction_in) ? EHCI_PID_IN : EHCI_PID_OUT;
    
    if (direction_in == 0 && data_buf && data_len > 0) {
        memcpy(buffer_virt, data_buf, data_len);
    }
    
    qtd_bulk->next_qtd = 1;
    qtd_bulk->alt_next_qtd = 1;
    
    uint32_t p_addr = buffer_phys;
    for (int i = 0; i < 5; i++) {
        qtd_bulk->buffer[i] = p_addr;
        p_addr = (p_addr + 4096) & 0xFFFFF000;
    }
    
    // O SEGREDO: Injeta o Data Toggle correto (0 ou 1) no Bit 31 do qTD
    uint32_t current_toggle = direction_in ? toggle_in : toggle_out;
    qtd_bulk->token = EHCI_T_IOC | (data_len << 16) | (pid_data << 8) | (3 << 10) | (current_toggle << 31) | EHCI_T_ACTIVE;
    
    qh->overlay.next_qtd = bloco_phys + 0x80;
    
    // Inserção na lista
    uint32_t original_link = async_head_virt->horizontal_link;
    qh->horizontal_link = original_link;
    async_head_virt->horizontal_link = bloco_phys | (1 << 1);
    
    int timeout = EHCI_TRANSFER_TIMEOUT;
    while (qtd_bulk->token & EHCI_T_ACTIVE) {
        ehci_wait_ms(1);
        if (--timeout <= 0) break;
    }
    
    async_head_virt->horizontal_link = original_link;
    uint32_t status_token = qtd_bulk->token;
    
    if (timeout <= 0) { terminal_print("EHCI: TIMEOUT\n"); return 0; }
    if (status_token & EHCI_T_HALTED) { terminal_print("EHCI: STALL\n"); return 0; }
    if (status_token & EHCI_T_XACTERR) { return 0; } // XactErr silenciado para não sujar a tela no fallback
    
    // SUCESSO! Precisamos inverter o Data Toggle para a próxima vez!
    uint32_t num_packets = data_len == 0 ? 1 : (data_len + 511) / 512;
    if (num_packets % 2 != 0) {
        if (direction_in) toggle_in ^= 1; else toggle_out ^= 1;
    }
    
    if (direction_in && data_buf && data_len > 0) {
        uint32_t bytes_restantes = (qtd_bulk->token >> 16) & 0x7FFF;
        memcpy(data_buf, buffer_virt, data_len - bytes_restantes);
    }
    
    return 1;
}

/**
 * MANTIDO INTOCADO: Interage diretamente em baixo nível com as rotinas físicas do barramento.
 */
static int ehci_bot_transfer(uint8_t addr, ehci_cbw_t* cbw, void* data_buf, uint16_t data_len) {
    if (!ehci_enviar_bulk(addr, usb_bulk_out_ep, cbw, sizeof(ehci_cbw_t), 0)) return 0;

    if (data_len > 0 && data_buf != NULL) {
        if (cbw->flags & CBW_DIR_IN) {
            if (!ehci_enviar_bulk(addr, usb_bulk_in_ep, data_buf, data_len, 1)) return 0;
        } else {
            if (!ehci_enviar_bulk(addr, usb_bulk_out_ep, data_buf, data_len, 0)) return 0;
        }
    }

    ehci_csw_t csw;
    memset(&csw, 0, sizeof(ehci_csw_t));

    if (!ehci_enviar_bulk(addr, usb_bulk_in_ep, &csw, sizeof(ehci_csw_t), 1)) return 0;

    if (csw.signature != CSW_SIGNATURE || csw.status != 0) return 0;

    return 1;
}

/**
 * MANTIDO INTOCADO: Usado internamente e de forma isolada para acordar os endpoints físicos.
 */
int ehci_storage_init(uint8_t addr, uint32_t* out_max_lba, uint32_t* out_block_size) {
    terminal_print("[USB STORAGE] Acordando endpoints com SET_CONFIGURATION...\n");

    usb_setup_packet_t setup;
    setup.bmRequestType = 0x00; 
    setup.bRequest = 0x09;      
    setup.wValue = 0x0001;      
    setup.wLength = 0x0000;
    setup.wIndex = 0x0000;

    if (!ehci_enviar_controle(addr, &setup, NULL, 0)) {
        terminal_print("[USB STORAGE] FALHA: Nao foi possivel definir a configuracao.\n");
        return 0;
    }

    ehci_wait_ms(50); 

    terminal_print("[USB STORAGE] Buscando portas Mass Storage...\n");

    uint32_t max_lba = 0;
    uint32_t block_size = 0;
    uint8_t ep_pairs[3][2] = { {0x01, 0x81}, {0x02, 0x81}, {0x01, 0x82} };

    for (int p = 0; p < 3; p++) {
        usb_bulk_out_ep = ep_pairs[p][0];
        usb_bulk_in_ep  = ep_pairs[p][1];
        
        toggle_out = 0;
        toggle_in = 0;

        for (int i = 0; i < 3; i++) {
            if (ehci_storage_read_capacity(addr, &max_lba, &block_size)) {
                terminal_print("[USB STORAGE] Capacidade detectada nos EPs: OUT=");
                char b[8]; int_to_string(usb_bulk_out_ep, b); terminal_print(b);
                terminal_print(" IN="); int_to_string(usb_bulk_in_ep, b); terminal_print(b); terminal_print("\n");
                
                if (out_max_lba)     *out_max_lba = max_lba;
                if (out_block_size)   *out_block_size = block_size;
                return 1;
            }
            ehci_wait_ms(50);
        }
    }

    terminal_print("[USB STORAGE] FALHA: Pendrive nao respondeu aos comandos SCSI.\n");
    return 0;
}

/**
 * MANTIDO INTOCADO: Usado internamente na varredura.
 */
int ehci_storage_read_capacity(uint8_t addr, uint32_t* max_lba, uint32_t* block_size) {
    ehci_cbw_t cbw;
    memset(&cbw, 0, sizeof(ehci_cbw_t));

    uint32_t tag = gerar_proximo_tag();
    uint8_t buffer_retorno[8]; 
    memset(buffer_retorno, 0, 8);
    
    char b[32];
    
    cbw.signature = CBW_SIGNATURE;
    cbw.tag = tag;
    cbw.data_transfer_len = 8;
    cbw.flags = CBW_DIR_IN;
    cbw.lun = 0;
    cbw.cb_length = 10;
    cbw.cb[0] = SCSI_CMD_READ_CAPACITY_10;

    if (!ehci_bot_transfer(addr, &cbw, buffer_retorno, 8)) return 0;

    uint32_t lba = (buffer_retorno[0] << 24) | (buffer_retorno[1] << 16) | (buffer_retorno[2] << 8) | buffer_retorno[3];
    uint32_t tamanho = (buffer_retorno[4] << 24) | (buffer_retorno[5] << 16) | (buffer_retorno[6] << 8) | buffer_retorno[7];

    if (max_lba)   *max_lba = lba;
    if (block_size) *block_size = tamanho;

    terminal_print(" -> Max LBA: "); int_to_string(lba, b); terminal_print(b);
    terminal_print(" | Bloco: "); int_to_string(tamanho, b); terminal_print(b);
    terminal_print(" bytes\n");

    return 1;
}

/**
 * PONTO 2: MODIFICADO SENSIVELMENTE (ehci_storage_read_sectors)
 * Agora o primeiro parâmetro recebe o dev_id e faz o controle de inicialização única
 */
int ehci_storage_read_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, void* buffer) {
    
    // Lazy Initialization: Só roda o setup elétrico do barramento USB na primeira requisição lógica
    if (!ehci_hardware_inicializado) {
        // Assume o endereço físico '1' para o primeiro pendrive enumerado no barramento
        uint32_t mlba = 0, bsize = 0;
        if (ehci_storage_init(1, &mlba, &bsize)) {
            ehci_hardware_inicializado = true;
        } else {
            return 0; // Se o hardware físico falhar, aborta a leitura
        }
    }

    // Traduz o dev_id lógico (1 para D:, 2 para E:) para o endereço físico real do dispositivo no barramento USB
    uint8_t real_usb_addr = obter_endereco_usb_por_dev_id(dev_id);
    if (real_usb_addr == 0) return 0;

    ehci_cbw_t cbw;
    memset(&cbw, 0, sizeof(ehci_cbw_t));

    uint32_t total_bytes = count * 512; 
    cbw.signature = CBW_SIGNATURE;
    cbw.tag = gerar_proximo_tag();
    cbw.data_transfer_len = total_bytes;
    cbw.flags = CBW_DIR_IN;
    cbw.lun = 0;
    cbw.cb_length = 10;

    cbw.cb[0] = SCSI_CMD_READ_10;
    cbw.cb[2] = (lba >> 24) & 0xFF;
    cbw.cb[3] = (lba >> 16) & 0xFF;
    cbw.cb[4] = (lba >> 8)  & 0xFF;
    cbw.cb[5] = lba & 0xFF;
    cbw.cb[7] = (count >> 8) & 0xFF;
    cbw.cb[8] = count & 0xFF;

    // Repassa o endereço físico traduzido (real_usb_addr) para que a transferência BOT ocorra com sucesso
    return ehci_bot_transfer(real_usb_addr, &cbw, buffer, total_bytes);
}

/**
 * PONTO 3: MODIFICADO SENSIVELMENTE (ehci_storage_write_sectors)
 * Sincronizado para seguir a mesma regra protetiva da leitura.
 */
int ehci_storage_write_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, const void* buffer) {
    
    // Lazy Initialization protetiva na escrita também
    if (!ehci_hardware_inicializado) {
        uint32_t mlba = 0, bsize = 0;
        if (ehci_storage_init(1, &mlba, &bsize)) {
            ehci_hardware_inicializado = true;
        } else {
            return 0;
        }
    }

    uint8_t real_usb_addr = obter_endereco_usb_por_dev_id(dev_id);
    if (real_usb_addr == 0) return 0;

    ehci_cbw_t cbw;
    memset(&cbw, 0, sizeof(ehci_cbw_t));

    uint32_t total_bytes = count * 512;
    cbw.signature = CBW_SIGNATURE;
    cbw.tag = gerar_proximo_tag();
    cbw.data_transfer_len = total_bytes;
    cbw.flags = CBW_DIR_OUT; 
    cbw.lun = 0;
    cbw.cb_length = 10;

    cbw.cb[0] = SCSI_CMD_WRITE_10;
    cbw.cb[2] = (lba >> 24) & 0xFF;
    cbw.cb[3] = (lba >> 16) & 0xFF;
    cbw.cb[4] = (lba >> 8)  & 0xFF;
    cbw.cb[5] = lba & 0xFF;
    cbw.cb[7] = (count >> 8) & 0xFF;
    cbw.cb[8] = count & 0xFF;

    return ehci_bot_transfer(real_usb_addr, &cbw, (void*)buffer, total_bytes);
}
