#include "fs/fat32.h"
#include "drivers/video.h"
#include "util/string.h"
#include "../mem/heap.h"

#include "drivers/pd/ehci_pci.h" 
#include "drivers/pd/ehci_core.h" 
#include "drivers/pd/storage.h"
#include "drivers/pd/ehci_storage.h"

/* --- ABSTRAÇÕES DE BAIXO NÍVEL / MEMÓRIA --- */
extern int storage_read_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, void* buffer);
extern int storage_write_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, const void* buffer);
extern void* pmm_alloc_block(void);
extern void  pmm_free_block(void* block);

// Declarações das funções do EHCI
extern int ehci_pci_init(void);
extern int ehci_core_init(void);
extern int ehci_detectar_dispositivos(void);
extern int ehci_init_async_list(void);
extern int ehci_ler_identificacao(void);
extern int ehci_storage_init(uint8_t addr, uint32_t* out_max_lba, uint32_t* out_block_size);
extern int ehci_storage_read_sectors(uint8_t addr, uint32_t lba, uint32_t count, void* buffer);
extern int ehci_storage_write_sectors(uint8_t addr, uint32_t lba, uint32_t count, const void* buffer);

/* --- VARIÁVEIS GLOBAIS DA GEOMETRIA ATIVA --- */
fat32_bpb_t disk_bpb;
uint32_t current_dir_cluster;
uint32_t fat_start_sector;
uint32_t data_start_sector;
uint32_t fat32_partition_offset = 0; 

/* --- CONTROLE DE CONTEXTO E UNIDADES --- */
static volatile int fat_lock = 0;
uint8_t fat32_current_dev_id = 0;

// Suporte para Unidade 0 (SATA / C:), Unidade 1 (USB / D:), Unidade 2 (USB / E:)
static fat32_volume_t unidades_sistema[3] = {0};

// FLAG CRUCIAL: Controla se o silício do controlador USB já foi acordado alguma vez
static bool usb_hardware_inicializado = false;

static void fat32_log(const char* msg) {
    terminal_print("[FAT32] ");
    terminal_print(msg);
    terminal_print("\n");
}

void fat32_salvar_contexto_disco(uint8_t dev_id) {
    if (dev_id >= 3) return; 

    unidades_sistema[dev_id].bpb              = disk_bpb;
    unidades_sistema[dev_id].fat_start        = fat_start_sector;
    unidades_sistema[dev_id].data_start       = data_start_sector;
    unidades_sistema[dev_id].root_cluster     = disk_bpb.root_cluster;
    unidades_sistema[dev_id].partition_offset = fat32_partition_offset;

    if (unidades_sistema[dev_id].inicializado == 0) {
        unidades_sistema[dev_id].current_cluster = disk_bpb.root_cluster;
    } else {
        unidades_sistema[dev_id].current_cluster = current_dir_cluster;
    }

    unidades_sistema[dev_id].inicializado = 1;
}

void fat32_mudar_disco_ativo(uint8_t dev_id) {
    if (dev_id >= 3) {
        fat32_log("ERRO: ID de dispositivo invalido.");
        return;
    }

    // --- MÁGICA DO AUTO-INIT PARA USB (DISCO 1 ou 2) ---
    if (dev_id == 1 || dev_id == 2) {
        if (unidades_sistema[dev_id].inicializado == 0) {
            
            // PASSO 1: O hardware físico (PCI/Core/DMA) só inicializa UMA VEZ para todo o sistema
            if (!usb_hardware_inicializado) {
                terminal_print("\n[USB] Primeira unidade detectada. Inicializando barramento fisico...\n");
                
                if (!ehci_pci_init() || !ehci_core_init()) {
                    terminal_print("USB: [FALHA] Erro na inicializacao global do EHCI.\n");
                    return;
                }
                if (!ehci_init_async_list()) {
                    terminal_print("USB: [FALHA] Erro ao ativar o motor de DMA.\n");
                    return;
                }
                
                int porta_usb_fisica = ehci_detectar_dispositivos();
                if (porta_usb_fisica == -1) {
                    terminal_print("USB: [AVISO] Nenhum pendrive detectado nas portas.\n");
                    return;
                }

                ehci_ler_identificacao();
                
                // Hardware inicializado com sucesso, muda a flag global!
                usb_hardware_inicializado = true;
            } else {
                // Se cair aqui (ex: alternando para E:), o hardware já está pronto!
                terminal_print("\n[USB] Controladora ja ativa. Mapeando nova unidade lógica...\n");
            }

            // PASSO 2: Inicializa os dados lógicos específicos do dispositivo (SCSI/Storage)
            uint32_t usb_max_lba = 0;
            uint32_t usb_block_size = 512;

            // Envia o comando usando 1 como endereço físico padrão do barramento USB
            if (!ehci_storage_init(1, &usb_max_lba, &usb_block_size)) {
                terminal_print("USB STORAGE: [FALHA] O pendrive nao respondeu aos comandos SCSI.\n");
                return; 
            }

            storage_register_device(
                dev_id, 
                "USB Flash Drive", 
                usb_max_lba,   
                usb_block_size, 
                (storage_read_func_t)ehci_storage_read_sectors, 
                (storage_write_func_t)ehci_storage_write_sectors
            );

            if (!fat32_mount(dev_id)) {
                terminal_print("[FAT32] Falha ao montar volume do Pendrive.\n");
                return;
            }
            
            terminal_print("[USB] Unidade configurada e montada automaticamente!\n");
        }
    }

    if (unidades_sistema[dev_id].inicializado == 0) {
        fat32_log("ERRO: Unidade de armazenamento nao inicializada/disponivel.");
        return;
    }

    while (__sync_lock_test_and_set(&fat_lock, 1));

    // Salva onde o usuário estava navegando no disco atual antes da troca
    unidades_sistema[fat32_current_dev_id].current_cluster = current_dir_cluster;

    fat32_current_dev_id = dev_id;

    // Restaura o contexto geométrico do disco destino
    disk_bpb               = unidades_sistema[dev_id].bpb;
    fat_start_sector       = unidades_sistema[dev_id].fat_start;
    data_start_sector      = unidades_sistema[dev_id].data_start;
    current_dir_cluster    = unidades_sistema[dev_id].current_cluster;
    fat32_partition_offset = unidades_sistema[dev_id].partition_offset;

    __sync_lock_release(&fat_lock);
}

int fat32_mount(uint8_t dev_id) {
    if (dev_id >= 3) return 0;

    fat32_current_dev_id = dev_id;
    
    uint8_t *sector_buffer = (uint8_t*)(uintptr_t)pmm_alloc_block();
    if (sector_buffer == 0) {
        fat32_log("ERRO: Falha ao alocar bloco fisico para o buffer FAT32.");
        return 0;
    }
    
    while (__sync_lock_test_and_set(&fat_lock, 1));

    if (storage_read_sectors(dev_id, 0, 1, sector_buffer) == 0) {
        fat32_log("ERRO: Falha ao ler a MBR do dispositivo.");
        __sync_lock_release(&fat_lock);
        pmm_free_block((void*)(uintptr_t)sector_buffer);
        return 0;
    }

    if (sector_buffer[0] == 0x00 && sector_buffer[510] == 0x00) {
        fat32_log("ERRO: Setor MBR totalmente vazio.");
        __sync_lock_release(&fat_lock);
        pmm_free_block((void*)(uintptr_t)sector_buffer);
        return 0;
    }

    mbr_partition_t* partition1 = (mbr_partition_t*)&sector_buffer[446];
    fat32_partition_offset = partition1->lba_start;

    if (fat32_partition_offset == 0xFFFFFFFF || fat32_partition_offset > 120000000) {
        fat32_log("AVISO: MBR Invalida/Incompativel. Usando fallback Setor 2048...");
        fat32_partition_offset = 2048; 
    } else if (fat32_partition_offset == 0) {
        fat32_log("AVISO: Particao inicia no setor 0? Usando fallback Setor 2048...");
        fat32_partition_offset = 2048; 
    }

    if (storage_read_sectors(dev_id, fat32_partition_offset, 1, sector_buffer) == 0) {
        fat32_log("ERRO: O dispositivo de armazenamento nao respondeu no VBR.");
        __sync_lock_release(&fat_lock);
        pmm_free_block((void*)(uintptr_t)sector_buffer);
        return 0;
    }

    memcpy(&disk_bpb, sector_buffer, sizeof(fat32_bpb_t));

    if (sector_buffer[510] != 0x55 || sector_buffer[511] != 0xAA) {
        fat32_log("ERRO: Assinatura FAT32 invalida no setor de boot.");
        __sync_lock_release(&fat_lock);
        pmm_free_block((void*)(uintptr_t)sector_buffer);
        return 0;
    }

    uint32_t fat_size = (disk_bpb.fat_size_16 != 0) ? disk_bpb.fat_size_16 : disk_bpb.fat_size_32;

    fat_start_sector    = fat32_partition_offset + disk_bpb.reserved_sectors;
    data_start_sector   = fat_start_sector + (disk_bpb.fat_count * fat_size);
    current_dir_cluster = disk_bpb.root_cluster;

    __sync_lock_release(&fat_lock);
    pmm_free_block((void*)(uintptr_t)sector_buffer); 
    
    fat32_salvar_contexto_disco(dev_id);

    if (dev_id == 0) {
        fat32_log("Montado com sucesso via SATA (Disco 0 -> C:)!");
    } else if (dev_id == 1) {
        fat32_log("Montado com sucesso via USB (Disco 1 -> D:)!");
    } else if (dev_id == 2) {
        fat32_log("Montado com sucesso via USB (Disco 2 -> E:)!");
    }

    return 1;
}
