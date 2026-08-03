#ifndef AHCI_PCI_H
#define AHCI_PCI_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Estrutura que armazena as informações de localização e 
 * controle da controladora AHCI no barramento PCI.
 */
typedef struct {
    uint32_t bar5_phys;  // Endereço físico do ABAR (AHCI Base Address Register)
    uint8_t bus;         // Barramento PCI onde o dispositivo foi encontrado
    uint8_t slot;        // Slot PCI do dispositivo
    uint8_t func;        // Função PCI do dispositivo
    bool encontrado;     // Flag indicando se a controladora foi detectada
} ahci_pci_info_t;

/**
 * @brief Varre o barramento PCI à procura de uma controladora SATA/AHCI compatível.
 * @return Uma estrutura ahci_pci_info_t preenchida com os dados do dispositivo e o BAR5 ativado.
 */
ahci_pci_info_t ahci_pci_detectar(void);

#endif // AHCI_PCI_H

