#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

#define STORAGE_MAX_DEVICES   4

#define STORAGE_DEV_SATA      0
#define STORAGE_DEV_USB       1

// Assinatura das funções de driver que o storage aceita
typedef int (*storage_read_func_t)(uint32_t lba, uint32_t count, void* buffer);
typedef int (*storage_write_func_t)(uint32_t lba, uint32_t count, const void* buffer);

// Estrutura que descreve um dispositivo de armazenamento no Kernel
typedef struct {
    uint8_t  id;                // ID do dispositivo (0, 1...)
    uint8_t  active;            // 1 se estiver conectado, 0 se vazio
    uint32_t total_sectors;     // Capacidade total em setores (LBA)
    uint32_t block_size;        // Tamanho do setor (geralmente 512 bytes)
    const char* name;           // Nome descritivo (Ex: "SATA HDD", "USB Flash")
    
    // Ponteiros de função para os drivers de hardware reais
    storage_read_func_t  read_sectors;
    storage_write_func_t write_sectors;
} block_device_t;

// Funções públicas do gerenciador de armazenamento
void storage_init(void);
int  storage_register_device(uint8_t id, const char* name, uint32_t total_sectors, uint32_t block_size, storage_read_func_t read_f, storage_write_func_t write_f);
void storage_unregister_device(uint8_t id);

int  storage_read_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, void* buffer);
int  storage_write_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, const void* buffer);

#endif // STORAGE_H
