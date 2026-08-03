#ifndef AHCI_PCI_GERENCIADOR_H
#define AHCI_PCI_GERENCIADOR_H

#include <stdint.h>

typedef struct {
    uint32_t bar5_phys;  
    uint8_t bus;         
    uint8_t slot;        
    uint8_t func;        
    uint8_t encontrado; // Mudado de bool para uint8_t
} ahci_pci_info_t;

ahci_pci_info_t ahci_pci_detectar_info(void);

#endif // AHCI_PCI_GERENCIADOR_H
