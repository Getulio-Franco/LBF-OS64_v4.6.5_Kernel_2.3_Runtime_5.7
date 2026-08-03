#ifndef INPUT_EVENTS_H
#define INPUT_EVENTS_H

#include <stdint.h>

// Processa as coordenadas e botões brutos do mouse vindo do driver/sistema
void events_process_mouse(int x, int y, uint8_t buttons, int8_t scroll);

// Processa a tecla vinda do driver de teclado
void events_process_keyboard(char key);

#endif
