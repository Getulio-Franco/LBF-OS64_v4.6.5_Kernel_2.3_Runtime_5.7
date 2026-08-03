#include "ehci_pci.h"
#include "../video.h"          
#include "../io.h"             
#include "../../util/string.h" 
#include "../../mem/vmm.h"     

uintptr_t ehci_base_virt = 0;
volatile ehci_regs_t* ehci_regs = 0;

static uint32_t local_pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

static void local_pci_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

int ehci_pci_init(void) {
    terminal_print("EHCI_PCI: Buscando controladora no Barramento PCI...\n");

    for (uint8_t slot = 0; slot < 32; slot++) {
        for (uint8_t func = 0; func < 8; func++) {
            uint32_t vendor_device = local_pci_read(0, slot, func, 0);
            if ((vendor_device & 0xFFFF) == 0xFFFF) continue; 

            uint32_t class_rev = local_pci_read(0, slot, func, 0x08);
            uint8_t classe    = (class_rev >> 24) & 0xFF;
            uint8_t subclasse = (class_rev >> 16) & 0xFF;
            uint8_t interface = (class_rev >> 8)  & 0xFF;

            if (classe == 0x0C && subclasse == 0x03 && interface == 0x20) {
                terminal_print("EHCI_PCI: Controlador EHCI (USB 2.0) valido encontrado!\n");

                // Ativa a placa no Barramento PCI (Bus Master e Memory Space)
                uint32_t pci_cmd = local_pci_read(0, slot, func, 0x04);
                pci_cmd |= (1 << 1) | (1 << 2); 
                local_pci_write(0, slot, func, 0x04, pci_cmd);

                // Captura a BAR0 (Endereço físico)
                uint32_t bar0 = local_pci_read(0, slot, func, 0x10);
                uintptr_t ehci_base_phys = bar0 & 0xFFFFFFF0;

                if (ehci_base_phys == 0) {
                    terminal_print("EHCI_PCI: Erro fatal! BAR0 retornou zero.\n");
                    return 0;
                }

                // Mapeamento na MMU com Cache Disable
                uint64_t phys_page = (uint64_t)ehci_base_phys & ~0xFFFULL;
                uint64_t virt_page = phys_page;
                uint64_t flags = PAGE_PRESENT | PAGE_PCD | PAGE_PWT;

                vmm_map_area(virt_page, phys_page, 4096, flags);
                ehci_base_virt = (uintptr_t)(virt_page | (ehci_base_phys & 0xFFF));
                
                terminal_print("EHCI_PCI: BAR0 USB mapeada para a MMU com Cache Disable!\n");

                // Sincronização dinâmica com os registradores operacionais
                volatile uint8_t* cap_regs = (volatile uint8_t*)ehci_base_virt;
                uint8_t cap_length = cap_regs[0]; 

                ehci_regs = (volatile ehci_regs_t*)(ehci_base_virt + cap_length);

                terminal_print("EHCI_PCI: Sincronizacao com registradores operacionais OK!\n");
                return 1;
            }
        }
    }
    terminal_print("EHCI_PCI: Varredura finalizada sem controladoras EHCI.\n");
    return 0;
}
