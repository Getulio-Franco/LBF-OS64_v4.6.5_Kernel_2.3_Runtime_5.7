#include "ahci_pci.h"
#include "pci.h"
#include "../../drivers/video.h"
#include "../../util/string.h"
#include <stdint.h>
#include <stdbool.h>

ahci_pci_info_t ahci_pci_detectar(void) {
    ahci_pci_info_t info;
    memset(&info, 0, sizeof(ahci_pci_info_t));

    // 1. Procura pela classe 0x01 (Storage), subclasse 0x06 (SATA / AHCI)
    pci_device_t dev = pci_find_device(0x01, 0x06);
    
    // Fallback: Se não achar, procura por classe 0x01 (Storage), subclasse 0x01 (IDE)
    if (!dev.found) {
        dev = pci_find_device(0x01, 0x01);
    }

    if (!dev.found) {
        terminal_print("AHCI_PCI: Nenhuma controladora SATA/IDE compativel no barramento PCI.\n");
        info.encontrado = false;
        return info;
    }

    // 2. Lê o BAR5 (Offset 0x24 no cabeçalho PCI) e limpa os bits de flag inferiores
    uint32_t bar5 = pci_read_config(dev.bus, dev.slot, dev.func, 0x24);
    info.bar5_phys = bar5 & 0xFFFFFFF0;
    info.bus = dev.bus;
    info.slot = dev.slot;
    info.func = dev.func;
    info.encontrado = true;

    // 3. Ativa Bus Master (Bit 2) e Memory Space (Bit 1) no registrador de Comando PCI (Offset 0x04)
    uint32_t pci_cmd = pci_read_config(dev.bus, dev.slot, dev.func, 0x04);
    pci_write_config(dev.bus, dev.slot, dev.func, 0x04, pci_cmd | 0x06);

    terminal_print("AHCI_PCI: Controladora encontrada com sucesso!\n");
    
    return info;
}
