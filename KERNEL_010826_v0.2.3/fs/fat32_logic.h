/**
 * ============================================================================
 * FAT32 LOGIC - HEADER (V3.2 - SYNC)
 * ============================================================================
 * Descrição: Protótipos para conversão de endereços e manipulação da FAT.
 * ============================================================================
 */

#ifndef FS_FAT32_LOGIC_H
#define FS_FAT32_LOGIC_H

#include <stdint.h>
#include "fs/fat32.h"

/* --- VARIÁVEIS GLOBAIS (EXTERN) --- */
extern fat32_bpb_t disk_bpb;
extern uint32_t fat_start_sector;
extern uint32_t data_start_sector;
extern uint32_t current_dir_cluster;

/* --- FUNÇÕES DE ENDEREÇAMENTO --- */
uint32_t fat32_cluster_to_lba(uint32_t cluster);

/* --- FUNÇÕES DE MANIPULAÇÃO DA TABELA FAT --- */
uint32_t fat32_get_next_cluster(uint32_t current_cluster);
void     fat32_set_cluster_entry(uint32_t cluster, uint32_t value);
uint32_t fat32_find_free_cluster(void);

/* --- FUNÇÕES DE BUSCA E FORMATAÇÃO --- */
int fat32_find_entry(const char* name, fat32_directory_entry_t* out_entry);
int fat32_find_entry_ext(const char* name, fat32_directory_entry_t* out_entry, uint32_t* out_lba, uint32_t* out_offset);
void fat32_to_83_filename(const char* input, char* output);
void fat32_format_name_for_display(char* dest, unsigned char* src);

#endif /* FS_FAT32_LOGIC_H */
