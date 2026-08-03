#include "ahci_reset.h"
#include "../../drivers/video.h"
#include "../../util/string.h"

// Função auxiliar para parar os motores de comando antes do reset
static void parar_comandos_porta(volatile ahci_port_reg_t* port) {
    port->cmd &= ~(1 << 0);  // Limpa o bit ST (Start)
    port->cmd &= ~(1 << 4);  // Limpa o bit FRE (FIS Receive Enable)

    // Aguarda o hardware confirmar que os motores pararam (CR e FR limpos)
    uint32_t timeout = 100000;
    while (timeout--) {
        if (!(port->cmd & ((1 << 15) | (1 << 14)))) break;
        __asm__ volatile("pause");
    }
}

int ahci_reset_controladora(void* abar_virt) {
    volatile ahci_hba_reg_t* hba = (volatile ahci_hba_reg_t*)abar_virt;

    // 1. Ativa o modo AHCI nativo (Global Host Control - Bit 31)
    hba->ghc |= (1ULL << 31);
    __asm__ volatile("mfence");

    // 2. Handshake BIOS/OS (BIOS Old Handoff Control) se suportado
    if (hba->cap2 & (1 << 0)) { // Verifica bit BOH no CAP2
        hba->bohc |= (1 << 1);  // Requere propriedade do OS (OOS)
        uint32_t timeout = 1000000;
        while ((hba->bohc & (1 << 0)) && timeout--) { // Espera BIOS liberar (BOS)
            __asm__ volatile("pause");
        }
    }

    int portas_ativas = 0;
    uint32_t pi = hba->pi; // Bitmask das portas implementadas no hardware

    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            volatile ahci_port_reg_t* port = &hba->ports[i];

            // 3. Reseta a máquina de estados da porta de forma limpa
            parar_comandos_porta(port);

            // 4. Força COMRESET elétrico via SControl (SCTL)
            // Detecção Física: Seta bit 0..3 para 1 (Gera reset na linha)
            port->sctl = (port->sctl & ~0x0F) | 0x01;
            __asm__ volatile("mfence");

            // Aguarda o pulso elétrico se estabilizar (cerca de 1 milissegundo)
            for (volatile int delay = 0; delay < 200000; delay++) { __asm__ volatile("pause"); }

            // Remove o sinal de COMRESET para iniciar a negociação normal
            port->sctl &= ~0x0F;
            __asm__ volatile("mfence");

            // 5. Verifica se o link físico estabilizou
            uint32_t timeout = 500000;
            while (timeout--) {
                // Se detecção (SSTS bits 0..3) == 3 (Device present e comunicação estabelecida)
                if ((port->ssts & 0x0F) == 0x03) {
                    // Limpa erros residuais gerados pelo reset elétrico
                    port->serr = 0xFFFFFFFF;
                    port->is = 0xFFFFFFFF;
                    __asm__ volatile("mfence");

                    // Verifica se a assinatura bate com um drive de dados SATA (0x00000101)
                    if (port->sig == 0x00000101) {
                        portas_ativas++;
                        char log_msg[64];
                        strcpy(log_msg, "AHCI_RESET: Disco SATA detectado na porta ");
                        char num_str[4];
                        itoa(i, num_str, 10);
                        strcat(log_msg, num_str);
                        strcat(log_msg, "\n");
                        terminal_print(log_msg);
                    }
                    break;
                }
                __asm__ volatile("pause");
            }
        }
    }

    return portas_ativas;
}
