#ifndef SERIAL_PCI_GERENCIADOR_H
#define SERIAL_PCI_GERENCIADOR_H

#include <stdint.h>

// Estrutura para mapeamento da porta serial PCI detectada
typedef struct {
    uint16_t io_base;    // Endereço base de I/O
    uint8_t irq;         // Linha de interrupção associada
    uint8_t encontrado;  // Mudado de bool para uint8_t
} serial_pci_info_t;

// Função para inicializar e retornar os dados da porta serial PCI
serial_pci_info_t serial_pci_init_info(void);

#endif // SERIAL_PCI_GERENCIADOR_H
