#ifndef AHCI_RESET_H
#define AHCI_RESET_H

#include <stdint.h>
#include <stdbool.h>

// Estruturas básicas de registradores para o Reset
typedef struct {
    uint32_t clb;       // 0x00, Command list base address, 1K aligned
    uint32_t clbu;      // 0x04, Command list base address upper 32 bits
    uint32_t fb;        // 0x08, FIS base address, 256-byte aligned
    uint32_t fbu;       // 0x0C, FIS base address upper 32 bits
    uint32_t is;        // 0x10, Interrupt status
    uint32_t ie;        // 0x14, Interrupt enable
    uint32_t cmd;       // 0x18, Command and status
    uint32_t rsv0;      // 0x1C, Reserved
    uint32_t tfd;       // 0x20, Task file data
    uint32_t sig;       // 0x24, Signature
    uint32_t ssts;      // 0x28, Serial ATA status (SStatus)
    uint32_t sctl;      // 0x2C, Serial ATA control (SControl)
    uint32_t serr;      // 0x30, Serial ATA error (SError)
    uint32_t sact;      // 0x34, Serial ATA active (SActive)
    uint32_t ci;        // 0x38, Command issue
    // O resto dos registradores da porta não importam para o reset
} __attribute__((packed)) ahci_port_reg_t;

typedef struct {
    uint32_t cap;       // 0x00, Host capabilities
    uint32_t ghc;       // 0x04, Global host control
    uint32_t is;        // 0x08, Interrupt status
    uint32_t pi;        // 0x0C, Ports implemented
    uint32_t vs;        // 0x10, Version
    uint32_t ccc_ctl;   // 0x14, Command completion coalescing control
    uint32_t ccc_pts;   // 0x18, Command completion coalescing ports
    uint32_t em_loc;    // 0x1C, Enclosure management location
    uint32_t em_ctl;    // 0x20, Enclosure management control
    uint32_t cap2;      // 0x24, Host capabilities extended
    uint32_t bohc;      // 0x28, BIOS/OS coexistence control
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    ahci_port_reg_t ports[32]; // 0x100, Portas alocadas no ABAR
} __attribute__((packed)) ahci_hba_reg_t;

/**
 * @brief Reseta globalmente a controladora AHCI e testa a presença física de discos.
 * @param abar_virt Endereço virtual mapeado pelo ahci_vmm_mapear_abar.
 * @return Quantidade de portas SATA operacionais encontradas e prontas para rebase.
 */
int ahci_reset_controladora(void* abar_virt);

#endif // AHCI_RESET_H
