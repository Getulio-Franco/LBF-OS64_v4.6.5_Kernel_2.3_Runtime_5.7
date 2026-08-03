#include "serial_pci_gerenciador.h"
#include "barramento_pci.h"
#include "../video.h"

serial_pci_info_t serial_pci_init_info(void) {
    serial_pci_info_t info;
    info.io_base = 0;
    info.irq = 0;
    info.encontrado = 0; // Mudado de false para 0
    
    pci_device_t dev;

    // Ajustado para checar explicitamente se o retorno numérico indica falha (0)
    if (pci_procurar_dispositivo(0x07, 0x00, -1, &dev) == 0) {
        terminal_print("SERIAL_GERENCIADOR: Nenhuma placa serial PCI encontrada no cache.\n");
        return info;
    }

    terminal_print("SERIAL_GERENCIADOR: Placa Serial detectada no barramento!\n");

    uint32_t bar0 = pci_read_config_info(dev.bus, dev.slot, dev.func, 0x10);
    
    if (bar0 & 0x01) {
        info.io_base = (uint16_t)(bar0 & 0xFFFC);
    } else {
        info.io_base = (uint16_t)(bar0 & 0xFFF0);
    }

    uint32_t reg3C = pci_read_config_info(dev.bus, dev.slot, dev.func, 0x3C);
    info.irq = (uint8_t)(reg3C & 0xFF);
    info.encontrado = 1; // Mudado de true para 1

    uint32_t pci_cmd = pci_read_config_info(dev.bus, dev.slot, dev.func, 0x04);
    pci_write_config_info(dev.bus, dev.slot, dev.func, 0x04, pci_cmd | 0x03);

    terminal_print("SERIAL_GERENCIADOR: Porta I/O mapeada com sucesso.\n");
    return info;
}
