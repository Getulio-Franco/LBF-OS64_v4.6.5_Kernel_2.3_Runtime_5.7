#include "fs/fat32_file.h" 
#include "fs/fat32.h"
#include "fs/fat32_logic.h"
#include "drivers/hw/ahci_hal.h"  
#include "drivers/pd/storage.h"
#include "drivers/video.h"
#include "util/string.h"
#include "mem/heap.h"
#include "util/sync.h" // Adicionado o cabeçalho do Spinlock

extern uint32_t current_dir_cluster;
extern fat32_bpb_t disk_bpb;
extern uint8_t fat32_current_dev_id;

// 🛡️ TRAVA GLOBAL DO SISTEMA DE ARQUIVOS
spinlock_t fat32_lock = SPINLOCK_UNLOCKED;

/**
 * @brief Lê e exibe o conteúdo de um arquivo no console VESA.
 */
int fat32_display_file(const char* target_name) {
    spin_lock(&fat32_lock); // ADQUIRE A TRAVA

    fat32_directory_entry_t entry;
    
    if (fat32_find_entry(target_name, &entry) != 0) {
        spin_unlock(&fat32_lock);
        return FS_NOT_FOUND;
    }

    if (entry.file_size == 0) {
        spin_unlock(&fat32_lock);
        return FS_SUCCESS;
    }

    uint8_t* file_data = (uint8_t*)kmalloc(entry.file_size + 1);
    if (!file_data) {
        spin_unlock(&fat32_lock);
        return FS_ERROR_READ;
    }

    // Nota: fat32_read_file chama fat32_read_file_at_offset, 
    // que também tenta pegar a trava. Para evitar deadlock (se o spinlock não for reentrante),
    // devemos chamar a lógica direta ou liberar a trava antes. 
    // Como fat32_read_file_at_offset já tem trava, vamos soltar temporariamente.
    spin_unlock(&fat32_lock); 

    if (fat32_read_file(target_name, file_data, entry.file_size) == FS_SUCCESS) {
        file_data[entry.file_size] = '\0'; 

        uint32_t cur_x = 20; 
        uint32_t cur_y = 150; 
        uint32_t color = 0x00FFFFFF; // Branco

        for (uint32_t i = 0; i < entry.file_size; i++) {
            char c = (char)file_data[i];
            
            if (c == '\n') {
                cur_x = 20;
                cur_y += 16; 
            } else if (c >= 32 && c <= 126) {
                draw_char(cur_x, cur_y, c, color, 1);
                cur_x += 8;
            }

            if (cur_x > 1000) { cur_x = 20; cur_y += 16; }
            if (cur_y > 740) break;
        }
    }

    kfree(file_data);
    return FS_SUCCESS;
}

int fat32_read_file_at_offset(const char* name, uint8_t* buffer, uint32_t size, uint32_t offset) {
    // SEM TRAVAS AQUI NO TOPO! Deixe o I/O fluir livremente.
    if (!buffer || size == 0) {
        return -1;
    }
    
    // VERIFICAÇÃO DE SEGURANÇA: Evita crash se o ponteiro do nome corrompeu
    if (!name || (uintptr_t)name < 0x1000) { 
        vga_print_string("\n[FAT32 v9_v4] ERRO: Ponteiro de nome corrompido ou invalido!", 0, 39);
        return -1;
    }

   // vga_print_string("\n[FAT32 v9_v4] Lendo arquivo: ", 0, 39);
   // vga_print_string(name, 30, 39);

    fat32_directory_entry_t entry;
    // O I/O do disco acontece aqui sem prender a CPU
    if (fat32_find_entry(name, &entry) != 0) {
        vga_print_string("\n[FAT32 v9_v4] ERRO: Arquivo nao encontrado!", 0, 39);
        return FS_NOT_FOUND;
    }

    if (offset >= entry.file_size) {
        return -1;
    }
    
    if (offset + size > entry.file_size) size = entry.file_size - offset;

    uint32_t bytes_per_cluster = disk_bpb.sectors_per_cluster * 512;
    uint32_t current_c = (((uint32_t)entry.cluster_high << 16) | entry.cluster_low) & 0x0FFFFFFF;

    // Avança clusters até alcançar o offset desejado
    uint32_t skip = offset / bytes_per_cluster;
    for (uint32_t j = 0; j < skip; j++) {
        current_c = fat32_get_next_cluster(current_c) & 0x0FFFFFFF;
        if (current_c >= FAT32_EOC || current_c < 2) {
            return -1;
        }
    }

    uint32_t internal_offset = offset % bytes_per_cluster;
    uint32_t total_read = 0;
    
    uint8_t* temp_c = (uint8_t*)kmalloc(bytes_per_cluster);
    if (!temp_c) {
        vga_print_string("\n[FAT32 v9_v4] ERRO: Falha de memoria temp_c!", 0, 39);
        return FS_ERROR_READ;
    }

    while (current_c >= 2 && current_c < FAT32_EOC && total_read < size) {
        uint32_t lba = fat32_cluster_to_lba(current_c);
        
        // I/O OCORRENDO AQUI (Sem travar as Interrupções!)
        if (storage_read_sectors(fat32_current_dev_id, lba, disk_bpb.sectors_per_cluster, temp_c) == 0) {
            vga_print_string("\n[FAT32 v9_v4] ERRO: Falha em storage_read_sectors!", 0, 39);
            kfree(temp_c);
            return FS_ERROR_READ;
        }

        uint32_t avail = bytes_per_cluster - internal_offset;
        uint32_t to_copy = (size - total_read > avail) ? avail : (size - total_read);

        // AQUI SIM! Protegemos apenas a cópia na RAM (evitando que outro processo bagunce o buffer)
        spin_lock(&fat32_lock);
        memcpy(buffer + total_read, temp_c + internal_offset, to_copy);
        spin_unlock(&fat32_lock);

        total_read += to_copy;
        internal_offset = 0; 

        uint32_t next_c = fat32_get_next_cluster(current_c) & 0x0FFFFFFF;
        if (next_c == current_c) break; 
        current_c = next_c;
    }

    kfree(temp_c);
    //vga_print_string("\n[FAT32 v9_v4] Leitura concluida com sucesso.", 0, 39);
    return FS_SUCCESS;
}
 
int fat32_read_file(const char* name, uint8_t* buffer, uint32_t max_size) {
    // Como fat32_read_file_at_offset gerencia a própria trava, não a chamamos aqui
    return fat32_read_file_at_offset(name, buffer, max_size, 0);
}

/**
 * @brief Grava um arquivo (Suporta caminhos simples como subpasta/arquivo.ext e sobrescreve se existir).
 */
int fat32_write_file(const char* name, uint8_t* input_buffer, uint32_t size) {
    spin_lock(&fat32_lock); // ADQUIRE A TRAVA

    uint32_t dir_c = current_dir_cluster & 0x0FFFFFFF;
    const char* file_name_part = name;

    // 1. PATH PARSER: Separar caminho do arquivo
    int last_slash = -1;
    for (int i = 0; name[i] != '\0'; i++) {
        if (name[i] == '/') last_slash = i;
    }

    if (last_slash != -1) {
        char dir_name[128];
        if (last_slash < 127) {
            memcpy(dir_name, name, last_slash);
            dir_name[last_slash] = '\0';
            
            file_name_part = name + last_slash + 1;

            fat32_directory_entry_t dir_entry;
            if (fat32_find_entry(dir_name, &dir_entry) == 0) {
                if (dir_entry.attributes & 0x10) {
                    dir_c = (((uint32_t)dir_entry.cluster_high << 16) | dir_entry.cluster_low) & 0x0FFFFFFF;
                    if (dir_c == 0) dir_c = 2;
                } else {
                    spin_unlock(&fat32_lock);
                    return FS_NOT_FOUND;
                }
            } else {
                spin_unlock(&fat32_lock);
                return FS_NOT_FOUND;
            }
        }
    }

    // 2. Transforma o nome do arquivo em formato 8.3
    char fat_name[11];
    fat32_to_83_filename(file_name_part, fat_name);

    // Se o arquivo já existia, deleta a versão antiga (precisa destravar temporariamente)
    fat32_directory_entry_t dummy;
    if (fat32_find_entry(name, &dummy) == 0) {
        spin_unlock(&fat32_lock);
        fat32_delete_file(name);
        spin_lock(&fat32_lock);
    }

    uint32_t bytes_per_cluster = disk_bpb.sectors_per_cluster * 512;
    uint32_t bytes_left = size;
    uint32_t current_c, prev_c = 0, first_c = 0;
    uint8_t* data_ptr = input_buffer;

    // Loop de Alocação e Escrita Física nos Clusters
    while (bytes_left > 0) {
        current_c = fat32_find_free_cluster() & 0x0FFFFFFF;
        if (current_c == 0) {
            spin_unlock(&fat32_lock);
            return FS_DISK_FULL;
        }
        
        if (first_c == 0) first_c = current_c;
        if (prev_c != 0) fat32_set_cluster_entry(prev_c, current_c);

        uint8_t* cluster_buf = (uint8_t*)kmalloc(bytes_per_cluster);
        if (!cluster_buf) {
            spin_unlock(&fat32_lock);
            return FS_ERROR_WRITE;
        }
        memset(cluster_buf, 0, bytes_per_cluster);

        uint32_t to_write = (bytes_left > bytes_per_cluster) ? bytes_per_cluster : bytes_left;
        memcpy(cluster_buf, data_ptr, to_write);

        // ESCRITA OTIMIZADA DO CLUSTER INTEIRO
        storage_write_sectors(fat32_current_dev_id, fat32_cluster_to_lba(current_c), disk_bpb.sectors_per_cluster, cluster_buf);
        
        fat32_set_cluster_entry(current_c, FAT32_EOC);
        kfree(cluster_buf);

        data_ptr += to_write;
        bytes_left -= to_write;
        prev_c = current_c;
    }

    // 3. Registrar entrada no diretório pai
    __attribute__((aligned(16))) uint8_t sect[512];
    
    while (dir_c < FAT32_EOC && dir_c >= 2) {
        uint32_t lba = fat32_cluster_to_lba(dir_c);
        for (uint32_t s = 0; s < disk_bpb.sectors_per_cluster; s++) {
            storage_read_sectors(fat32_current_dev_id, lba + s, 1, sect);
            
            for (int i = 0; i < 512; i += 32) {
                if (sect[i] == 0x00 || (uint8_t)sect[i] == 0xE5) {
                    fat32_directory_entry_t* e = (fat32_directory_entry_t*)&sect[i];
                    memcpy(e->filename, fat_name, 11);
                    e->attributes = 0x20; 
                    e->cluster_high = (first_c >> 16) & 0xFFFF;
                    e->cluster_low = first_c & 0xFFFF;
                    e->file_size = size;
                    
                    storage_write_sectors(fat32_current_dev_id, lba + s, 1, sect);
                    spin_unlock(&fat32_lock);
                    return FS_SUCCESS;
                }
            }
        }
        dir_c = fat32_get_next_cluster(dir_c) & 0x0FFFFFFF;
    }
    
    spin_unlock(&fat32_lock);
    return FS_DISK_FULL;
}

/**
 * @brief Deleta um arquivo e limpa a tabela FAT.
 */
int fat32_delete_file(const char* filename) {
    spin_lock(&fat32_lock); // ADQUIRE A TRAVA

    char fat_name[11];
    fat32_to_83_filename(filename, fat_name);
    __attribute__((aligned(16))) uint8_t sect[512];
    uint32_t dir_c = current_dir_cluster & 0x0FFFFFFF;

    while (dir_c < FAT32_EOC && dir_c >= 2) {
        uint32_t lba = fat32_cluster_to_lba(dir_c);
        for (uint32_t s = 0; s < disk_bpb.sectors_per_cluster; s++) {
            storage_read_sectors(fat32_current_dev_id, lba + s, 1, sect);
            
            fat32_directory_entry_t* e = (fat32_directory_entry_t*)sect;
            for (int i = 0; i < 16; i++) {
                if (e[i].filename[0] == 0x00) {
                    spin_unlock(&fat32_lock);
                    return FS_NOT_FOUND;
                }
                if (memcmp(e[i].filename, fat_name, 11) == 0) {
                    uint32_t c = (((uint32_t)e[i].cluster_high << 16) | e[i].cluster_low) & 0x0FFFFFFF;
                    e[i].filename[0] = 0xE5; 
                    
                    storage_write_sectors(fat32_current_dev_id, lba + s, 1, sect);
                    
                    while (c >= 2 && c < FAT32_EOC) {
                        uint32_t next = fat32_get_next_cluster(c) & 0x0FFFFFFF;
                        fat32_set_cluster_entry(c, 0); 
                        if (next == c) break;
                        c = next;
                    }
                    spin_unlock(&fat32_lock);
                    return FS_SUCCESS;
                }
            }
        }
        dir_c = fat32_get_next_cluster(dir_c) & 0x0FFFFFFF;
    }
    
    spin_unlock(&fat32_lock);
    return FS_NOT_FOUND;
}

/**
 * @brief Adiciona dados ao final de um arquivo existente.
 */
int fat32_append_file(const char* filename, uint8_t* data, uint32_t data_len) {
    spin_lock(&fat32_lock); // ADQUIRE A TRAVA

    if (!filename || !data || data_len == 0) {
        spin_unlock(&fat32_lock);
        return FS_SUCCESS;
    }

    fat32_directory_entry_t entry;
    uint32_t entry_lba = 0;
    int entry_idx = 0;
    
    uint32_t dir_c = current_dir_cluster & 0x0FFFFFFF;
    __attribute__((aligned(16))) uint8_t sect[512];
    char fat_name[11];
    fat32_to_83_filename(filename, fat_name);
    int found = 0;

    while (dir_c < FAT32_EOC && dir_c >= 2 && !found) {
        uint32_t lba = fat32_cluster_to_lba(dir_c);
        for (uint32_t s = 0; s < disk_bpb.sectors_per_cluster; s++) {
            storage_read_sectors(fat32_current_dev_id, lba + s, 1, sect);
            
            fat32_directory_entry_t* es = (fat32_directory_entry_t*)sect;
            for (int i = 0; i < 16; i++) {
                if (memcmp(es[i].filename, fat_name, 11) == 0) {
                    entry = es[i]; 
                    entry_lba = lba + s; 
                    entry_idx = i;
                    found = 1; 
                    break;
                }
            }
            if (found) break;
        }
        if (!found) dir_c = fat32_get_next_cluster(dir_c) & 0x0FFFFFFF;
    }
    
    if (!found) {
        spin_unlock(&fat32_lock);
        return FS_NOT_FOUND;
    }

    uint32_t bytes_per_cluster = disk_bpb.sectors_per_cluster * 512;
    uint32_t last_c = (((uint32_t)entry.cluster_high << 16) | entry.cluster_low) & 0x0FFFFFFF;
    
    while ((fat32_get_next_cluster(last_c) & 0x0FFFFFFF) < FAT32_EOC) {
        uint32_t nxt = fat32_get_next_cluster(last_c) & 0x0FFFFFFF;
        if (nxt == last_c || nxt < 2) break;
        last_c = nxt;
    }

    uint32_t pos = entry.file_size % bytes_per_cluster;
    uint32_t written = 0;
    uint8_t* buf = (uint8_t*)kmalloc(bytes_per_cluster);
    if (!buf) {
        spin_unlock(&fat32_lock);
        return FS_ERROR_WRITE;
    }

    if (pos != 0 || entry.file_size == 0) {
        uint32_t space = bytes_per_cluster - pos;
        
        // LEITURA DO CLUSTER INTEIRO
        storage_read_sectors(fat32_current_dev_id, fat32_cluster_to_lba(last_c), disk_bpb.sectors_per_cluster, buf);
        
        uint32_t to_copy = (data_len < space) ? data_len : space;
        memcpy(buf + pos, data, to_copy);
        
        // ESCRITA DO CLUSTER INTEIRO
        storage_write_sectors(fat32_current_dev_id, fat32_cluster_to_lba(last_c), disk_bpb.sectors_per_cluster, buf);
        written = to_copy;
    }

    uint32_t prev = last_c;
    while (written < data_len) {
        uint32_t new_c = fat32_find_free_cluster() & 0x0FFFFFFF;
        if (!new_c) { 
            kfree(buf); 
            spin_unlock(&fat32_lock);
            return FS_DISK_FULL; 
        }
        
        fat32_set_cluster_entry(prev, new_c);
        fat32_set_cluster_entry(new_c, FAT32_EOC);
        memset(buf, 0, bytes_per_cluster);
        
        uint32_t to_c = (data_len - written > bytes_per_cluster) ? bytes_per_cluster : (data_len - written);
        memcpy(buf, data + written, to_c);
        
        storage_write_sectors(fat32_current_dev_id, fat32_cluster_to_lba(new_c), disk_bpb.sectors_per_cluster, buf);
        written += to_c; 
        prev = new_c;
    }

    storage_read_sectors(fat32_current_dev_id, entry_lba, 1, sect);
    ((fat32_directory_entry_t*)sect)[entry_idx].file_size += data_len;
    storage_write_sectors(fat32_current_dev_id, entry_lba, 1, sect);

    kfree(buf);
    spin_unlock(&fat32_lock);
    return FS_SUCCESS;
}

/**
 * @brief Cria um novo subdiretório no diretório atual.
 */
int fat32_create_dir(const char* name) {
    spin_lock(&fat32_lock); // ADQUIRE A TRAVA

    char fat_name[11];
    fat32_to_83_filename(name, fat_name);

    fat32_directory_entry_t dummy;
    if (fat32_find_entry(name, &dummy) == 0) {
        spin_unlock(&fat32_lock);
        return FS_NOT_FOUND; 
    }

    uint32_t new_cluster = fat32_find_free_cluster() & 0x0FFFFFFF;
    if (new_cluster == 0) {
        spin_unlock(&fat32_lock);
        return FS_DISK_FULL;
    }

    fat32_set_cluster_entry(new_cluster, FAT32_EOC);

    uint32_t bytes_per_cluster = disk_bpb.sectors_per_cluster * 512;
    uint8_t* cluster_buf = (uint8_t*)kmalloc(bytes_per_cluster);
    if (!cluster_buf) {
        spin_unlock(&fat32_lock);
        return FS_DISK_FULL;
    }
    memset(cluster_buf, 0, bytes_per_cluster);

    // Configura a entrada "."
    fat32_directory_entry_t* dot = (fat32_directory_entry_t*)&cluster_buf[0];
    memset(dot->filename, ' ', 11);
    dot->filename[0] = '.';        
    dot->attributes = 0x10;        
    dot->cluster_high = (new_cluster >> 16) & 0xFFFF;
    dot->cluster_low = new_cluster & 0xFFFF;
    dot->file_size = 0;

    // Configura a entrada ".."
    uint32_t parent_c = current_dir_cluster & 0x0FFFFFFF;
    if (parent_c == 2) parent_c = 0;

    fat32_directory_entry_t* dotdot = (fat32_directory_entry_t*)&cluster_buf[32];
    memset(dotdot->filename, ' ', 11);
    dotdot->filename[0] = '.';        
    dotdot->filename[1] = '.';        
    dotdot->attributes = 0x10;        
    dotdot->cluster_high = (parent_c >> 16) & 0xFFFF;
    dotdot->cluster_low = parent_c & 0xFFFF;
    dotdot->file_size = 0;

    // Gravação do cluster de uma só vez
    uint32_t start_lba = fat32_cluster_to_lba(new_cluster);
    storage_write_sectors(fat32_current_dev_id, start_lba, disk_bpb.sectors_per_cluster, cluster_buf);
    kfree(cluster_buf);

    // Registra a entrada no diretório pai
    uint32_t dir_c = current_dir_cluster & 0x0FFFFFFF;
    __attribute__((aligned(16))) uint8_t sect[512];

    while (dir_c < FAT32_EOC && dir_c >= 2) {
        uint32_t lba = fat32_cluster_to_lba(dir_c);
        for (uint32_t s = 0; s < disk_bpb.sectors_per_cluster; s++) {
            storage_read_sectors(fat32_current_dev_id, lba + s, 1, sect);

            fat32_directory_entry_t* e = (fat32_directory_entry_t*)sect;
            for (int i = 0; i < 16; i++) {
                if (e[i].filename[0] == 0x00 || (uint8_t)e[i].filename[0] == 0xE5) {
                    memcpy(e[i].filename, fat_name, 11);
                    e[i].attributes = 0x10; 
                    e[i].cluster_high = (new_cluster >> 16) & 0xFFFF;
                    e[i].cluster_low = new_cluster & 0xFFFF;
                    e[i].file_size = 0;

                    storage_write_sectors(fat32_current_dev_id, lba + s, 1, sect);
                    spin_unlock(&fat32_lock);
                    return FS_SUCCESS;
                }
            }
        }
        dir_c = fat32_get_next_cluster(dir_c) & 0x0FFFFFFF;
    }

    spin_unlock(&fat32_lock);
    return FS_DISK_FULL;
}

int fat32_rename(const char* old_name, const char* new_name) {
    spin_lock(&fat32_lock); // ADQUIRE A TRAVA

    fat32_directory_entry_t entry;
    uint32_t setor_da_entrada = 0;
    uint32_t offset_no_setor = 0;

    if (!old_name || !new_name) {
        spin_unlock(&fat32_lock);
        return -1;
    }

    if (fat32_find_entry_ext(old_name, &entry, &setor_da_entrada, &offset_no_setor) != 0) {
        spin_unlock(&fat32_lock);
        return -1; 
    }

    fat32_directory_entry_t dummy;
    if (fat32_find_entry(new_name, &dummy) == 0) {
        spin_unlock(&fat32_lock);
        return -2; 
    }

    __attribute__((aligned(16))) uint8_t setor_buffer[512];
    if (storage_read_sectors(fat32_current_dev_id, setor_da_entrada, 1, setor_buffer) == 0) {
        spin_unlock(&fat32_lock);
        return -3; 
    }

    fat32_directory_entry_t* entry_no_disco = (fat32_directory_entry_t*)&setor_buffer[offset_no_setor];

    char nome_fat[11];
    fat32_to_83_filename(new_name, nome_fat);

    memcpy(entry_no_disco->filename, nome_fat, 11);

    if (storage_write_sectors(fat32_current_dev_id, setor_da_entrada, 1, setor_buffer) == 0) {
        spin_unlock(&fat32_lock);
        return -4; 
    }

    spin_unlock(&fat32_lock);
    return 0; 
}

/**
 * @brief Obtém metadados de um arquivo ou pasta no FAT32.
 */
int fat32_stat(const char* name, file_info_t* out_info) {
    spin_lock(&fat32_lock); // ADQUIRE A TRAVA

    fat32_directory_entry_t entry;
    
    if (!name || !out_info) {
        spin_unlock(&fat32_lock);
        return -1;
    }

    if (fat32_find_entry(name, &entry) != 0) {
        spin_unlock(&fat32_lock);
        return -1; 
    }

    out_info->size = entry.file_size;        
    out_info->attributes = entry.attributes;  

    spin_unlock(&fat32_lock);
    return 0; 
}
