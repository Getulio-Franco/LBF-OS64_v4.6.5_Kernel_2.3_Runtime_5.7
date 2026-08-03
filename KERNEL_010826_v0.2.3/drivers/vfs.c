/**
 * ============================================================================
 * VFS - VIRTUAL FILE SYSTEM CORE - V3.3 (FAT32 & AHCI INTEGRATED)
 * ============================================================================
 * Description: Abstração de arquivos, tratamento de RamFS e ganchos para o FAT32.
 * Location: fs/vfs.c
 * ============================================================================
 */

#include "vfs.h"
#include "fs/fat32_file.h"   // Para mapear leituras nativas do FAT32
#include "fs/fat32_logic.h"  // Para mapear buscas de diretório (fat32_find_entry)
#include "mem/heap.h"
#include "util/string.h"
#include "drivers/video.h"    
#include "drivers/keyboard.h" 

/* Tabela global de arquivos abertos */
open_file_t fd_table[MAX_FILES_PER_PROCESS];

/* Nó raiz do sistema ("/") */
vfs_node_t *vfs_root = 0;

// ====================================================================
// SECTION 0: RAMFS (Internal in-memory filesystem)
// ====================================================================

static uint32_t ramfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node->ptr || offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;

    memcpy(buffer, (uint8_t*)node->ptr + offset, size);
    return size;
}

static uint32_t ramfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node->ptr || offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;

    memcpy((uint8_t*)node->ptr + offset, buffer, size);
    return size;
}

static vfs_node_t* ramfs_finddir(vfs_node_t *node, char *name) {
    vfs_node_t *current = (vfs_node_t*)node->ptr; 
    while (current) {
        if (strcmp(current->name, name) == 0) return current;
        current = current->next;
    }
    return 0;
}

static vfs_node_t* ramfs_readdir(vfs_node_t *node, uint32_t index) {
    if ((node->type & VFS_TYPE_DIRECTORY) && node->ptr) {
        vfs_node_t *current = (vfs_node_t*)node->ptr;
        uint32_t i = 0;
        while (current && i < index) {
            current = current->next;
            i++;
        }
        return current;
    }
    return 0;
}

static vfs_ops_t ramfs_file_ops = { .read = ramfs_read, .write = ramfs_write };
static vfs_ops_t ramfs_dir_ops  = { .finddir = ramfs_finddir, .readdir = ramfs_readdir };

// ====================================================================
// SECTION 0.5: GANCHO DE ADAPTAÇÃO FAT32 -> VFS INTERFACE
// ====================================================================

/**
 * @brief Traduz chamadas de leitura do VFS para a lógica de clusters do FAT32.
 */
static uint32_t vfs_fat32_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    // O nome do arquivo foi guardado no nó durante o finddir
    int res = fat32_read_file_at_offset(node->name, buffer, size, offset);
    if (res == FS_SUCCESS) {
        // Como o fat32_read_file_at_offset trunca o tamanho internamente se estourar,
        // precisamos garantir que retornamos a quantidade real lida.
        if (offset + size > node->size) {
            return node->size - offset;
        }
        return size;
    }
    return 0; // Erro de I/O na leitura do disco SATA
}

/**
 * @brief Traduz chamadas de escrita do VFS para a lógica de blocos do FAT32.
 */
static uint32_t vfs_fat32_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    // Para simplificar a escrita direta via Syscall (ex: redirecionamento de output)
    if (offset == 0) {
        if (fat32_write_file(node->name, buffer, size) == FS_SUCCESS) return size;
    } else {
        if (fat32_append_file(node->name, buffer, size) == FS_SUCCESS) return size;
    }
    return 0;
}

/* Tabela de operações que o VFS usará quando detectar que o arquivo está no disco */
static vfs_ops_t fat32_vfs_ops = {
    .read = vfs_fat32_read,
    .write = vfs_fat32_write,
    .open = 0,
    .close = 0
};

/**
 * @brief Intercepta a busca de arquivos no VFS. Se não encontrar no RamFS, busca no FAT32.
 */
static vfs_node_t* vfs_root_combined_finddir(vfs_node_t *node, char *name) {
    // 1. Tenta buscar primeiro no RamFS interno (STDIN, STDOUT, drivers virtuais...)
    vfs_node_t *found = ramfs_finddir(node, name);
    if (found) return found;

    // 2. Se não achou na RAM, busca nativamente nos setores do disco FAT32!
    fat32_directory_entry_t fat_entry;
    if (fat32_find_entry(name, &fat_entry) == 0) {
        // Aloca um nó virtual temporário para representar esse arquivo em tempo de execução
        vfs_node_t *fat_node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
        if (!fat_node) return 0;

        memset(fat_node, 0, sizeof(vfs_node_t));
        strncpy(fat_node->name, name, 127);
        
        // Define se é diretório ou arquivo com base nos atributos do FAT32
        if (fat_entry.attributes & 0x10) { // ATTR_DIRECTORY
            fat_node->type = VFS_TYPE_DIRECTORY;
            fat_node->ops = &ramfs_dir_ops; // Diretórios do FAT usam varredura genérica por enquanto
        } else {
            fat_node->type = VFS_TYPE_FILE;
            fat_node->ops = &fat32_vfs_ops;  // Vincula as funções de leitura/escrita SATA!
        }

        fat_node->size = fat_entry.file_size;
        // Armazena o cluster inicial no ponteiro genérico privado caso o loader precise
        fat_node->ptr = (void*)(uintptr_t)(((uint32_t)fat_entry.cluster_high << 16) | fat_entry.cluster_low);
        fat_node->next = 0;

        return fat_node;
    }

    return 0; // Arquivo verdadeiramente não existe no sistema
}

/* Operação de diretório customizada para a raiz unificada */
static vfs_ops_t combined_root_ops = {
    .finddir = vfs_root_combined_finddir,
    .readdir = ramfs_readdir
};

// ====================================================================
// SECTION 1: CORE VFS OPERATIONS
// ====================================================================

vfs_node_t* vfs_lookup(char *path) {
    if (!vfs_root || !path) return 0;
    if (path[0] == '/' && path[1] == '\0') return vfs_root;

    vfs_node_t *current_node = vfs_root;
    char name_buffer[128];
    int i = (path[0] == '/') ? 1 : 0;

    while (path[i] != '\0') {
        int j = 0;
        while (path[i] != '/' && path[i] != '\0') {
            name_buffer[j++] = path[i++];
            if (j >= 127) break;
        }
        name_buffer[j] = '\0';

        vfs_node_t *next_node = (current_node->ops && current_node->ops->finddir) ? 
                               current_node->ops->finddir(current_node, name_buffer) : 0;
        
        if (!next_node) return 0;
        
        // Se o nó anterior foi um nó gerado dinamicamente pelo FAT32 e não for a raiz, 
        // precisamos gerenciar a memória para evitar memory leaks (pode ser expandido no futuro)
        current_node = next_node;
        if (path[i] == '/') i++;
    }
    return current_node;
}

// ====================================================================
// SECTION 2: SYSCALL INTERFACE (Userland Compatibility)
// ====================================================================

int sys_open(char *path) {
    if (!path) return -1;

    vfs_node_t *node = vfs_lookup(path);
    if (!node) return -1;

    for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) {
        if (!fd_table[i].used) {
            fd_table[i].node = node;
            fd_table[i].offset = 0;
            fd_table[i].used = 1;
            
            if (node->ops && node->ops->open) node->ops->open(node);
            return i;
        }
    }
    return -1; 
}

int sys_read(int fd, uint8_t *buffer, uint32_t size) {
    if (!buffer || size == 0) return -1;

    uintptr_t buffer_end = (uintptr_t)buffer + size;
    if ((uintptr_t)buffer >= 0xFFFF800000000000 || buffer_end >= 0xFFFF800000000000) {
        return -14; 
    }

    if (fd == 0) {
        if (keyboard_device_node.ops && keyboard_device_node.ops->read) {
            return (int)keyboard_device_node.ops->read(&keyboard_device_node, 0, size, buffer);
        }
        return 0; 
    }

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !fd_table[fd].used) return -1;

    open_file_t *file = &fd_table[fd];

    if (file->node->ops && file->node->ops->read) {
        uint32_t bytes_read = file->node->ops->read(file->node, file->offset, size, buffer);
        file->offset += bytes_read; 
        return (int)bytes_read;
    }

    return 0; 
}

int sys_write(int fd, uint8_t *buffer, uint32_t size) {
    if (!buffer || size == 0) return -1;

    uint64_t buffer_end = (uint64_t)buffer + size;
    if ((uint64_t)buffer >= 0xFFFF800000000000 || buffer_end >= 0xFFFF800000000000) {
        return -14; 
    }

    if (fd == 1 || fd == 2) {
        for (uint32_t i = 0; i < size; i++) {
            terminal_putc(buffer[i]);
        }
       // video_flush(); // já é chamado no syscall esta chamando duas vezes - 04
        return (int)size;
    }

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !fd_table[fd].used) return -1;

    open_file_t *file = &fd_table[fd];

    if (file->node->ops && file->node->ops->write) {
        uint32_t bytes_written = file->node->ops->write(file->node, file->offset, size, buffer);
        file->offset += bytes_written; 
        return (int)bytes_written;
    }
    
    return 0; 
}

int sys_close(int fd) {
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !fd_table[fd].used) return -EBADF;

    open_file_t *file = &fd_table[fd];
    if (file->node->ops && file->node->ops->close) {
        file->node->ops->close(file->node);
    }

    // Se o nó foi alocado dinamicamente pelo buscador do FAT32, libera a memória dele
    // Nota: STDIN e STDOUT são estáticos e não entram aqui.
    if (file->node != vfs_root && file->node->ops == &fat32_vfs_ops) {
        kfree(file->node);
    }

    file->node = 0;
    file->offset = 0;
    file->used = 0;
    return 0;
}

// ====================================================================
// SECTION 3: INITIALIZATION
// ====================================================================

void vfs_init_standard_streams(vfs_node_t *keyboard_node, vfs_node_t *video_node) {
    /* Reset FD table */
    for(int i = 0; i < MAX_FILES_PER_PROCESS; i++) {
        fd_table[i].used = 0;
    }

    /* Inicializa Root "/" vinculando-o à busca unificada RamFS + FAT32 */
    vfs_root = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    memset(vfs_root, 0, sizeof(vfs_node_t));
    strcpy(vfs_root->name, "/");
    vfs_root->type = VFS_TYPE_DIRECTORY;
    vfs_root->ops = &combined_root_ops; // Mapeia a nova busca unificada híbrida!

    /* FD 0 = STDIN (Teclado) */
    if (keyboard_node) { 
        fd_table[0].node = keyboard_node; 
        fd_table[0].used = 1; 
    }
    
    /* FD 1 = STDOUT e FD 2 = STDERR */
    if (video_node) { 
        fd_table[1].node = video_node; 
        fd_table[1].used = 1; 
        fd_table[2].node = video_node; 
        fd_table[2].used = 1; 
    }
}
