#ifndef Z_ORDER_H
#define Z_ORDER_H

#include "gui.h"

/**
 * Inicializa a pilha de janelas (Z-Order).
 * Deve ser chamado uma única vez no boot da GUI.
 */
void z_order_init();

/**
 * Adiciona uma nova janela ao topo da pilha.
 */
void z_order_add(TForm* form);

/**
 * Remove todas as janelas pertencentes a um PID específico.
 * Utilizado pelo process_reaper quando um processo termina.
 */
void z_order_remove_by_pid(int pid);

/**
 * Remove uma instância específica de formulário da pilha.
 */
void z_order_remove_form(TForm* form);

/**
 * Move o formulário especificado para o topo visual (fim do array).
 */
void z_order_bring_to_front(TForm* form);

/**
 * Retorna a quantidade atual de janelas na pilha.
 */
int z_order_get_count();

/**
 * Retorna o formulário em um índice específico.
 * Útil para o loop de redesenho (Algoritmo do Pintor).
 */
TForm* z_order_get_at(int index);

/**
 * Busca qual janela está na coordenada (x, y), do topo para o fundo.
 * Retorna NULL se clicar no papel de parede.
 */
TForm* z_order_find_at(int x, int y);

#endif
