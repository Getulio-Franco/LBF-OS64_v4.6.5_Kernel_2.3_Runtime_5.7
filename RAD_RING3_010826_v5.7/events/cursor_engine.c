#include "cursor_engine.h"
#include "../system/graphics.h" 

#define MOUSE_W 16
#define MOUSE_H 16

static uint32_t mouse_backbuffer[MOUSE_W * MOUSE_H];

// Estados do pixel para facilitar o desenho visual da seta
#define T 0 // Transparente (Fundo da tela)
#define W 1 // Branco (Preenchimento da seta)
#define B 2 // Preto (Borda da seta)

// Matriz 16x16: Desenho perfeito da seta padrão de S.O. com contorno
static const uint8_t mouse_cursor_map[MOUSE_H][MOUSE_W] = {
    {B, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
    {B, W, B, T, T, T, T, T, T, T, T, T, T, T, T, T},
    {B, W, W, B, T, T, T, T, T, T, T, T, T, T, T, T},
    {B, W, W, W, B, T, T, T, T, T, T, T, T, T, T, T},
    {B, W, W, W, W, B, T, T, T, T, T, T, T, T, T, T},
    {B, W, W, W, W, W, B, T, T, T, T, T, T, T, T, T},
    {B, W, W, W, W, W, W, B, T, T, T, T, T, T, T, T},
    {B, W, W, W, W, W, W, W, B, T, T, T, T, T, T, T},
    {B, W, W, W, W, W, W, W, W, B, T, T, T, T, T, T},
    {B, W, W, W, W, W, W, W, W, W, B, T, T, T, T, T},
    {B, W, W, W, W, W, W, B, B, B, B, T, T, T, T, T},
    {B, W, W, W, B, W, W, B, T, T, T, T, T, T, T, T},
    {B, W, W, B, T, B, W, W, B, T, T, T, T, T, T, T},
    {B, W, B, T, T, B, W, W, B, T, T, T, T, T, T, T},
    {B, B, T, T, T, T, B, W, W, B, T, T, T, T, T, T},
    {T, T, T, T, T, T, T, B, B, B, T, T, T, T, T, T}
};

void cursor_restore_bg(int mx, int my, int screen_w, int screen_h) {
    for (int y = 0; y < MOUSE_H; y++) {
        for (int x = 0; x < MOUSE_W; x++) {
            int sx = mx + x;
            int sy = my + y;
            if (sx < screen_w && sy < screen_h && sx >= 0 && sy >= 0) {
                // Restaura o pixel salvo no buffer
                sys_draw_put_pixel(sx, sy, mouse_backbuffer[y * MOUSE_W + x]);
            }
        }
    }
}

void cursor_save_bg(int mx, int my, int screen_w, int screen_h) {
    for (int y = 0; y < MOUSE_H; y++) {
        for (int x = 0; x < MOUSE_W; x++) {
            int sx = mx + x;
            int sy = my + y;
            if (sx < screen_w && sy < screen_h && sx >= 0 && sy >= 0) {
                // Salva o pixel do fundo antes de desenhar a seta
                mouse_backbuffer[y * MOUSE_W + x] = sys_draw_get_pixel(sx, sy);
            }
        }
    }
}

void cursor_draw(int mx, int my, int screen_w, int screen_h) {
    for (int y = 0; y < MOUSE_H; y++) {
        for (int x = 0; x < MOUSE_W; x++) {
            
            int pixel_type = mouse_cursor_map[y][x];
            
            // Se o pixel não for transparente (T), nós processamos o desenho
            if (pixel_type != T) { 
                int sx = mx + x;
                int sy = my + y;
                
                // Proteção para não desenhar fora da memória de vídeo
                if (sx >= 0 && sx < screen_w && sy >= 0 && sy < screen_h) {
                    if (pixel_type == W) {
                        sys_draw_put_pixel(sx, sy, 0x00FFFFFF); // Desenha Branco
                    } else if (pixel_type == B) {
                        sys_draw_put_pixel(sx, sy, 0x00000000); // Desenha Preto
                    }
                }
            }
        }
    }
}
