/**
 * ============================================================================
 * FAT32 DIRECTORY MANAGEMENT - V3.4 (CLEAN & SAFE)
 * ============================================================================
 * Descrição: Operações de diretório usando a abstração estável do HAL AHCI.
 * Removido: Todas as dependências gráficas (VESA/UI) para evitar travamentos.
 * Adicionado: Proteção de Pilha via kmalloc em todas as rotinas.
 * ============================================================================
 */

#include "fs/fat32.h"
#include "fs/fat32_logic.h" 
#include "drivers/hw/ahci_hal.h"  
#include "drivers/hw/ahci_cmd.h"  
#include "util/string.h"
#include "mem/heap.h"

extern uint32_t current_dir_cluster;
extern fat32_bpb_t disk_bpb;
extern uint8_t fat32_current_dev_id;

extern int storage_read_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, void* buffer);
extern int storage_write_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, const void* buffer);

/**
 * @brief Escaneia o diretório (Gráficos removidos por segurança arquitetural).
 * No futuro, esta função deve preencher um array de structs em vez de desenhar.
 */
void fat32_list_directory(uint32_t cluster) {
    uint8_t* sector_buffer = (uint8_t*)kmalloc(512);
    if (!sector_buffer) return; // Erro silencioso e seguro

    while (cluster < FAT32_EOC && cluster >= 2) {
        uint32_t base_lba = fat32_cluster_to_lba(cluster);
        
        for (uint32_t sector = 0; sector < disk_bpb.sectors_per_cluster; sector++) {
            if (storage_read_sectors(fat32_current_dev_id, base_lba + sector, 1, sector_buffer) == 0) {
                kfree(sector_buffer); 
                return;
            }

            fat32_directory_entry_t* entries = (fat32_directory_entry_t*)sector_buffer;

            for (int i = 0; i < 16; i++) {
                if (entries[i].filename[0] == 0x00) {
                    kfree(sector_buffer); 
                    return;
                }
            }
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    
    kfree(sector_buffer);
}

/**
 * @brief Cria um novo diretório no cluster atual.
 */
int fat32_create_directory(const char* dir_name) {
    uint8_t* sector_buffer = (uint8_t*)kmalloc(512);
    if (!sector_buffer) return -1;

    char fat_name[11];
    fat32_to_83_filename(dir_name, fat_name);

    fat32_directory_entry_t dummy;
    if (fat32_find_entry(dir_name, &dummy) == 0) {
        kfree(sector_buffer);
        return -1; // Nome já existe
    }

    uint32_t new_cluster = fat32_find_free_cluster();
    if (new_cluster == 0) {
        kfree(sector_buffer);
        return -1; // Disco cheio
    }

    fat32_set_cluster_entry(new_cluster, 0x0FFFFFFF);

    uint32_t parent_cluster = current_dir_cluster;
    int found_slot = 0;

    while (parent_cluster < FAT32_EOC && !found_slot) {
        uint32_t lba = fat32_cluster_to_lba(parent_cluster);
        for (uint32_t s = 0; s < disk_bpb.sectors_per_cluster; s++) {
            
            storage_read_sectors(fat32_current_dev_id, lba + s, 1, sector_buffer);
            fat32_directory_entry_t* entries = (fat32_directory_entry_t*)sector_buffer;
            
            for (int i = 0; i < 16; i++) {
                if (entries[i].filename[0] == 0x00 || entries[i].filename[0] == 0xE5) {
                    memcpy(entries[i].filename, fat_name, 11);
                    entries[i].attributes = 0x10; 
                    entries[i].cluster_low = (uint16_t)(new_cluster & 0xFFFF);
                    entries[i].cluster_high = (uint16_t)((new_cluster >> 16) & 0xFFFF);
                    entries[i].file_size = 0;
                    
                    storage_write_sectors(fat32_current_dev_id, lba + s, 1, sector_buffer);
                    found_slot = 1;
                    break;
                }
            }
            if (found_slot) break;
        }
        if (!found_slot) parent_cluster = fat32_get_next_cluster(parent_cluster);
    }

    uint32_t new_lba = fat32_cluster_to_lba(new_cluster);
    
    for (uint32_t s = 0; s < disk_bpb.sectors_per_cluster; s++) {
        memset(sector_buffer, 0, 512);
        
        if (s == 0) {
            fat32_directory_entry_t* dot_entries = (fat32_directory_entry_t*)sector_buffer;

            memcpy(dot_entries[0].filename, ".          ", 11);
            dot_entries[0].attributes = 0x10;
            dot_entries[0].cluster_low = (uint16_t)(new_cluster & 0xFFFF);
            dot_entries[0].cluster_high = (uint16_t)((new_cluster >> 16) & 0xFFFF);

            memcpy(dot_entries[1].filename, "..         ", 11);
            dot_entries[1].attributes = 0x10;
            uint32_t parent_val = (current_dir_cluster == disk_bpb.root_cluster) ? 0 : current_dir_cluster;
            dot_entries[1].cluster_low = (uint16_t)(parent_val & 0xFFFF);
            dot_entries[1].cluster_high = (uint16_t)((parent_val >> 16) & 0xFFFF);
        }
        
        storage_write_sectors(fat32_current_dev_id, new_lba + s, 1, sector_buffer);
    }

    kfree(sector_buffer);
    return 0;
}

/**
 * @brief Altera o diretório (CD).
 */
int fat32_change_directory(const char* folder_name) {
    fat32_directory_entry_t entry;
    
    if (fat32_find_entry(folder_name, &entry) == 0) {
        if (entry.attributes & 0x10) {
            uint32_t target = ((uint32_t)entry.cluster_high << 16) | entry.cluster_low;
            current_dir_cluster = (target == 0) ? disk_bpb.root_cluster : target;
            return 0;
        } 
    } 
    
    return -1; // Diretório não encontrado ou não é diretório
}

/**
 * @brief Busca metadados de uma entrada específica pelo seu índice no diretório atual.
 */
int fat32_get_entry_by_index(uint32_t cluster, int target_index, char* name_out, uint32_t* size_out, uint8_t* attr_out) {
    uint8_t* sector_buffer = (uint8_t*)kmalloc(512);
    if (!sector_buffer) return -1;

    char formatted_name[13]; 
    int current_valid_index = 0;

    while (cluster < FAT32_EOC && cluster >= 2) {
        uint32_t base_lba = fat32_cluster_to_lba(cluster);
        
        for (uint32_t sector = 0; sector < disk_bpb.sectors_per_cluster; sector++) {
            if (storage_read_sectors(fat32_current_dev_id, base_lba + sector, 1, sector_buffer) == 0) {
                kfree(sector_buffer);
                return -1; 
            }

            fat32_directory_entry_t* entries = (fat32_directory_entry_t*)sector_buffer;

            for (int i = 0; i < 16; i++) {
                if (entries[i].filename[0] == 0x00) {
                    kfree(sector_buffer);
                    return 0;   
                }
                if (entries[i].filename[0] == 0xE5) continue; 
                if (entries[i].attributes == 0x0F) continue;  

                if (current_valid_index == target_index) {
                    fat32_format_name_for_display(formatted_name, entries[i].filename);
                    
                    strcpy(name_out, formatted_name);
                    *size_out = entries[i].file_size;
                    *attr_out = entries[i].attributes;
                    
                    kfree(sector_buffer);
                    return 1; 
                }

                current_valid_index++;
            }
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    
    kfree(sector_buffer);
    return 0;
}
