#ifndef WINDOW_DRAG_H
#define WINDOW_DRAG_H

#include "gui.h"

/**
 * @brief Tenta iniciar o processo de arraste de uma janela.
 * Verifica se o clique ocorreu na área da barra de título.
 * * @param f Ponteiro para o formulário (janela) alvo.
 * @param mx Posição X absoluta do mouse.
 * @param my Posição Y absoluta do mouse.
 */
void drag_check_start(TForm* f, int mx, int my);

/**
 * @brief Atualiza a posição da janela que está sendo arrastada.
 * Deve ser chamada enquanto o botão do mouse estiver pressionado.
 * * @param mx Nova posição X absoluta do mouse.
 * @param my Nova posição Y absoluta do mouse.
 */
void drag_update(int mx, int my);

/**
 * @brief Finaliza qualquer operação de arraste em curso.
 * Reseta as flags IsDragging e limpa o ponteiro global de arraste.
 */
void drag_stop(void);

#endif // WINDOW_DRAG_H
