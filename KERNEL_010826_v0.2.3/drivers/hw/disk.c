#include "disk.h"
#include "../video.h"       // Ajuste o caminho conforme sua pasta
#include "../../util/string.h" // Ajuste o caminho

disk_t system_disks[MAX_DISKS];
int disk_count = 0;
// Variável global que o FAT32 usará para saber de qual disco ler/gravar
int current_disk_target = 0;

void disk_register(disk_t d) {
    if (disk_count >= MAX_DISKS) {
        terminal_print("HAL: Erro - Limite de discos atingido!\n");
        return;
    }

    system_disks[disk_count] = d;

    // Log dinâmico usando o Terminal VESA com Scroll
    terminal_print("HAL: Disco registrado -> ");
    terminal_print(d.name);
    
    if (d.type == DISK_TYPE_SATA) {
        terminal_print(" [SATA/AHCI]\n");
    } else if (d.type == DISK_TYPE_IDE) {
        terminal_print(" [PATA/IDE]\n");
    } else if (d.type == DISK_TYPE_NVME) {
        terminal_print(" [NVMe/PCIe]\n");
    } else if (d.type == DISK_TYPE_USB) {
        terminal_print(" [USB/MSD]\n");
    } else {
        terminal_print(" [Desconhecido]\n");
    }
    
    disk_count++;
}

int disk_read(int disk_id, uint64_t sector, uint32_t count, void* buffer) {
    if (disk_id < 0 || disk_id >= disk_count) return -1;
    if (buffer == 0) return -1;
    if (!system_disks[disk_id].read) return -1;

    return system_disks[disk_id].read(&system_disks[disk_id], sector, count, buffer);
}

int disk_write(int disk_id, uint64_t sector, uint32_t count, void* buffer) {
    if (disk_id < 0 || disk_id >= disk_count) return -1;
    if (buffer == 0) return -1;
    if (!system_disks[disk_id].write) {
        terminal_print("HAL: Erro - Escrita nao suportada neste disco.\n");
        return -1; 
    }

    return system_disks[disk_id].write(&system_disks[disk_id], sector, count, buffer);
}

void set_current_disk_target(int disk_id) {
    // Garante que o ID do disco seja válido e já esteja registrado no HAL
    if (disk_id >= 0 && disk_id < disk_count) {
        current_disk_target = disk_id;
    }
}

// ============================================================================
// CAMADA DE COMPATIBILIDADE LEGADA PARA O FAT32 (ANTI-IDE)
// ============================================================================

/**
 * @brief Substitui o antigo driver IDE. Redireciona a leitura para a HAL SATA.
 * @return 0 em caso de sucesso (o que o FAT32 espera), -1 em erro.
 */
int ide_read_sector(uint32_t sector, uint8_t *buffer) {
    // Chama a HAL universal usando o disco alvo configurado (current_disk_target)
    // O ahci_read retorna 1 para sucesso, convertemos para 0 para o FAT32
    if (disk_read(current_disk_target, (uint64_t)sector, 1, (void*)buffer) == 1) {
        return 0; 
    }
    return -1;
}

/**
 * @brief Substitui o antigo driver IDE. Redireciona a escrita para a HAL SATA.
 * @return 0 em caso de sucesso, -1 em erro.
 */
int ide_write_sector(uint32_t sector, const uint8_t *buffer) {
    // Redireciona para o disk_write da HAL
    if (disk_write(current_disk_target, (uint64_t)sector, 1, (void*)buffer) == 1) {
        return 0;
    }
    return -1;
}
