#ifndef EHCI_PCI_GERENCIADOR_H
#define EHCI_PCI_GERENCIADOR_H

#include <stdint.h>

typedef struct {
    volatile uint32_t usbcmd;           
    volatile uint32_t usbsts;           
    volatile uint32_t usbintr;          
    volatile uint32_t frindex;          
    volatile uint32_t ctrldssegment;    
    volatile uint32_t periodiclistbase; 
    volatile uint32_t asynclistaddr;    
    uint32_t reserved[9];               
    volatile uint32_t configflag;       
    volatile uint32_t portsc[15];       
} __attribute__((packed)) ehci_regs_t;

// Variáveis globais renomeadas com sufixo exclusivo para o Gerenciador de Dispositivos
extern uintptr_t ehci_base_virt_info;
extern volatile ehci_regs_t* ehci_regs_info;

int ehci_pci_init_info(void);

#endif // EHCI_PCI_GERENCIADOR_H
