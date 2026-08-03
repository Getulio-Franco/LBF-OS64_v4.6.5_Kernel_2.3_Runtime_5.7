#ifndef AHCI_VMM_H
#define AHCI_VMM_H

#include <stdint.h>

/**
 * @brief Mapeia o endereço físico do BAR5 do AHCI na tabela de páginas do Kernel
 * desativando o cache da CPU para comunicação estável em hardware real/VirtualBox.
 * @param phys_addr Endereço físico retornado pelo ahci_pci_detectar (info.bar5_phys).
 * @return Ponteiro virtual mapeado e seguro para uso no driver.
 */
void* ahci_vmm_mapear_abar(uint32_t phys_addr);

#endif // AHCI_VMM_H
