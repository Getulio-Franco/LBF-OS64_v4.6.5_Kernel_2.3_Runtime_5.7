#include "drivers/hw/ps2.h"
#include "drivers/io.h"

// Importa as funções que você já escreveu nos seus arquivos nativos
extern void keyboard_init(void);
extern void mouse_init(void);

// Importa o unmask do seu driver pic.h para ativar as linhas físicas
extern void pic_unmask(uint8_t irq);

/**
 * @brief Bloqueia a execução até que o buffer do chip esteja pronto para receber dados (Input Vazio)
 */
static void ps2_wait_input_empty(void) {
    volatile uint32_t timeout = 100000;
    while ((inb(PS2_STATUS_PORT) & 0x02) && --timeout);
}

/**
 * @brief Bloqueia a execução até que o chip coloque algum dado para leitura (Output Cheio)
 */
static void ps2_wait_output_full(void) {
    volatile uint32_t timeout = 100000;
    while (!(inb(PS2_STATUS_PORT) & 0x01) && --timeout);
}

void ps2_bus_init(void) {
    // 1. Desativa ambos os canais temporariamente para o chip não misturar comandos
    ps2_wait_input_empty();
    outb(PS2_COMMAND_PORT, PS2_CMD_DISABLE_KBD);
    ps2_wait_input_empty();
    outb(PS2_COMMAND_PORT, PS2_CMD_DISABLE_MOUSE);

    // 2. Limpa qualquer lixo residual deixado pela BIOS no buffer de saída
    while (inb(PS2_STATUS_PORT) & 0x01) {
        inb(PS2_DATA_PORT);
    }

    // 3. Lê o byte de configuração do barramento (Command Byte)
    ps2_wait_input_empty();
    outb(PS2_COMMAND_PORT, PS2_CMD_READ_CONFIG);
    ps2_wait_output_full();
    uint8_t config = inb(PS2_DATA_PORT);

    // Modifica o byte de configuração:
    // Bit 0 = Habilita interrupção do Teclado (IRQ1)
    // Bit 1 = Habilita interrupção do Mouse (IRQ12)
    // Bit 6 = Ativa tradução do Scancode Set 1 (Essencial para o seu abnt2_map funcionar)
    config |= 0x43; 

    // Salva a configuração atualizada de volta no chip
    ps2_wait_input_empty();
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_CONFIG);
    ps2_wait_input_empty();
    outb(PS2_DATA_PORT, config);

    // 4. Reativa as portas físicas do chip agora que estão configuradas
    ps2_wait_input_empty();
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_KBD);
    ps2_wait_input_empty();
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_MOUSE);

    // 5. CHAMA OS SEUS DRIVERS NATIVOS DO SISTEMA OPERACIONAL
    keyboard_init();
    mouse_init();

    // 6. Desenvolve as linhas de interrupção no PIC para o processador escutar os cliques e teclas
    pic_unmask(1);  // IRQ1: Teclado
    pic_unmask(12); // IRQ12: Mouse (Slave PIC)
}
