#include "ehci_core.h"
#include "ehci_pci.h"
#include "../video.h"
#include "../../util/string.h"
#include "../../mem/pmm.h"
#include "../../mem/vmm.h"

extern volatile ehci_regs_t* ehci_regs;
extern uintptr_t ehci_base_virt;

ehci_qh_t* async_head_virt = NULL;
static uint32_t async_head_phys = 0;
static char str_buf[32];
static uint16_t transfer_tag = 0x1000;

void ehci_wait_ms(uint32_t ms) {
    volatile uint32_t delay = ms * 100000;
    while (delay--) {
        __asm__ volatile ("pause");
    }
}

void ehci_dump_qh(const char* name, ehci_qh_t* qh) {
    if (!qh) return;
    terminal_print(name);
    terminal_print(" QH: hlink="); hex_to_string(qh->horizontal_link, str_buf); terminal_print(str_buf);
    terminal_print(" epchar="); hex_to_string(qh->endpoint_char, str_buf); terminal_print(str_buf);
    terminal_print("\n");
}

void ehci_dump_qtd(const char* name, ehci_qtd_t* qtd) {
    if (!qtd) return;
    terminal_print(name);
    terminal_print(" qTD: next="); hex_to_string(qtd->next_qtd, str_buf); terminal_print(str_buf);
    terminal_print(" token="); hex_to_string(qtd->token, str_buf); terminal_print(str_buf);
    terminal_print("\n");
}

int ehci_core_init(void) {
    terminal_print("EHCI_CORE: Inicializando Core USB 2.0...\n");
    if (ehci_regs == 0) return 0;
    
    // PASSO 1: HALT
    ehci_regs->usbcmd &= ~(1 << 0);
    int timeout = 1000000;
    while (!(ehci_regs->usbsts & (1 << 12))) {
        if (--timeout <= 0) { terminal_print("EHCI_CORE: ERRO - Timeout HALT\n"); return 0; }
    }
    
    // PASSO 2: HCRESET
    ehci_regs->usbcmd |= (1 << 1);
    timeout = 1000000;
    while (ehci_regs->usbcmd & (1 << 1)) {
        if (--timeout <= 0) { terminal_print("EHCI_CORE: ERRO - Timeout HCRESET\n"); return 0; }
    }
    ehci_wait_ms(10);
    
    // PASSO 3: CONFIGURAÇÃO
    ehci_regs->usbsts = 0x3F; // Limpa interrupções antigas
    ehci_regs->usbintr = 0;   // Desativa interrupções
    ehci_regs->configflag = 1; // Roteia as portas para o EHCI (tira do companion)
    
    // PASSO 4: RUN
    ehci_regs->usbcmd |= (1 << 0);
    timeout = 1000000;
    while (ehci_regs->usbsts & (1 << 12)) {
        if (--timeout <= 0) { terminal_print("EHCI_CORE: ERRO - Timeout RUN\n"); return 0; }
    }
    
    terminal_print("EHCI_CORE: Controlador ativo!\n");
    return 1;
}

/*void ehci_detectar_dispositivos(void) {
    volatile uint32_t* hcsparams = (volatile uint32_t*)(ehci_base_virt + 0x04);
    uint8_t n_ports = (*hcsparams) & 0x0F;
    
    for (uint8_t i = 0; i < n_ports; i++) {
        uint32_t port_status = ehci_regs->portsc[i];
        if (port_status & EHCI_PORT_CONNECTED) {
            terminal_print("Porta conectada detectada. Resetando...\n");
            // Limpa bits de mudança (Write 1 to Clear)
            ehci_regs->portsc[i] |= (EHCI_PORT_CHANGE_CONNECT | EHCI_PORT_CHANGE_ENABLE);
            ehci_reset_porta(i);
        }
    }
}*/ // foi alterada pois só acha o pendrive = 1 e precisamos encontra os outros numeros

int ehci_detectar_dispositivos(void) {
    volatile uint32_t* hcsparams = (volatile uint32_t*)(ehci_base_virt + 0x04);
    uint8_t n_ports = (*hcsparams) & 0x0F;
    
    for (uint8_t i = 0; i < n_ports; i++) {
        uint32_t port_status = ehci_regs->portsc[i];
        if (port_status & EHCI_PORT_CONNECTED) {
            terminal_print("Porta conectada detectada. Resetando...\n");
            // Limpa bits de mudança (Write 1 to Clear)
            ehci_regs->portsc[i] |= (EHCI_PORT_CHANGE_CONNECT | EHCI_PORT_CHANGE_ENABLE);
            ehci_reset_porta(i);
            
            return i; // RETORNA O ÍNDICE DA PORTA FÍSICA ENCONTRADA (0 para USB 1, 1 para USB 2)
        }
    }
    return -1; // Nenhum dispositivo encontrado
}

int ehci_reset_porta(uint8_t porta_idx) {
    uint32_t port_reg = ehci_regs->portsc[porta_idx];
    
    // Pulso de reset (50ms é ideal para VB e hardware real)
    ehci_regs->portsc[porta_idx] = (port_reg & ~EHCI_PORT_ENABLE) | EHCI_PORT_RESET;
    ehci_wait_ms(50);
    
    // Finaliza reset
    ehci_regs->portsc[porta_idx] &= ~EHCI_PORT_RESET;
    
    // Aguarda hardware finalizar a transição de reset
    int timeout = 10000;
    while (ehci_regs->portsc[porta_idx] & EHCI_PORT_RESET) {
        if (--timeout <= 0) break;
    }
    ehci_wait_ms(10);
    
    if (ehci_regs->portsc[porta_idx] & EHCI_PORT_ENABLE) {
        terminal_print("EHCI_CORE: Porta habilitada em High-Speed!\n");
        return 1;
    }
    
    terminal_print("EHCI_CORE: Aviso - Porta nao habilitou (pode nao ser High-Speed).\n");
    return 0; 
}

int ehci_init_async_list(void) {
    void* qh_phys_ptr = pmm_alloc_block();
    if (!qh_phys_ptr) return 0;
    
    uint64_t qh_phys_64 = (uint64_t)qh_phys_ptr;
    async_head_phys = (uint32_t)qh_phys_64;
    
    vmm_map_area(qh_phys_64, qh_phys_64, 4096, PAGE_PRESENT | PAGE_WRITE | PAGE_PCD | PAGE_PWT);
    async_head_virt = (ehci_qh_t*)(uintptr_t)qh_phys_64;
    memset((void*)async_head_virt, 0, sizeof(ehci_qh_t));
    
    // CORREÇÃO CRÍTICA: Bit 27 (Head of Reclamation) | Bit 14 (DTC) | Speed = High (2 << 12)
    async_head_virt->horizontal_link = async_head_phys | (1 << 1); // Aponta pra si mesmo, Type=QH
    async_head_virt->endpoint_char = (1 << 27) | (2 << 12) | (1 << 14);
    async_head_virt->overlay.next_qtd = 1; // Terminate (T=1)
    async_head_virt->overlay.alt_next_qtd = 1;
    async_head_virt->overlay.token = 0;
    
    ehci_regs->asynclistaddr = async_head_phys;
    ehci_regs->usbcmd |= (1 << 5); // Ativa Async Schedule
    
    int timeout = 1000000;
    while (!(ehci_regs->usbsts & (1 << 15))) {
        if (--timeout <= 0) return 0;
    }
    terminal_print("EHCI_CORE: Motor DMA ativo (Async Schedule ON)!\n");
    return 1;
}

int ehci_enviar_controle(uint8_t addr, usb_setup_packet_t* setup, void* data_buf, uint16_t data_len) {
    if (!setup || !async_head_virt) return 0;
    
    void* bloco_phys_ptr = pmm_alloc_block();
    if (!bloco_phys_ptr) return 0;
    
    uint64_t bloco_phys_64 = (uint64_t)bloco_phys_ptr;
    uint32_t bloco_phys = (uint32_t)bloco_phys_64;
    
    vmm_map_area(bloco_phys_64, bloco_phys_64, 4096, PAGE_PRESENT | PAGE_WRITE | PAGE_PCD | PAGE_PWT);
    void* bloco_virt = (void*)(uintptr_t)bloco_phys_64;
    memset(bloco_virt, 0, 4096);
    
    ehci_qh_t* qh = (ehci_qh_t*)bloco_virt;
    ehci_qtd_t* qtd_setup = (ehci_qtd_t*)(bloco_virt + 0x80);
    ehci_qtd_t* qtd_data = (ehci_qtd_t*)(bloco_virt + 0xA0);
    ehci_qtd_t* qtd_status = (ehci_qtd_t*)(bloco_virt + 0xC0);
    uint32_t buffer_phys = bloco_phys + 0x100;
    void* buffer_virt = bloco_virt + 0x100;
    
    // Configura QH (DTC=1, Maximum Packet Length=64, Endpoint=0, Address=addr)
    uint32_t ep_char = (addr & 0x7F) | (0 << 8) | (2 << 12) | (64 << 16) | (1 << 14);
    qh->horizontal_link = 1; // Terminate inicial
    qh->endpoint_char = ep_char;
    qh->endpoint_caps = (1 << 30); // Multiplier = 1
    qh->overlay.next_qtd = 1;
    qh->overlay.alt_next_qtd = 1;
    qh->overlay.token = 0;
    
    // 1. qTD SETUP (Sempre DATA0)
    memcpy(buffer_virt, setup, 8);
    qtd_setup->next_qtd = bloco_phys + 0xA0;
    qtd_setup->alt_next_qtd = 1;
    qtd_setup->buffer[0] = buffer_phys;
    qtd_setup->token = EHCI_T_DATA0 | (8 << 16) | (EHCI_PID_SETUP << 8) | (3 << 10) | EHCI_T_ACTIVE;
    
    ehci_qtd_t* last_qtd = qtd_setup;
    
    // 2. qTD DATA (Opcional, Sempre inicia com DATA1)
    if (data_len > 0) {
        uint32_t data_buffer_phys = buffer_phys + 64;
        void* data_buffer_virt = buffer_virt + 64;
        
        uint8_t direction = setup->bmRequestType & 0x80;
        uint8_t pid_data = (direction) ? EHCI_PID_IN : EHCI_PID_OUT;
        
        if (direction == 0 && data_buf) {
            memcpy(data_buffer_virt, data_buf, data_len); // OUT: copia do host para buffer DMA
        }
        
        qtd_data->next_qtd = bloco_phys + 0xC0;
        qtd_data->alt_next_qtd = 1;
        qtd_data->buffer[0] = data_buffer_phys;
        qtd_data->token = EHCI_T_DATA1 | (data_len << 16) | (pid_data << 8) | (3 << 10) | EHCI_T_ACTIVE;
        last_qtd = qtd_data;
    } else {
        qtd_setup->next_qtd = bloco_phys + 0xC0;
    }
    
    // 3. qTD STATUS (Sempre DATA1 e direção oposta ao DATA)
    uint8_t direction = (setup->bmRequestType & 0x80);
    uint8_t pid_status = (direction && data_len > 0) ? EHCI_PID_OUT : EHCI_PID_IN;
    if (data_len == 0) pid_status = EHCI_PID_IN; // Se não teve dados, status é sempre IN
    
    qtd_status->next_qtd = 1; // Termina a cadeia
    qtd_status->alt_next_qtd = 1;
    qtd_status->buffer[0] = buffer_phys;
    qtd_status->token = EHCI_T_DATA1 | EHCI_T_IOC | (0 << 16) | (pid_status << 8) | (3 << 10) | EHCI_T_ACTIVE;
    
    last_qtd->next_qtd = bloco_phys + 0xC0;
    qh->overlay.next_qtd = bloco_phys + 0x80;
    
    // Inserção Atômica na Lista Circular (Não desligue o usbcmd!)
    uint32_t original_link = async_head_virt->horizontal_link;
    qh->horizontal_link = original_link;
    async_head_virt->horizontal_link = bloco_phys | (1 << 1); // Aponta p/ o nosso QH, Type=QH
    
    // Aguarda conclusão do qTD Status
    int timeout = EHCI_TRANSFER_TIMEOUT;
    while (qtd_status->token & EHCI_T_ACTIVE) {
        ehci_wait_ms(1);
        if (--timeout <= 0) break;
    }
    
    // Removemos da lista
    async_head_virt->horizontal_link = original_link;
    
    uint32_t status_token = qtd_status->token;
    
    if (timeout <= 0) { terminal_print("EHCI_CORE: TIMEOUT!\n"); return 0; }
    if (status_token & EHCI_T_HALTED) { terminal_print("EHCI_CORE: STALL / ERRO\n"); return 0; }
    if (status_token & EHCI_T_XACTERR) { terminal_print("EHCI_CORE: XactErr\n"); return 0; }
    
    // Copia dados de volta se foi leitura (IN)
    if (data_len > 0 && data_buf && (setup->bmRequestType & 0x80)) {
        void* data_buffer_virt = buffer_virt + 64;
        memcpy(data_buf, data_buffer_virt, data_len - ((qtd_data->token >> 16) & 0x7FFF));
    }
    // --- COMPATIBILIZAÇÃO E LIBERAÇÃO DE MEMÓRIA ---
    // Mapeia a área com flags zeradas (remove o bit PAGE_PRESENT e invalida a página)
    vmm_map_area((uint64_t)bloco_virt, bloco_phys_64, 4096, 0); 
    
    // Libera o bloco no gerenciador de memória física
    pmm_free_block(bloco_phys_ptr);
    
    terminal_print("EHCI_CORE: Transferencia Concluida.\n");
    return 1;
}

int ehci_set_address(uint8_t old_addr, uint8_t new_addr) {
    usb_setup_packet_t setup;
    setup.bmRequestType = 0x00; // Host to Device, Standard, Device
    setup.bRequest = 0x05;      // SET_ADDRESS
    setup.wValue = new_addr;
    setup.wIndex = 0;
    setup.wLength = 0;
    
    return ehci_enviar_controle(old_addr, &setup, NULL, 0);
}

int ehci_ler_identificacao(void) {
    usb_setup_packet_t setup;
    setup.bmRequestType = 0x80; // Device to Host, Standard, Device
    setup.bRequest = 0x06;      // GET_DESCRIPTOR
    setup.wValue = 0x0100;      // Device Descriptor
    setup.wIndex = 0x0000;
    setup.wLength = 8;          // Lê os 8 primeiros bytes para descobrir Max Packet
    
    uint8_t desc_buf[18];
    memset(desc_buf, 0, 18);
    
    if (!ehci_enviar_controle(0, &setup, desc_buf, 8)) {
        terminal_print("EHCI_CORE: Falha na leitura inicial.\n");
        return 0;
    }
    
    if (desc_buf[0] == 18) {
        setup.wLength = 18;
        if (ehci_enviar_controle(0, &setup, desc_buf, 18)) {
            uint16_t vendor_id = (desc_buf[9] << 8) | desc_buf[8];
            uint16_t product_id = (desc_buf[11] << 8) | desc_buf[10];
            
            terminal_print("\n=== DEVICE ===\nVendor: ");
            hex_to_string(vendor_id, str_buf); terminal_print(str_buf);
            terminal_print("\nProduct: ");
            hex_to_string(product_id, str_buf); terminal_print(str_buf);
            terminal_print("\n==============\n");
            
            ehci_wait_ms(10); // Pausa recomendada após controle pesado
            ehci_set_address(0, 1);
            return 1;
        }
    }
    return 0;
}
