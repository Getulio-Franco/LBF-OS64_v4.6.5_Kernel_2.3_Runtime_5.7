/* ============================================================================
 * liblib.h - Interface Nativa do S.O. LBF-VESA (SDK Ring 3) - v.0.0.5
 * ============================================================================
 * Esta biblioteca é a representação externa do syscall.c. 
 * Sem dependências externas. Pura e nativa.
 * ============================================================================
 */

#ifndef LIBLIB_H
#define LIBLIB_H

/* ============================================================================
 * 1. DEFINIÇÕES DE TIPOS
 * ============================================================================ */
typedef __uint128_t    uint128_t;
typedef __UINT64_TYPE__    uint64_t;
typedef __UINT32_TYPE__    uint32_t;
typedef __UINT16_TYPE__    uint16_t;
typedef __UINT8_TYPE__     uint8_t;

typedef __INT64_TYPE__     int64_t;
typedef __INT32_TYPE__     int32_t;

typedef __INTPTR_TYPE__    intptr_t;
typedef __SIZE_TYPE__      size_t;
typedef __UINTPTR_TYPE__   uintptr_t;
typedef __PTRDIFF_TYPE__   ptrdiff_t;

#ifndef __cplusplus
#define bool  _Bool
#define true  1
#define false 0
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef __builtin_va_list va_list;
#define va_start(v, l)   __builtin_va_start(v, l)
#define va_end(v)        __builtin_va_end(v)
#define va_arg(v, l)     __builtin_va_arg(v, l)

/* --- 2. ESTRUTURAS DO SISTEMA --- */
typedef struct {
    uint64_t pid;
    char name[16];
    uint32_t state;
    uint64_t cr3;
} TProcessInfo;

typedef struct {
    uint64_t x;
    uint64_t y;
    uint64_t buttons;
} mouse_t;

typedef struct {
    uint64_t lfb_address;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
} vesa_info_t;

// Estrutura simplificada para o STAT retornar os metadados do arquivo
typedef struct {
    uint32_t size;       // Tamanho em bytes
    uint8_t attributes;  // Atributos do FAT32 (0x10 indica se é diretório)
} file_info_t;

/* ============================================================================
 * 2. TABELA DE SYSCALLS (Sincronizada com syscall.c)
 * ============================================================================ */

#define SYS_OPEN            0
#define SYS_READ            1
#define SYS_WRITE           2
#define SYS_CLOSE           3
#define SYS_CLEAR           4  
#define SYS_MEM_INFO        5
#define SYS_SERIAL_WRITE    6
#define SYS_SERIAL_READ     7
#define SYS_SERIAL_OPEN     8
#define SYS_SERIAL_CLOSE    9

#define SYS_FATLS           10
#define SYS_FATCAT          11
#define SYS_FATRM           12
#define SYS_FATCP           13
#define SYS_FATRENAME       14 //não foi testado ainda so incrementado.
#define SYS_FATAPPEND       15
#define SYS_MKDIR           16
#define SYS_CHDIR           17
#define SYS_XCOPY           18

#define SYS_EXEC            19
#define SYS_EXIT            20
#define SYS_KILL            21
#define SYS_PS              22
#define SYS_GETPID          23
#define SYS_FATSTAT         24 //não foi testado ainda so incrementado.
#define SYS_GET_PS_DATA     25
#define SYS_FATREADDIR      27

#define SYS_GET_PARAM       40
#define SYS_SLEEP           42
#define SYS_GET_TICKS       43
#define SYS_SBRK            45

/* A NOVA BASE GRÁFICA */
#define SYS_GET_MOUSE             60
#define SYS_GET_KEY               61
#define SYS_VIDEO_FLIP            62 
#define SYS_GET_LFB_CRITICAL_DATA 63
#define SYS_GET_PCI_DEVICE  67

/* ============================================================================
 * SYSCALL DE DEBUG / LOGS DO KERNEL (RING 0 -> RING 3)
 * ============================================================================ */
#define SYS_READ_KERNEL_LOG  69

/* ============================================================================
 * 3. MOTOR DE INTERRUPÇÃO (Int 0x80)
 * ============================================================================ */

// Declaração da função externa compilada via Assembly (syscall.asm)
extern uint64_t _do_syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);

// Macros de conveniência usadas pelas funções static inline abaixo
#define _syscall(n, a1, a2, a3) _do_syscall((uint64_t)(n), (uint64_t)(a1), (uint64_t)(a2), (uint64_t)(a3), 0, 0)
#define _syscall5(n, a1, a2, a3, a4, a5) _do_syscall((uint64_t)(n), (uint64_t)(a1), (uint64_t)(a2), (uint64_t)(a3), (uint64_t)(a4), (uint64_t)(a5))
/* ============================================================================
 * 4. WRAPPERS DA API (Interface para o Programador)
 * ============================================================================ */

/* --- I/O, Sistema e Arquivos --- */
static inline int      sys_open(char *path) { return (int)_syscall(SYS_OPEN, path, 0, 0); }
static inline int      sys_read(int fd, void *buf, uint32_t size) { return (int)_syscall(SYS_READ, fd, buf, size); }
static inline int      sys_write(int fd, const void *buf, uint32_t size) { return (int)_syscall(SYS_WRITE, fd, buf, size); }
static inline void     sys_clear() { _syscall(SYS_CLEAR, 0, 0, 0); }
static inline uint64_t sys_get_pid() { return _syscall(SYS_GETPID, 0, 0, 0); }
static inline uint64_t get_system_ticks(void) { return _syscall(SYS_GET_TICKS, 0, 0, 0); }
static inline void     sys_sleep(uint32_t ms) { _syscall(SYS_SLEEP, ms, 0, 0); }
//static inline int      sys_exec(const char* elf_path) {if (!elf_path) return -1; return (int)_syscall(SYS_EXEC, (uint64_t)elf_path, 0, 0);}
static inline void     sys_exec(const char* elf_path) {_syscall(SYS_EXEC, (uint64_t)elf_path, 0, 0);}
static inline void     sys_exit() { _syscall(SYS_EXIT, 0, 0, 0); while(1); }
static inline int      sys_kill(uint64_t pid) {return (int)_syscall(SYS_KILL, pid, 0, 0);}

static inline int putchar(int c) {char ch = (char)c; sys_write(1, &ch, 1); return c; }
static inline char getchar() { char c = 0; while(sys_read(0, &c, 1) == 0); return c; }
static inline int sys_get_ps_data(TProcessInfo* buffer, int max_count) {return (int)_syscall(SYS_GET_PS_DATA, buffer, max_count, 0);}

static inline __attribute__((always_inline)) void* sys_sbrk(intptr_t increment) {return (void*)_syscall(SYS_SBRK, increment, 0, 0);}

/* --- HARDWARE GRÁFICO (A Mágica Ring 3) --- */

// Pega os dados do VESA/Monitor (Substitui o antigo parametro 46)
static inline int get_video_info(vesa_info_t* info) {return (int)_syscall(SYS_GET_LFB_CRITICAL_DATA, info, 0, 0);}

// Pega o estado do Mouse
static inline void get_mouse(mouse_t* data) {_syscall(SYS_GET_MOUSE, data, 0, 0);}

// Pega a Tecla (0 se vazio)
static inline char get_key() {return (char)_syscall(SYS_GET_KEY, 0, 0, 0);}

// Joga o Backbuffer inteiro para a tela de uma vez!
static inline void video_flip(void* backbuffer) {_syscall(SYS_VIDEO_FLIP, backbuffer, 0, 0);}

// Abre a porta (Ex: port_num = 1 para COM1, baud = 9600). Retorna o handleID.
static inline int sys_serial_open(int port_num, uint32_t baud) {
    return (int)_do_syscall(SYS_SERIAL_OPEN, (uint64_t)port_num, (uint64_t)baud, 0, 0, 0);
}

// Envia dados especificando qual ID de porta usar
static inline int sys_serial_write(int port_id, char* data, uint32_t size) {
    return (int)_do_syscall(SYS_SERIAL_WRITE, (uint64_t)port_id, (uint64_t)data, (uint64_t)size, 0, 0);
}

// Lê dados de uma porta específica
static inline int sys_serial_read(int port_id, uint8_t* buffer, uint32_t count) {
    return (int)_do_syscall(SYS_SERIAL_READ, (uint64_t)port_id, (uint64_t)buffer, (uint64_t)count, 0, 0);
}

// Fecha a porta serial liberando o lock (in_use = false) e restaurando o PIC
static inline int sys_serial_close(int port_id) {
    return (int)_do_syscall(SYS_SERIAL_CLOSE, (uint64_t)port_id, 0, 0, 0, 0);
}

/* ========================================================================
   APIs DE ESPAÇO DE USUÁRIO (RING 3) PARA O GERENCIADOR DE ARQUIVOS
   ========================================================================
*/
static inline int sys_fat_ls(void) {return (int)_syscall(SYS_FATLS, 0, 0, 0);}
static inline int sys_fat_cat(const char* filename) {return (int)_syscall(SYS_FATCAT, filename, 0, 0);}
static inline int sys_fat_rm(const char* target) {return (int)_syscall(SYS_FATRM, target, 0, 0);}
static inline int sys_fat_copy(const char* source, const char* dest) {return (int)_syscall(SYS_FATCP, source, dest, 0);}
static inline int sys_fat_mkdir(const char* dirname) {return (int)_syscall(SYS_MKDIR, dirname, 0, 0);}
static inline int sys_fat_chdir(const char* target_dir) {return (int)_syscall(SYS_CHDIR, target_dir, 0, 0);}
static inline int sys_fat_append(const char* filename, const char* data, uint32_t size) {return (int)_do_syscall(SYS_FATAPPEND, (uint64_t)filename, (uint64_t)data, (uint64_t)size, 0, 0);}
static inline int sys_fat_rename(const char* old_name, const char* new_name) {return (int)_syscall(SYS_FATRENAME, old_name, new_name, 0);} //Renomeia ou move um arquivo/diretório no FAT32.
static inline int sys_fat_stat(const char* filename, file_info_t* info) {return (int)_syscall(SYS_FATSTAT, filename, info, 0);} //Obtém informações (tamanho, tipo) de um item.
static inline int sys_fat_readdir(int idx, char* buf, file_info_t* info) {return (int)_syscall(SYS_FATREADDIR, idx, buf, info);}

static inline int sys_get_pci_device(int index, uintptr_t user_dev_ptr) {return (int)_syscall(SYS_GET_PCI_DEVICE, (uint64_t)index, (uint64_t)user_dev_ptr, 0);}

/**
 * @brief Lê as mensagens de log/debug geradas pelo Kernel em tempo real.
 * * @param buffer Ponteiro para o buffer no Ring 3 onde os textos serão copiados.
 * @param max_count Tamanho máximo de bytes que o buffer suporta.
 * @return int Quantidade exata de caracteres novos lidos do Kernel (0 se não houver novos logs).
 */
static inline int sys_read_kernel_log(char* buffer, int max_count) {return (int)_syscall(SYS_READ_KERNEL_LOG, buffer, max_count, 0);}
 
/* ============================================================================
 * 5. MALLOC E MEMÓRIA
 * ============================================================================ */
#ifndef MALLOC_FUNCS
#define MALLOC_FUNCS
void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void  free(void* ptr);
#endif

#endif // LIBLIB_H
