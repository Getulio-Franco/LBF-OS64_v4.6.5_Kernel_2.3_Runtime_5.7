#ifndef E820_H
#define E820_H

#include <stdint.h>

/**
 * @struct e820_entry_t
 * @brief Estrutura de 24 bytes retornada pela interrupção INT 15h, EAX=E820.
 */
typedef struct {
    uint64_t base;      // Endereço físico inicial
    uint64_t length;    // Tamanho da região em bytes
    uint32_t type;      // 1 = RAM Disponível, 2 = Reservada, 3 = ACPI Reclaim, etc.
    uint32_t extended;  // Atributos (ignorado na maioria dos casos)
} __attribute__((packed)) e820_entry_t;

void detect_memory_from_e820();

#endif
