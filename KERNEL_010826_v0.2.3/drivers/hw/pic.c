#include "pic.h"
#include "../io.h"       // Ajustado para o diretório pai (drivers/io.h)
#include "../video.h"    // Sistema de log visual VESA

/**
 * @brief Inicializa e remapeia os vetores de interrupção dos PICs Master e Slave.
 */
void pic_init(void) {
    // Guarda as máscaras atuais para não quebrar estados anteriores se re-inicializado
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    // 1. Inicia a sequência de inicialização em modo cascata (ICW1)
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // 2. Define o deslocamento dos vetores na IDT (ICW2)
    // Redirecionamos os IRQs de hardware para longe das exceções da CPU (0 a 31).
    // Master IRQ 0-7   -> Vetores 0x20 a 0x27 (32-39)
    // Slave  IRQ 8-15  -> Vetores 0x28 a 0x2F (40-47)
    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    // 3. Configura o acoplamento em cascata (ICW3)
    outb(PIC1_DATA, 4);  // Master: Informa que há um Slave no IRQ2 (0100 em binário)
    io_wait();
    outb(PIC2_DATA, 2);  // Slave: Informa sua identidade de cascata ao Master (ID 2)
    io_wait();

    // 4. Define o modo de operação do ambiente de execução (ICW4)
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // 5. Configuração Segura de Máscaras Iniciais
    // Restauramos máscaras antigas se válidas ou bloqueamos por segurança.
    // Deixamos por padrão a linha IRQ2 aberta no Master para o Slave conseguir reportar.
    outb(PIC1_DATA, 0xFB); // 11111011b (Bloqueia tudo exceto IRQ2)
    outb(PIC2_DATA, 0xFF); // 11111111b (Bloqueia todas as linhas do Slave)

    terminal_print("PIC: Controladores 8259 remapeados para [0x20-0x2F].\n");
}

/**
 * @brief Envia o sinal de Fim de Interrupção (End of Interrupt - EOI) para o hardware.
 */
void pic_send_eoi(uint8_t irq) {
    // Se a interrupção veio do chip Slave (IRQ 8 a 15), precisamos resetar ambos
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    // O Master sempre recebe o sinal EOI
    outb(PIC1_COMMAND, PIC_EOI);
}

/**
 * @brief Mascara (desativa) uma linha de interrupção de hardware específica.
 */
void pic_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    // Setar o bit correspondente desativa a linha no chip
    value = inb(port) | (1 << irq);
    outb(port, value);
}

/**
 * @brief Desenquadra (ativa) uma linha de interrupção de hardware específica.
 */
void pic_unmask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    // Limpar o bit correspondente ativa a linha para escuta da CPU
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}
