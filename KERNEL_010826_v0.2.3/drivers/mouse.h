#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

// Definições de Botões (Máscaras de Bits)
#define MOUSE_BTN_LEFT   0x01
#define MOUSE_BTN_RIGHT  0x02
#define MOUSE_BTN_MIDDLE 0x04

// Variáveis Globais de Posição (Para consulta do GUI/Desktop)
extern int mouse_x;
extern int mouse_y;

// Funções de Inicialização e Comunicação com Hardware
void mouse_init(void);
void mouse_handler(void); // Chamada pela IRQ12

// Função de Calibração de Tela (Chamar quando mudar resolução)
void mouse_set_screen_size(int width, int height);

/**
 * Nota Técnica: 
 * A função mouse_handler_it foi absorvida pela lógica interna 
 * do mouse_handler() no seu novo mouse.c. Se você não a usa 
 * externamente, ela pode ser removida do Header.
 */

// Protótipos de funções de porta I/O (Geralmente definidas no seu assembly ou io.c)
//extern void outb(uint16_t port, uint8_t val);
//extern uint8_t inb(uint16_t port);

#endif
