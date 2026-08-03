#include "storage.h"
#include "../video.h"
#include "../../util/string.h"
#include "../hw/ahci_hal.h"
#include "ehci_storage.h"

// Tabela global indexada contendo os dispositivos ativos no sistema
static block_device_t devices[STORAGE_MAX_DEVICES];

// Declarações externas das funções dos drivers de hardware (SINCRONIZADAS COM AHCI_HAL.H)
extern int ahci_hal_ler(uint8_t dev_id, uint64_t lba, uint32_t count, void* buffer);
extern int ahci_hal_escrever(uint8_t dev_id, uint64_t lba, uint32_t count, const void* buffer);
extern int ehci_storage_read_sectors(uint8_t addr, uint32_t lba, uint32_t count, void* buffer);

void storage_init(void) {
    for (int i = 0; i < STORAGE_MAX_DEVICES; i++) {
        devices[i].active = 0;
        devices[i].id = i;
        devices[i].name = "Vazio";
    }
    terminal_print("[STORAGE] Gerenciador unificado de discos online.\n");
}

int storage_register_device(uint8_t id, const char* name, uint32_t total_sectors, uint32_t block_size, storage_read_func_t read_f, storage_write_func_t write_f) {
    if (id >= STORAGE_MAX_DEVICES) return 0;
    
    char local_buf[32];
    
    devices[id].name = name;
    devices[id].total_sectors = total_sectors;
    devices[id].block_size = block_size;
    devices[id].read_sectors = read_f;
    devices[id].write_sectors = write_f;
    devices[id].active = 1;

    terminal_print("[STORAGE] Novo dispositivo registrado: [Disco ");
    int_to_string(id, local_buf); terminal_print(local_buf);
    terminal_print("] -> "); terminal_print(name);
    terminal_print("\n");
    
    return 1;
}

void storage_unregister_device(uint8_t id) {
    if (id < STORAGE_MAX_DEVICES) {
        char local_buf[32];
        
        devices[id].active = 0;
        devices[id].name = "Vazio";
        devices[id].read_sectors = NULL;
        devices[id].write_sectors = NULL;
        
        terminal_print("[STORAGE] Dispositivo ");
        int_to_string(id, local_buf); terminal_print(local_buf);
        terminal_print(" desconectado.\n");
    }
}

int storage_read_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, void* buffer) {
    // CORREÇÃO: Passando o dev_id (0) para o HAL do SATA
    if (dev_id == 0) {
        return (ahci_hal_ler(dev_id, lba, count, buffer) == 0) ? 1 : 0;
    }

    // Aceita o ID 1 ou ID 2 como rotas válidas para o driver USB/EHCI
    if (dev_id == 1 || dev_id == 2) {
        return ehci_storage_read_sectors(dev_id, lba, count, buffer);
    }

    return 0; 
}

int storage_write_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, const void* buffer) {
    // CORREÇÃO: Passando o dev_id (0) e removido cast desnecessário de ponteiro
    if (dev_id == 0) {
        // SATA: Inverte o retorno (0 vira 1 para Sucesso no FAT32)
        return (ahci_hal_escrever(dev_id, lba, count, buffer) == 0) ? 1 : 0;
    }
    
    if (dev_id == 1 || dev_id == 2) {
        // USB / EHCI: Deixamos o aviso até que você implemente a escrita do USB
        terminal_print("[STORAGE] Escrita via USB ainda nao implementada.\n");
        return 0; 
    }

    return 0; // ID inválido
}
