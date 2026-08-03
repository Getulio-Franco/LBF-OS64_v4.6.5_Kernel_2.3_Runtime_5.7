#include "ahci_mem.h"
#include "../../util/string.h"
#include "../../drivers/video.h"

// Estruturas internas para o cálculo de tamanhos
typedef struct {
    uint32_t cfl:5;
    uint32_t a:1;
    uint32_t w:1;
    uint32_t p:1;
    uint32_t r:1;
    uint32_t b:1;
    uint32_t c:1;
    uint32_t rsv0:1;
    uint32_t pmp:4;
    uint32_t prdtl:16;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv1[4];
} __attribute__((packed)) ahci_cmd_header_t;

bool ahci_mem_configurar_porta(volatile ahci_port_reg_t* port, int port_no) {
    // 1. Garante que os motores da porta estão desligados para aceitar a nova memória
    port->cmd &= ~(1 << 0);  // Limpa ST
    port->cmd &= ~(1 << 4);  // Limpa FRE
    
    uint32_t timeout = 100000;
    while (timeout--) {
        if (!(port->cmd & ((1 << 15) | (1 << 14)))) break;
        __asm__ volatile("pause");
    }
    if (timeout == 0) return false;

    // 2. Calcula a base de memória exclusiva para esta porta (1MB de espaçamento garante alinhamento de 4KB)
    uint64_t porta_mem_base = AHCI_MEM_SAFE_BASE + (port_no * 0x100000);

    // 3. Aloca a Command List (Exige alinhamento de 1KB. Nossa base por ser múltipla de 4KB já atende)
    uint64_t clb_phys = porta_mem_base;
    void* clb_virt = (void*)(uintptr_t)clb_phys;
    memset(clb_virt, 0, 1024); // Limpa os 1024 bytes da Command List (32 slots * 32 bytes)

    port->clb = (uint32_t)(clb_phys & 0xFFFFFFFF);
    port->clbu = (uint32_t)(clb_phys >> 32);

    // 4. Aloca o FIS Receive Area (Exige alinhamento de 256 bytes. Colocamos no deslocamento de 4KB)
    uint64_t fb_phys = porta_mem_base + 4096;
    void* fb_virt = (void*)(uintptr_t)fb_phys;
    memset(fb_virt, 0, 256); // Limpa os 256 bytes do buffer de recebimento do FIS

    port->fb = (uint32_t)(fb_phys & 0xFFFFFFFF);
    port->fbu = (uint32_t)(fb_phys >> 32);

    // 5. Aloca as Command Tables para cada um dos 32 slots (Exige alinhamento de 128 bytes)
    // Vamos posicioná-las a partir do deslocamento de 8KB (8192)
    ahci_cmd_header_t* cmd_header = (ahci_cmd_header_t*)clb_virt;
    uint64_t ct_phys_base = porta_mem_base + 8192;

    for (int slot = 0; slot < 32; slot++) {
        // Cada tabela de comando ocupa 256 bytes (alinhamento perfeito de 128 bytes mantido)
        uint64_t ct_phys = ct_phys_base + (slot * 256);
        void* ct_virt = (void*)(uintptr_t)ct_phys;
        memset(ct_virt, 0, 256);

        cmd_header[slot].ctba = (uint32_t)(ct_phys & 0xFFFFFFFF);
        cmd_header[slot].ctbau = (uint32_t)(ct_phys >> 32);
        cmd_header[slot].prdtl = 1; // Vamos configurar para suportar 1 entrada PRDT por comando
        cmd_header[slot].cfl = 5;   // Tamanho do comando FIS padrão (5 dwords = 20 bytes)
    }

    // 6. Ativa os mecanismos de energia exigidos pelo VirtualBox para acordar os motores físicos
    port->cmd |= (1 << 23); // Bit POD (Power On Device)
    port->cmd |= (1 << 1);  // Bit SUD (Spin Up Device)
    __asm__ volatile("mfence");

    // 7. Reativa o motor de recebimento do FIS (FRE) - Obrigatório antes de ligar o ST
    port->cmd |= (1 << 4);  // Seta FRE
    __asm__ volatile("mfence");

    // 8. Reativa o motor de execução de comandos (ST)
    port->cmd |= (1 << 0);  // Seta ST
    __asm__ volatile("mfence");

    char log_msg[64];
    strcpy(log_msg, "AHCI_MEM: Rebase concluido e motores ligados na porta ");
    char num_str[4];
    itoa(port_no, num_str, 10);
    strcat(log_msg, num_str);
    strcat(log_msg, "\n");
    terminal_print(log_msg);

    return true;
}
