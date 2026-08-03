/**
 * ============================================================================
 * UNIFIED I/O & CPU CONTROL - VERSION 1.1 (Mês 3 - Suporte PCI 32-bit)
 * ============================================================================
 */

#ifndef IO_H
#define IO_H

#include <stdint.h>

/* --- Comunicação com Portas (I/O) --- */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ADICIONADO: Escrita de 32 bits (Long) necessária para o PCI Configuration Space */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

/* ADICIONADO: Leitura de 32 bits (Long) necessária para o PCI Configuration Space */
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* --- Controle de Estado da CPU (Interrupções) --- */

/**
 * @brief Desativa interrupções e retorna o RFLAGS anterior.
 */
static inline uint64_t cli_save() {
    uint64_t flags;
    __asm__ volatile(
        "pushfq\n\t"
        "pop %0\n\t"
        "cli"
        : "=rm" (flags) 
        : 
        : "memory"
    );
    return flags;
}

/**
 * @brief Restaura o estado das interrupções a partir de um RFLAGS salvo.
 */
static inline void sti_restore(uint64_t flags) {
    __asm__ volatile(
        "push %0\n\t"
        "popfq"
        : 
        : "rm" (flags) 
        : "memory", "cc"
    );
}

/* --- Protótipos de Funções Implementadas no .c --- */

void io_wait();
void cpu_halt();

#endif
