#include "ahci_pci_gerenciador.h"
#include "barramento_pci.h" 
#include "../../drivers/video.h"
#include "../../util/string.h"

ahci_pci_info_t ahci_pci_detectar_info(void) {
    ahci_pci_info_t info;
    memset(&info, 0, sizeof(ahci_pci_info_t));
    pci_device_t dev;

    // Usando uint8_t para casar com o novo retorno do barramento_pci
    uint8_t achou = pci_procurar_dispositivo(0x01, 0x06, -1, &dev);
    
    // Fallback: Se não achar SATA nativo, busca IDE (Classe 0x01, Subclasse 0x01)
    if (achou == 0) {
        achou = pci_procurar_dispositivo(0x01, 0x01, -1, &dev);
    }

    if (achou == 0) {
        terminal_print("AHCI_GERENCIADOR: Nenhuma controladora SATA/IDE no cache PCI.\n");
        info.encontrado = 0; // Mudado de false para 0
        return info;
    }

    uint32_t bar5 = pci_read_config_info(dev.bus, dev.slot, dev.func, 0x24);
    info.bar5_phys = bar5 & 0xFFFFFFF0;
    info.bus = dev.bus;
    info.slot = dev.slot;
    info.func = dev.func;
    info.encontrado = 1; // Mudado de true para 1

    // Ativa Bus Master e Memory Space via barramento_pci
    uint32_t pci_cmd = pci_read_config_info(dev.bus, dev.slot, dev.func, 0x04);
    pci_write_config_info(dev.bus, dev.slot, dev.func, 0x04, pci_cmd | 0x06);

    terminal_print("AHCI_GERENCIADOR: Controladora SATA vinculada com sucesso!\n");
    return info;
}
