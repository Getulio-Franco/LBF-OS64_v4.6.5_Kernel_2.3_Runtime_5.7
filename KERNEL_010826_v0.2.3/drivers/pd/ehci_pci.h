#ifndef EHCI_PCI_H
#define EHCI_PCI_H

#include <stdint.h>

// Estrutura física dos registradores operacionais do EHCI
// Melhoria: Portas embutidas diretamente como um vetor (array) na struct
typedef struct {
    volatile uint32_t usbcmd;           // 0x00 - Comando USB
    volatile uint32_t usbsts;           // 0x04 - Status USB
    volatile uint32_t usbintr;          // 0x08 - Interrupções
    volatile uint32_t frindex;          // 0x0C - Índice de Frame
    volatile uint32_t ctrldssegment;    // 0x10 - Segmento de 64-bits alto
    volatile uint32_t periodiclistbase; // 0x14 - Lista Periódica Base
    volatile uint32_t asynclistaddr;    // 0x18 - Lista Assíncrona Base
    uint32_t reserved[9];               // 0x1C até 0x3F - Reservado
    volatile uint32_t configflag;       // 0x40 - Flag de Configuração de Roteamento
    volatile uint32_t portsc[15];       // 0x44 em diante - Registradores de Controle de cada Porta
} __attribute__((packed)) ehci_regs_t;

// Variáveis globais de controle
extern uintptr_t ehci_base_virt;
extern volatile ehci_regs_t* ehci_regs;

// Protótipo principal de varredura
int ehci_pci_init(void);

#endif // EHCI_PCI_H
