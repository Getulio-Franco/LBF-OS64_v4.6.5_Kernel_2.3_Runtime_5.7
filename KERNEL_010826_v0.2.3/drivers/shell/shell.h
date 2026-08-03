#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

/**
 * @brief Ponto de entrada da tarefa Shell.
 * Deve ser chamada pelo escalonador (scheduler) como um processo/thread.
 */
void shell_task();

/**
 * @brief Executa uma string de comando.
 * @param cmd Ponteiro para a string contendo o comando e argumentos.
 */
void shell_execute_command(char* cmd);

/**
 * @brief Recupera a Shell de um estado de erro, limpando buffers.
 */
void shell_recover();

/**
 * @brief Funções de saída de texto exclusivas para o contexto da Shell.
 * (Pode mapear internamente para o terminal_putc ou drivers de vídeo).
 */
void shell_print(const char* str);
void shell_print_char(char c);

/**
 * @brief Retorna o tempo do sistema em milissegundos.
 * Movido para cá ou para um timer.h para uso geral.
 */
unsigned long timer_get_ms();

#endif // SHELL_H
