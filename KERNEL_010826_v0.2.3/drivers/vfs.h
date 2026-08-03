/**
 * ============================================================================
 * VFS - VIRTUAL FILE SYSTEM HEADER - V3.3 (AHCI & FAT32 SYNC)
 * ============================================================================
 * Descrição: Definições de estruturas para a camada de abstração (VFS).
 * Sincronizado com o vfs.c híbrido (RamFS + FAT32 SATA).
 * ============================================================================
 */

#ifndef FS_VFS_H
#define FS_VFS_H

#include <stdint.h>
#include <stddef.h>

/* --- VFS NODE TYPES --- */
#define VFS_TYPE_FILE         0x01
#define VFS_TYPE_DIRECTORY    0x02
#define VFS_TYPE_CHAR_DEVICE  0x03  /* Teclado, Terminal, Serial */
#define VFS_TYPE_BLOCK_DEVICE 0x04  /* Discos Rígidos, Partições */

/* Limite de arquivos abertos simultaneamente por processo */
#define MAX_FILES_PER_PROCESS 16

/* --- CÓDIGOS DE ERRO (PADRÃO POSIX) --- */
#define EPERM    1   /* Operation not permitted */
#define ENOENT   2   /* No such file or directory */
#define EIO      5   /* I/O error */
#define EBADF    9   /* Bad file number (FD inválido ou fechado) */
#define EACCES  13   /* Permission denied */
#define EFAULT  14   /* Bad address (Ponteiro fora do espaço de usuário) */

/* Forward declaration da estrutura de nó */
struct vfs_node;

/**
 * @brief Tabela de operações (function pointers) para um nó do VFS.
 */
typedef struct {
    uint32_t (*read)(struct vfs_node*, uint32_t offset, uint32_t size, uint8_t* buffer);
    uint32_t (*write)(struct vfs_node*, uint32_t offset, uint32_t size, uint8_t* buffer);
    void     (*open)(struct vfs_node*);
    void     (*close)(struct vfs_node*);
    struct vfs_node* (*readdir)(struct vfs_node*, uint32_t index);
    struct vfs_node* (*finddir)(struct vfs_node*, char* name);
} vfs_ops_t;

/**
 * @brief Estrutura base de um nó (Arquivo, Diretório ou Dispositivo).
 */
typedef struct vfs_node {
    char name[128];
    uint32_t type;
    uint32_t size;
    vfs_ops_t *ops;
    void *ptr;             /* RAMFS: Dados | FAT32: Metadata/Cluster inicial */
    struct vfs_node *next; /* Encadeamento para lista de diretórios (irmãos) */
} vfs_node_t;

/**
 * @brief Estrutura que representa um arquivo aberto (File Descriptor entry).
 */
typedef struct {
    vfs_node_t *node;
    uint32_t offset;
    uint32_t used;
} open_file_t;

/* --- SYSCALL INTERFACE (KERNEL SIDE) --- */

/**
 * @brief Abre um arquivo pelo caminho e retorna um File Descriptor (index).
 * @param path Caminho absoluto do arquivo (ex: "/EXPLORER.ELF").
 * @return FD (0 a 15) em caso de sucesso, ou código negativo em erro (-ENOENT, etc).
 */
int sys_open(char *path);

/**
 * @brief Lê dados de um File Descriptor para um buffer de usuário.
 * Realiza checagem de ponteiros contra limites de memória do Ring 3 (Segurança 64-bit).
 * @return Quantidade de bytes lidos ou código de erro negativo.
 */
int sys_read(int fd, uint8_t *buffer, uint32_t size);

/**
 * @brief Escreve dados de um buffer de usuário para um File Descriptor.
 * Redireciona FDs 1 e 2 diretamente para o terminal gráfico VESA chamando video_flush().
 * @return Quantidade de bytes escritos ou código de erro negativo.
 */
int sys_write(int fd, uint8_t *buffer, uint32_t size);

/**
 * @brief Fecha um descritor de arquivo, liberando a entrada na tabela.
 * Se o nó associado for volátil (gerado pelo FAT32), gerencia a liberação do nó.
 * @return 0 em sucesso, ou -EBADF se o descritor for inválido.
 */
int sys_close(int fd);

/* --- VFS CORE FUNCTIONS --- */

/**
 * @brief Localiza um nó no sistema de arquivos percorrendo o path informado.
 * Transita recursivamente ou interativamente entre RamFS e FAT32.
 */
vfs_node_t* vfs_lookup(char *path);

/**
 * @brief Inicializa a raiz "/" de modo combinado e configura STDIN, STDOUT e STDERR.
 * @param keyboard_node Nó do driver de teclado.
 * @param video_node Nó do driver de vídeo (VESA/LFB).
 */
void vfs_init_standard_streams(vfs_node_t *keyboard_node, vfs_node_t *video_node);

#endif /* FS_VFS_H */
