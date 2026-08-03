#include "drivers/timer.h"
#include "io.h"
#include "drivers/video.h"
#include "drivers/proc.h" 

#ifndef NULL
#define NULL ((void*)0)
#endif

extern void tss_set_stack(uint64_t stack);
extern void pic_send_eoi(uint8_t irq); // Importado do pic.h

volatile uint64_t system_ticks = 0;

void timer_init(uint32_t frequency) {
    uint32_t divisor = PIT_BASE_FREQ / frequency;
    
    outb(PIT_COMMAND_PORT, 0x36); // Modo 3: Gerador de onda quadrada
    outb(PIT_DATA_PORT_0, (uint8_t)(divisor & 0xFF));
    outb(PIT_DATA_PORT_0, (uint8_t)((divisor >> 8) & 0xFF));
}

uint64_t timer_handler(uint64_t rsp) {
    system_ticks++;

    // O EOI agora é disparado de forma limpa via abstração do PIC se necessário,
    // mas já está sendo tratado de forma fixa no interrupts.asm. Portanto, sem outb redundante aqui.
    
    // 1. O escalonador decide qual será a próxima tarefa
    uint64_t next_rsp = schedule(rsp);

    // 2. Atualização segura do TSS para isolamento de Ring 3
    if (current_process != NULL) {
        uint64_t kernel_stack_top = (uint64_t)current_process->stack_mem + STACK_SIZE;
        tss_set_stack(kernel_stack_top);
    }

    return next_rsp;
}

uint64_t timer_get_ticks() {
    return system_ticks;
}

void timer_wait(uint64_t ticks) {
    uint64_t start_ticks = system_ticks;
    while ((system_ticks - start_ticks) < ticks) {
        __asm__ volatile("sti"); 
        __asm__ volatile("hlt"); 
    }
}

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

uint64_t read_cmos_time(void) {
    return 1711713600ULL; 
}
