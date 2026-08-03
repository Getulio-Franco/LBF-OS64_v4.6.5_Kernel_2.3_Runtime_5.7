#include "pci.h"
#include "../io.h"      // Ajuste o caminho
#include "../video.h"   // Ajuste o caminho

/**
 * Função interna para printar Hex no terminal com scroll
 */
static void terminal_print_hex16(uint16_t val) {
    char buf[7];
    char hex_chars[] = "0123456789ABCDEF";
    buf[0] = '0'; buf[1] = 'x';
    buf[2] = hex_chars[(val >> 12) & 0xF];
    buf[3] = hex_chars[(val >> 8) & 0xF];
    buf[4] = hex_chars[(val >> 4) & 0xF];
    buf[5] = hex_chars[val & 0xF];
    buf[6] = '\0';
    terminal_print(buf);
}

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));

    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));

    outl(0xCF8, address);
    outl(0xCFC, value);
}

pci_device_t pci_find_device(uint8_t class_id, uint8_t subclass_id) {
    pci_device_t dev;
    dev.found = 0;

    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                
                uint32_t vendor_device = pci_read_config(bus, slot, func, 0);
                if ((vendor_device & 0xFFFF) == 0xFFFF) continue; // Slot vazio

                // Offset 0x08 contém: [Class|Subclass|ProgIF|Revision]
                uint32_t reg08 = pci_read_config(bus, slot, func, 0x08);
                uint8_t current_class = (uint8_t)(reg08 >> 24);
                uint8_t current_subclass = (uint8_t)(reg08 >> 16);

                if (current_class == class_id && current_subclass == subclass_id) {
                    dev.bus = bus;
                    dev.slot = slot;
                    dev.func = func;
                    dev.vendor_id = (uint16_t)(vendor_device & 0xFFFF);
                    dev.device_id = (uint16_t)(vendor_device >> 16);
                    dev.found = 1;
                    
                    // Log VESA integrado com Scroll
                    terminal_print("PCI: Achou Classe 0x");
                    char class_buf[3] = { "0123456789ABCDEF"[(class_id>>4)&0xF], "0123456789ABCDEF"[class_id&0xF], '\0' };
                    terminal_print(class_buf);
                    terminal_print(" -> Vendor ");
                    terminal_print_hex16(dev.vendor_id);
                    terminal_print(" Device ");
                    terminal_print_hex16(dev.device_id);
                    terminal_print("\n");
                    
                    return dev;
                }
            }
        }
    }
    return dev;
}
