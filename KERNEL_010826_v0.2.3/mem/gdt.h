#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// 1. Entrada padrão de 8 bytes da GDT.
struct gdt_entry_struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));
typedef struct gdt_entry_struct gdt_entry_t;

// 2. Estrutura explícita de 16-bytes para o descritor TSS em x86_64
struct tss_descriptor_struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_highest; // Bits 32-63 da base física da TSS
    uint32_t reserved;     // Deve ser obrigatoriamente 0
} __attribute__((packed));
typedef struct tss_descriptor_struct tss_descriptor_t;

// 3. Estrutura passada para a instrução LGDT.
struct gdt_ptr_struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));
typedef struct gdt_ptr_struct gdt_ptr_t;

// 4. Estrutura TSS de 64 bits (Task State Segment).
struct tss_struct {
    uint32_t reserved0;
    uint64_t rsp0;      
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];    
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset; 
} __attribute__((packed));
typedef struct tss_struct tss_t;

/* Funções externas em Assembly NASM (gdt_flush.asm) */
extern void gdt_flush(gdt_ptr_t* gdt_ptr_addr);
extern void enter_user_mode(uint64_t entry_point, uint64_t user_stack);
extern uint64_t get_rsp(void);

// Protótipos de Inicialização
void gdt_init();
void tss_set_stack(uint64_t stack);
//void enter_user_mode(uint64_t entry_point, uint64_t user_stack);

#endif
