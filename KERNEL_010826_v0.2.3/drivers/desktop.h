#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>

/* --- Estruturas de Dados --- */

typedef struct {
    int x;          
    int y;          
    uint8_t show;   
    uint8_t buttons;      
} mouse_cursor_t;

typedef struct {
    int width;                  
    int height;                 
    mouse_cursor_t mouse;       
} desktop_t;

/* --- Variáveis Globais de Controle --- */

extern desktop_t ctx;
extern volatile int mouse_needs_update;
extern int refresh_screen;

/* --- Funções de Ciclo de Vida e Hardware --- */

// Inicializa coordenadas e limites da tela
void desktop_init(int w, int h);

// Recebe dados brutos do driver de mouse e repassa para o wm_handle_mouse
void desktop_update_mouse(int delta_x, int delta_y, uint8_t buttons, int8_t scroll);

// Apenas sinaliza que a tela precisa ser redesenhada
void desktop_redraw(void);

#endif
