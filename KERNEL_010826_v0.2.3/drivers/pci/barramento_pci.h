#ifndef BARRAMENTO_PCI_H
#define BARRAMENTO_PCI_H

#include <stdint.h>

#define MAX_PCI_DEVICES 64

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t prog_if;
    uint8_t encontrado;
} __attribute__((packed)) pci_device_t;

extern pci_device_t pci_device_list[MAX_PCI_DEVICES];
extern int pci_device_count;

uint32_t pci_read_config_info(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config_info(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
void pci_iniciar_barramento(void);

// Alterado de bool para uint8_t para casar com a remoção do stdbool.h
uint8_t pci_procurar_dispositivo(uint8_t class_id, uint8_t subclass_id, int prog_if_filtro, pci_device_t* dev_out);

#endif // BARRAMENTO_PCI_H
