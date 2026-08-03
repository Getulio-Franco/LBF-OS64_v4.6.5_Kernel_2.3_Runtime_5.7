#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/* --- Endereços de Portas dos PICs --- */
#define PIC1_COMMAND 0x0020
#define PIC1_DATA    0x0021
#define PIC2_COMMAND 0x00A0
#define PIC2_DATA    0x00A1

/* --- Comandos e Flags de Inicialização --- */
#define PIC_EOI      0x20    /* End of Interrupt (Fim de Interrupção) */

#define ICW1_ICW4    0x01    /* Indica que ICW4 será enviado */
#define ICW1_SINGLE  0x02    /* Modo único (0 = Modo Cascata) */
#define ICW1_INTERVAL4 0x04  /* Intervalo de vetor 4 (0 = 8) */
#define ICW1_LEVEL   0x08    /* Modo engatilhado por nível (0 = borda) */
#define ICW1_INIT    0x10    /* Bit obrigatório de inicialização */

#define ICW4_8086    0x01    /* Modo Microprocessador 8086/88 */
#define ICW4_AUTO    0x02    /* Fim de interrupção automático */
#define ICW4_BUF_SLAVE 0x08  /* Modo bufferizado (Slave) */
#define ICW4_BUF_MASTER 0x0C /* Modo bufferizado (Master) */
#define ICW4_SFNM    0x10    /* Modo especial totalmente aninhado */

/* --- Linhas IRQ Mapeadas Comumente --- */
#define IRQ_TIMER    0
#define IRQ_KEYBOARD 1
#define IRQ_CASCADE  2
#define IRQ_SERIAL2  3
#define IRQ_SERIAL1  4
#define IRQ_LPT2     5
#define IRQ_FLOPPY   6
#define IRQ_LPT1     7
#define IRQ_CMOS     8
#define IRQ_MOUSE    12
#define IRQ_ATA_PRM  14      /* IDE/SATA Primário */
#define IRQ_ATA_SEC  15      /* IDE/SATA Secundário */

/* --- Protótipos das Funções Mês 3 --- */
void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);

#endif
