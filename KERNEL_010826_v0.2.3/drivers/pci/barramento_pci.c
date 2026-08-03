#include "barramento_pci.h"
#include "../io.h"      
#include "../video.h"   
#include "ahci_pci_gerenciador.h"
#include "ehci_pci_gerenciador.h"
#include "xhci_pci_gerenciador.h"
#include "serial_pci_gerenciador.h"

pci_device_t pci_device_list[MAX_PCI_DEVICES];
int pci_device_count = 0;

void terminal_print_hex16(uint16_t val) {
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

void terminal_print_hex8(uint8_t val) {
    char buf[5];
    char hex_chars[] = "0123456789ABCDEF";
    buf[0] = '0'; buf[1] = 'x';
    buf[2] = hex_chars[(val >> 4) & 0xF];
    buf[3] = hex_chars[val & 0xF];
    buf[4] = '\0';
    terminal_print(buf);
}

// Removido o _info para casar com o cabeçalho e chamadas dos drivers
uint32_t pci_read_config_info(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

// Removido o _info para casar com o cabeçalho e chamadas dos drivers
void pci_write_config_info(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | 0x80000000);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

// Nome padronizado como pci_procurar_dispositivo
// Altere a assinatura e os retornos para inteiros (1 e 0)
uint8_t pci_procurar_dispositivo(uint8_t class_id, uint8_t subclass_id, int prog_if_filtro, pci_device_t* dev_out) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_device_list[i].class_id == class_id && pci_device_list[i].subclass_id == subclass_id) {
            if (prog_if_filtro == -1 || pci_device_list[i].prog_if == (uint8_t)prog_if_filtro) {
                *dev_out = pci_device_list[i];
                return 1; // Sucesso
            }
        }
    }
    return 0; // Falha
}

void pci_iniciar_barramento(void) {
    terminal_print("PCI: Escaneando barramento de hardware...\n");
    pci_device_count = 0;

    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint32_t vendor_device = pci_read_config_info(bus, slot, func, 0);
                if ((vendor_device & 0xFFFF) == 0xFFFF) continue; 

                uint32_t reg08 = pci_read_config_info(bus, slot, func, 0x08);

                if (pci_device_count < MAX_PCI_DEVICES) {
                    pci_device_t* dev = &pci_device_list[pci_device_count];
                    dev->bus = bus;
                    dev->slot = slot;
                    dev->func = func;
                    dev->vendor_id = (uint16_t)(vendor_device & 0xFFFF);
                    dev->device_id = (uint16_t)(vendor_device >> 16);
                    dev->class_id = (uint8_t)(reg08 >> 24);
                    dev->subclass_id = (uint8_t)(reg08 >> 16);
                    dev->prog_if = (uint8_t)(reg08 >> 8);
                    dev->encontrado = true;

                    terminal_print("  Dev [");
                    terminal_print_hex8(dev->bus); terminal_print(":");
                    terminal_print_hex8(dev->slot); terminal_print("] Class ");
                    terminal_print_hex8(dev->class_id); terminal_print(" Sub ");
                    terminal_print_hex8(dev->subclass_id); terminal_print(" Ven ");
                    terminal_print_hex16(dev->vendor_id); terminal_print("\n");

                    pci_device_count++;
                }
            }
        }
    }
    terminal_print("PCI: Mapeando informacoes para o Gerenciador de Dispositivos...\n");
    
    ahci_pci_detectar_info();
    ehci_pci_init_info();
    xhci_pci_init_info();
    serial_pci_init_info();
}
