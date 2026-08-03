#ifndef DISK_H
#define DISK_H

#include <stdint.h>

#define MAX_DISKS 10

typedef enum {
    DISK_TYPE_SATA,
    DISK_TYPE_IDE,
    DISK_TYPE_NVME,
    DISK_TYPE_USB
} disk_type_t;

typedef struct disk {
    char name[32];
    disk_type_t type;
    void* priv_data;
    uint32_t size_sectors; 
    
    int (*read)(struct disk* d, uint64_t sector, uint32_t count, void* buffer);
    int (*write)(struct disk* d, uint64_t sector, uint32_t count, void* buffer);
} disk_t;

extern disk_t system_disks[MAX_DISKS];
extern int disk_count;
// Exporta a variável e a função para o resto do Kernel
extern int current_disk_target;

void disk_register(disk_t d);
int disk_read(int disk_id, uint64_t sector, uint32_t count, void* buffer);
int disk_write(int disk_id, uint64_t sector, uint32_t count, void* buffer);
void set_current_disk_target(int disk_id);

int ide_read_sector(uint32_t sector, uint8_t *buffer);
int ide_write_sector(uint32_t sector, const uint8_t *buffer);

#endif
