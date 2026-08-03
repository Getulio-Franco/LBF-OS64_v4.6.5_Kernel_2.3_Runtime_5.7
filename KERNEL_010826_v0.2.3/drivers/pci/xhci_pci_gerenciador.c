#include "xhci_pci_gerenciador.h"
#include "barramento_pci.h"
#include "../video.h"
#include "../../mem/pmm.h"
#include "../../mem/vmm.h"
#include "../../util/string.h"

// Definição das globais exclusivas do gerenciador
uint64_t xhci_base_phys_info = 0;
uint64_t xhci_base_virt_info = 0;
volatile xhci_regs_t* xhci_regs_info = 0;
volatile xhci_intr_regs_t* xhci_intr_info = 0; 
volatile uint32_t* xhci_doorbells_info = 0;

int xhci_pci_init_info(void) {
    pci_device_t dev;

    // Ajustado para checar explicitamente se o retorno numérico indica falha (0)
    if (pci_procurar_dispositivo(0x0C, 0x03, 0x30, &dev) == 0) {
        return 0; 
    }

    uint32_t bar0_low = pci_read_config_info(dev.bus, dev.slot, dev.func, 0x10);
    xhci_base_phys_info = bar0_low & 0xFFFFFFF0;
    
    if ((bar0_low & 0x06) == 0x04) {
        uint64_t bar0_high = pci_read_config_info(dev.bus, dev.slot, dev.func, 0x14);
        xhci_base_phys_info |= (bar0_high << 32);
    }

    if (xhci_base_phys_info == 0) return 0;

    uint32_t pci_cmd = pci_read_config_info(dev.bus, dev.slot, dev.func, 0x04);
    pci_write_config_info(dev.bus, dev.slot, dev.func, 0x04, pci_cmd | 0x06);

    vmm_map_page_2mb_ext(xhci_base_phys_info, xhci_base_phys_info, PAGE_PRESENT | PAGE_WRITE); 
    xhci_base_virt_info = xhci_base_phys_info;

    uint32_t cap_reg_bruto = *(volatile uint32_t*)(xhci_base_virt_info);
    uint8_t cap_len = cap_reg_bruto & 0xFF;
    if (cap_len == 0 || cap_len == 0xFF) return 0;
    
    xhci_regs_info = (volatile xhci_regs_t*)(xhci_base_virt_info + cap_len);

    uint32_t db_offset = *(volatile uint32_t*)(xhci_base_virt_info + 0x14);
    xhci_doorbells_info = (volatile uint32_t*)(xhci_base_virt_info + db_offset);

    uint32_t rt_offset = *(volatile uint32_t*)(xhci_base_virt_info + 0x18);
    xhci_intr_info = (volatile xhci_intr_regs_t*)(xhci_base_virt_info + rt_offset + 0x20);

    terminal_print("xHCI_GERENCIADOR: Informacoes xHCI (USB 3.0) mapeadas com sucesso.\n");
    return 1;
}
