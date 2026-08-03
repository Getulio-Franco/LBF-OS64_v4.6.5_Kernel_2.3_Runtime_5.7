/**
 * ============================================================================
 * TIMER.H - PIT TIMER INTERFACE - V3.0 (LFB/VESA COMPATIBLE)
 * ============================================================================
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* --- CONFIGURAÇÕES DO HARDWARE PIT (Intel 8253/8254) --- */

#define PIT_DATA_PORT_0    0x40    // Canal 0: Ligado à IRQ0
#define PIT_COMMAND_PORT   0x43    // Porta de controle do modo/frequência
#define PIT_BASE_FREQ      1193182 // Frequência base em Hz

/* --- PROTÓTIPOS DAS FUNÇÕES --- */

/**
 * @brief Inicializa o PIT para uma frequência específica.
 * @param frequency Frequência em Hz (Ex: 100 ou 1000).
 */
void timer_init(uint32_t frequency);

/**
 * @brief Handler C para a interrupção do relógio (IRQ0).
 * Faz a ponte com o scheduler e atualiza o TSS para suporte a Ring 3.
 * @param rsp O Stack Pointer da tarefa interrompida.
 * @return O Stack Pointer da próxima tarefa.
 */
uint64_t timer_handler(uint64_t rsp);

/**
 * @brief Retorna a contagem total de tiques (ticks) desde o boot.
 */
uint64_t timer_get_ticks();

/**
 * @brief Realiza uma espera (delay) bloqueante baseada em tiques.
 * Utiliza a instrução 'hlt' para economizar CPU.
 */
void timer_wait(uint64_t ticks);

/**
 * @brief Lê o tempo atual através do RTC/CMOS.
 */
uint64_t read_cmos_time(void);

#endif // TIMER_H
