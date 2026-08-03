/**
 * ============================================================================
 * FAT32 LOGIC - CONSOLIDATED & NATIVE AHCI SYNC
 * ============================================================================
 * Descrição: Conversão de clusters, mapeamento de tabela FAT e busca via SATA.
 * ============================================================================
 */

#include "fs/fat32.h"
#include "fs/fat32_logic.h"
#include "drivers/hw/ahci_hal.h"  
#include "util/string.h"

extern fat32_bpb_t disk_bpb;
extern uint32_t fat_start_sector;
extern uint32_t data_start_sector;
extern uint32_t current_dir_cluster;
extern uint8_t fat32_current_dev_id;

/* CORREÇÃO: Alinhamento das declarações externas de IO com assinaturas do tipo void* */
extern int storage_read_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, void* buffer);
extern int storage_write_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, const void* buffer);

uint32_t fat32_cluster_to_lba(uint32_t cluster) {
    if (cluster < 2) cluster = disk_bpb.root_cluster;
    return data_start_sector + ((cluster - 2) * disk_bpb.sectors_per_cluster);
}

uint32_t fat32_get_next_cluster(uint32_t cluster) {
    __attribute__((aligned(16))) uint8_t fat_buffer[512];
    
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    if (storage_read_sectors(fat32_current_dev_id, fat_sector, 1, fat_buffer) == 0) {
        return 0x0FFFFFFF; 
    }

    uint32_t next_cluster = *(uint32_t*)&fat_buffer[ent_offset];
    return next_cluster & 0x0FFFFFFF; 
}

void fat32_set_cluster_entry(uint32_t cluster, uint32_t value) {
    __attribute__((aligned(16))) uint8_t sector_buffer[512];
    uint32_t fat_sector = fat_start_sector + (cluster / 128);
    uint32_t fat_index = cluster % 128;

    if (storage_read_sectors(fat32_current_dev_id, fat_sector, 1, sector_buffer) == 1) {
        ((uint32_t*)sector_buffer)[fat_index] = (value & 0x0FFFFFFF);
        storage_write_sectors(fat32_current_dev_id, fat_sector, 1, sector_buffer);
    }
}

void fat32_to_83_filename(const char* input, char* output) {
    int i = 0, j = 0;
    for (int k = 0; k < 11; k++) output[k] = ' ';
    
    if (input[0] == '.') {
        output[0] = '.';
        if (input[1] == '.') output[1] = '.';
        return;
    }

    while (input[i] != '.' && input[i] != '\0' && j < 8) {
        char c = input[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        output[j++] = c;
    }

    if (input[i] == '.') {
        i++; j = 8;
        while (input[i] != '\0' && j < 11) {
            char c = input[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            output[j++] = c;
        }
    }
}

void fat32_format_name_for_display(char* dest, unsigned char* src) {
    int p = 0;
    for (int i = 0; i < 8; i++) {
        if (src[i] != ' ') dest[p++] = src[i];
    }
    if (src[8] != ' ') {
        dest[p++] = '.';
        for (int i = 8; i < 11; i++) {
            if (src[i] != ' ') dest[p++] = src[i];
        }
    }
    dest[p] = '\0';
}

int fat32_find_entry(const char* name, fat32_directory_entry_t* out_entry) {
    __attribute__((aligned(16))) uint8_t sector_buffer[512];
    char fat_name[11];
    uint32_t cluster = current_dir_cluster;

    fat32_to_83_filename(name, fat_name);

    while (cluster < FAT32_EOC && cluster >= 2) {
        uint32_t lba = fat32_cluster_to_lba(cluster);
        for (uint32_t s = 0; s < disk_bpb.sectors_per_cluster; s++) {
            
            if (storage_read_sectors(fat32_current_dev_id, lba + s, 1, sector_buffer) == 0) 
                return -1;
            
            fat32_directory_entry_t* entries = (fat32_directory_entry_t*)sector_buffer;
            for (int i = 0; i < 16; i++) {
                if (entries[i].filename[0] == 0x00) return -1;
                if (entries[i].filename[0] == 0xE5) continue;
                if (memcmp(entries[i].filename, fat_name, 11) == 0) {
                    memcpy(out_entry, &entries[i], sizeof(fat32_directory_entry_t));
                    return 0;
                }
            }
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    return -1;
}

int fat32_find_entry_ext(const char* name, fat32_directory_entry_t* out_entry, uint32_t* out_lba, uint32_t* out_offset) {
    __attribute__((aligned(16))) uint8_t sector_buffer[512];
    char fat_name[11];
    uint32_t cluster = current_dir_cluster;

    fat32_to_83_filename(name, fat_name);

    while (cluster < FAT32_EOC && cluster >= 2) {
        uint32_t lba = fat32_cluster_to_lba(cluster);
        for (uint32_t s = 0; s < disk_bpb.sectors_per_cluster; s++) {
            
            if (storage_read_sectors(fat32_current_dev_id, lba + s, 1, sector_buffer) == 0) 
                return -1;
            
            fat32_directory_entry_t* entries = (fat32_directory_entry_t*)sector_buffer;
            for (int i = 0; i < 16; i++) {
                if (entries[i].filename[0] == 0x00) return -1;
                if (entries[i].filename[0] == 0xE5) continue;
                
                if (memcmp(entries[i].filename, fat_name, 11) == 0) {
                    memcpy(out_entry, &entries[i], sizeof(fat32_directory_entry_t));
                    
                    if (out_lba)     *out_lba = lba + s;                   
                    if (out_offset)  *out_offset = i * sizeof(fat32_directory_entry_t); 
                    
                    return 0;
                }
            }
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    return -1;
}

uint32_t fat32_find_free_cluster(void) {
    __attribute__((aligned(16))) uint8_t sector_buffer[512];
    uint32_t fat_size = (disk_bpb.fat_size_16 != 0) ? disk_bpb.fat_size_16 : disk_bpb.fat_size_32;

    for (uint32_t s = 0; s < fat_size; s++) {
        if (storage_read_sectors(fat32_current_dev_id, fat_start_sector + s, 1, sector_buffer) == 0) 
            return 0;
        uint32_t* table = (uint32_t*)sector_buffer;
        for (int i = 0; i < 128; i++) {
            if (s == 0 && i < 2) continue; 
            if ((table[i] & 0x0FFFFFFF) == 0x00000000) {
                return (s * 128) + i;
            }
        }
    }
    return 0;
}
