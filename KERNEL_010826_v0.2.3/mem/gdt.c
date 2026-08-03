#include "gdt.h"
#include "util/string.h"
#include "drivers/video.h"

gdt_entry_t gdt_entries[8] __attribute__((aligned(16)));
gdt_ptr_t   gdt_ptr;
tss_t       kernel_tss __attribute__((aligned(16)));

static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

void install_tss(void) {
    uint64_t base = (uint64_t)&kernel_tss;
    uint32_t limit = sizeof(tss_t) - 1;

    memset(&kernel_tss, 0, sizeof(tss_t));
    
    // Obtém o RSP atual de forma segura via função NASM
    kernel_tss.rsp0 = get_rsp(); 
    kernel_tss.iopb_offset = sizeof(tss_t);

    // Estrutura de 16 bytes mapeada nos slots 5 e 6 da GDT
    tss_descriptor_t *tss_desc = (tss_descriptor_t*)&gdt_entries[5];
    
    tss_desc->limit_low    = (limit & 0xFFFF);
    tss_desc->base_low     = (base & 0xFFFF);
    tss_desc->base_middle  = (base >> 16) & 0xFF;
    tss_desc->access       = 0x89; // Presente, Ring 0, Executável, TSS de 64 bits
    tss_desc->granularity  = (limit >> 16) & 0x0F;
    tss_desc->base_high    = (base >> 24) & 0xFF;
    tss_desc->base_highest = (base >> 32) & 0xFFFFFFFF;
    tss_desc->reserved     = 0;
}

void tss_set_stack(uint64_t stack) {
    if (stack == 0) {
        vga_print_string("ERRO: TSS RSP0 SET TO ZERO!", 0, 0);
        return;
    }
    kernel_tss.rsp0 = stack; 
}

void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 8) - 1;
    gdt_ptr.base  = (uint64_t)&gdt_entries;

    memset(gdt_entries, 0, sizeof(gdt_entries));

    gdt_set_gate(0, 0, 0, 0, 0);                // 0x00: Null
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xAF);    // 0x08: Kernel Code (Long Mode)
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);    // 0x10: Kernel Data
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xAF);    // 0x18: User Code 
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);    // 0x20: User Data 

    install_tss();

    // Invoca o recarregamento da GDT e do LTR em Assembly puro
    gdt_flush(&gdt_ptr);
}
