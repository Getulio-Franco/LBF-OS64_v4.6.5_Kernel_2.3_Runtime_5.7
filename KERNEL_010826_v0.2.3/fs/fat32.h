#ifndef FS_FAT32_H
#define FS_FAT32_H

#include <stdint.h>

/* --- CONSTANTES DE GERENCIAMENTO DE CLUSTERS FAT32 --- */
#define FAT32_FREE_CLUSTER     0x00000000
#define FAT32_RESERVED_CLUSTER 0x00000001
#define FAT32_BAD_CLUSTER      0x0FFFFFF7
#define FAT32_EOC              0x0FFFFFF8  /* End of Chain (Mínimo) */
#define FAT32_EOF              0x0FFFFFFF  /* End of File (Máximo) */
#define FAT32_CLUSTER_MASK     0x0FFFFFFF  /* Ignora os 4 bits superiores */

/**
 * @struct mbr_partition_t
 * @brief Estrutura de 16 bytes que mapeia uma entrada de partição na MBR (Setor 0).
 */
typedef struct __attribute__((packed)) {
    uint8_t  drive_status;     // 0x80 = Ativo/Bootável, 0x00 = Inativo
    uint8_t  chs_start[3];     // Endereço CHS inicial (Legado)
    uint8_t  partition_type;   // Tipo: 0x0C ou 0x0B indicam FAT32 LBA
    uint8_t  chs_end[3];       // Endereço CHS final (Legado)
    uint32_t lba_start;        // O SETOR ABSOLUTO ONDE A PARTIÇÃO COMEÇA NO DISCO
    uint32_t total_sectors;    // Número total de setores mapeados nesta partição
} mbr_partition_t;

/**
 * @struct fat32_directory_entry_t
 * @brief Estrutura de 32 bytes de uma entrada de diretório (Padrão 8.3).
 */
typedef struct __attribute__((packed)) {
    uint8_t  filename[11];         /* 8 bytes nome + 3 bytes extensão */
    uint8_t  attributes;           /* 0x10=Diretório, 0x20=Arquivo, 0x0F=LFN */
    uint8_t  reserved_nt;
    uint8_t  creation_time_ms;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_high;         /* Parte superior do endereço do cluster */
    uint16_t modification_time;
    uint16_t modification_date;
    uint16_t cluster_low;          /* Parte inferior do endereço do cluster */
    uint32_t file_size;            /* Tamanho real em bytes */
} fat32_directory_entry_t;

/**
 * @struct fat32_bpb_t
 * @brief BIOS Parameter Block para FAT32 (Setor de Boot).
 */
typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entries;         /* Sempre 0 no FAT32 */
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    /* Extensão FAT32 específica */
    uint32_t fat_size_32;
    uint16_t extended_flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     system_id[8];
} fat32_bpb_t;

/**
 * @struct fat32_volume_t
 * @brief Contexto isolado de cada unidade de disco ligada ao barramento do sistema.
 */
typedef struct {
    fat32_bpb_t bpb;
    uint32_t    fat_start;
    uint32_t    data_start;
    uint32_t    root_cluster;
    uint32_t    current_cluster;
    uint32_t    partition_offset;
    uint8_t     inicializado;
} fat32_volume_t;

/* --- VARIÁVEIS GLOBAIS (EXTERN) --- */
extern fat32_bpb_t disk_bpb;
extern uint32_t current_dir_cluster;
extern uint32_t fat_start_sector;
extern uint32_t data_start_sector;
extern uint32_t fat32_partition_offset; 
extern uint8_t  fat32_current_dev_id;

/* --- PROTÓTIPOS DE INTERFACE --- */
int  fat32_mount(uint8_t dev_id);
void fat32_salvar_contexto_disco(uint8_t dev_id);
void fat32_mudar_disco_ativo(uint8_t dev_id);

void     fat32_init(void);
uint32_t fat32_cluster_to_lba(uint32_t cluster);
uint32_t fat32_get_next_cluster(uint32_t current_cluster);
void     fat32_set_cluster_entry(uint32_t cluster, uint32_t value);
uint32_t fat32_find_free_cluster(void);
void     fat32_to_83_filename(const char* input, char* output);
void     fat32_format_name_for_display(char* dest, unsigned char* src);

void fat32_list_directory(uint32_t cluster);
int  fat32_change_directory(const char* name);
int  fat32_create_directory(const char* name);

int fat32_write_file(const char* name, uint8_t* buffer, uint32_t size);
int fat32_read_file(const char* name, uint8_t* buffer, uint32_t max_size);
int fat32_display_file(const char* name); 
int fat32_delete_file(const char* name);  
int fat32_append_file(const char* name, uint8_t* buffer, uint32_t size);

int fat32_find_entry(const char* name, fat32_directory_entry_t* out_entry);
int fat32_find_entry_ext(const char* name, fat32_directory_entry_t* out_entry, uint32_t* out_lba, uint32_t* out_offset);

#endif /* FS_FAT32_H */
