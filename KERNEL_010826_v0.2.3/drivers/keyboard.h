/**
 * ============================================================================
 * KEYBOARD.H - PS/2 DRIVER INTERFACE - V3.1 (LFB/VESA COMPATIBLE)
 * ============================================================================
 * Suporte a ABNT2, Buffer Circular Atômico e Integração com VFS.
 * ============================================================================
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "drivers/vfs.h"

// --- DEFINIÇÕES DE CONTROLE ---
#define KEYBOARD_BUFFER_SIZE 256

/**
 * @brief Nó de dispositivo para o sistema de arquivos virtual (VFS).
 * Permite que o teclado seja acessado como um arquivo (stdin / fd 0).
 */
extern vfs_node_t keyboard_device_node;

/**
 * @brief Inicializa o driver de teclado.
 * Reseta o buffer circular e os estados de modificadores (Shift/Caps).
 */
void keyboard_init(void);

/**
 * @brief Verifica se há algum caractere aguardando no buffer.
 * Essencial para evitar busy-waiting excessivo em chamadas de sistema.
 * @return 1 se houver dados, 0 se estiver vazio.
 */
int keyboard_available(void);

/**
 * @brief Extrai e remove o próximo caractere disponível no buffer.
 * @return O caractere em ASCII ou 0 se o buffer estiver vazio.
 */
char keyboard_pop_char(void);

/**
 * @brief Handler de interrupção (IRQ1).
 * Chamado pela IDT para processar scancodes brutos do hardware.
 */
void keyboard_handler(void);

/**
 * @brief Interface de leitura padrão para o VFS.
 * @note Implementa o bloqueio de processo caso o buffer esteja vazio.
 */
uint32_t keyboard_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);

#endif // KEYBOARD_H
