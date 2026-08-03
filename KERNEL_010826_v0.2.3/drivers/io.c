/**
 * ============================================================================
 * I/O & CPU SUPPORT IMPLEMENTATION
 * ============================================================================
 */

#include "io.h"

/**
 * @brief Pequeno atraso de I/O (I/O Wait).
 * Essencial para dar tempo ao hardware antigo (como o PIC) de processar.
 */
void io_wait() {
    // Porta 0x80 é segura para escrita de lixo e gera um delay de barramento
    outb(0x80, 0); 
}

/**
 * @brief Coloca a CPU em estado de baixo consumo até a próxima interrupção.
 */
void cpu_halt() {
    __asm__ volatile ("hlt");
}
