#ifndef XHCI_PCI_GERENCIADOR_H
#define XHCI_PCI_GERENCIADOR_H

#include <stdint.h>

typedef struct {
    uint32_t iman;          
    uint32_t imod;          
    uint32_t erstsz;        
    uint32_t rsvd;          
    uint64_t erstba;        
    uint64_t erdp;          
} __attribute__((packed)) xhci_intr_regs_t;

typedef struct {
    uint32_t usbcmd;        
    uint32_t usbsts;        
    uint32_t pagesize;      
    uint32_t rsv0[2];       
    uint32_t dnctrl;        
    uint64_t crcr;          
    uint32_t rsv1[4];       
    uint64_t dcbaap;        
    uint32_t config;        
} __attribute__((packed)) xhci_regs_t;

// Variáveis globais isoladas com o sufixo _info para não colidir com o driver principal
extern uint64_t xhci_base_phys_info;
extern uint64_t xhci_base_virt_info;
extern volatile xhci_regs_t* xhci_regs_info;
extern volatile xhci_intr_regs_t* xhci_intr_info;
extern volatile uint32_t* xhci_doorbells_info;

int xhci_pci_init_info(void);

#endif // XHCI_PCI_GERENCIADOR_H
