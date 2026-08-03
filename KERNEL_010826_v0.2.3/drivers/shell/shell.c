#include "shell.h"
#include "shell_prompt.h"
#include "shell_commands.h"
#include "util/string.h"
#include "drivers/keyboard.h"
#include "drivers/proc.h"
#include "drivers/video.h" // Necessário para terminal_putc

#define CMD_BUFFER_SIZE 128

/**
 * @brief Imprime uma string no terminal.
 * Removido o 'static' para coincidir com a declaração em shell.h
 */
void shell_print(const char* str) {
    if (!str) return;
    while (*str) terminal_putc(*str++);
}

/**
 * @brief Imprime um caractere no terminal.
 */
void shell_print_char(char c) {
    terminal_putc(c);
}

/**
 * @brief Loop principal da Shell - Foco total em processamento de comandos.
 */
void shell_task() {
    static char cmd_buffer[CMD_BUFFER_SIZE];
    int idx = 0;
    
    shell_print("SISTEMA OPERACIONAL 64-BIT - v4.0 [Kernel_v0.1.4 22 07 26]\n");

    while(1) {
       // Exibe o prompt (ex: /root>)
        shell_print(get_current_path());
        shell_print(">");

        idx = 0;
        memset(cmd_buffer, 0, CMD_BUFFER_SIZE);

        while(1) {
            // Verifica se este processo (Shell) tem o foco do teclado
            process_t* me = get_current_process();
            if (me != NULL && me->is_foreground == 0) {
                __asm__ volatile("pause");
                continue; 
            }

            // Captura entrada do driver de teclado (agora agnóstico à interface)
            char c = keyboard_pop_char(); // DESABILITA O KEYBOARD PARA O TERMINAL PARA O MODO GRAFICO FUNCIONAR
           
            if (c == 0) {
              sys_sleep(10); // Dá um descanso para o Kernel e evita spam
              continue;
            }
            
            if (c != 0) { 
                // ENTER: Finaliza o comando e executa
                if (c == '\n' || c == '\r') {
                    cmd_buffer[idx] = '\0'; 
                    terminal_putc('\n');
                    if (strlen(cmd_buffer) > 0) {
                        shell_execute_command(cmd_buffer); 
                    }
                    break; // Sai do loop de leitura para imprimir o prompt novamente
                } 
                // BACKSPACE: Apaga o último caractere
                else if (c == '\b' && idx > 0) {
                    idx--; 
                    terminal_putc('\b');
                }
                // CARACTERES IMPRIMÍVEIS
                else if (idx < (CMD_BUFFER_SIZE - 1) && c >= 32 && c <= 126) {
                    cmd_buffer[idx++] = c;
                    terminal_putc(c);
                }
            }
            // Pequena pausa para não fritar a CPU no polling
            __asm__ volatile("pause");
        }
    }
}
