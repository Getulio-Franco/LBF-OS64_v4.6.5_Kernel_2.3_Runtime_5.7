#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stdbool.h> // Necessário para o tipo bool

/**
 * ============================================================================
 * ELF.H - CARREGADOR DE EXECUTÁVEIS 64-BIT
 * ============================================================================
 */

// --- Índices do e_ident ---
#define EI_MAG0       0
#define EI_MAG1       1
#define EI_MAG2       2
#define EI_MAG3       3
#define EI_CLASS      4
#define EI_DATA       5

// --- Valores Esperados ---
#define ELFMAG0       0x7F
#define ELFMAG1       'E'
#define ELFMAG2       'L'
#define ELFMAG3       'F'
#define ELFCLASS64    2
#define ELFDATA2LSB   1

// --- Tipos de Arquivo (e_type) ---
#define ET_EXEC       2
#define ET_DYN        3

// --- Tipos de Program Header (p_type) ---
#define PT_NULL       0
#define PT_LOAD       1
#define PT_DYNAMIC    2
#define PT_INTERP     3
#define PT_NOTE       4

/**
 * @struct Elf64_Ehdr
 * @brief Cabeçalho Principal do ELF para x86_64
 */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

/**
 * @struct Elf64_Phdr
 * @brief Cabeçalho de Segmento (Program Header) para x86_64
 */
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;

/* --- Protótipos --- */

// Unifica a assinatura para bool e remove a duplicata no fim do arquivo
bool elf_is_valid(Elf64_Ehdr* elf_header);
void elf_unload_failed_pml4(uint64_t* pml4_phys);
uint64_t elf_load_file(const char* path, uint64_t* pml4_dest);
uint64_t create_elf_process(const char* filename);
void elf_clear_bss(uint64_t* pml4_dest, uint64_t vaddr, uint64_t file_size, uint64_t mem_size);

#endif
