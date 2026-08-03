#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>

extern uint8_t* ram_buffer;
extern int screen_w;
extern int screen_h;
extern int screen_pitch;
extern int bpp_bytes;

/**
 * @brief Estrutura de superfície (Canvas) para desenho em Ring 3
 */
typedef struct {
    int width;
    int height;
    int pitch;
    int bpp;
    uint8_t* buffer; 
} surface_t;

// Gerenciamento de Memória Gráfica
surface_t* graphics_create_surface(int w, int h);
void graphics_destroy_surface(surface_t* sur);
void graphics_commit(void);

// Funções de Desenho Primitivo
void draw_pixel(surface_t* sur, int x, int y, uint32_t color);
void draw_rect(surface_t* sur, int x, int y, int w, int h, uint32_t color);
void draw_clear(surface_t* sur, uint32_t color);

// Funções de Texto e Fontes
void draw_char(surface_t* sur, int x, int y, char c, uint32_t color, int scale);
void draw_string(surface_t* sur, int x, int y, const char* str, uint32_t color, int scale);

// Funções Numéricas e Bitmaps
void draw_hex(surface_t* sur, int x, int y, uint64_t val, uint32_t color);
void draw_dec(surface_t* sur, int x, int y, uint32_t val, uint32_t color);
void draw_bitmap(surface_t* sur, int x, int y, int w, int h, uint32_t* data);

// Funções de Sistema (Usadas pelo Cursor e Explorer)
void sys_draw_rect(int x, int y, int w, int h, uint32_t color);
void sys_draw_string(int x, int y, const char* str, uint32_t color, int scale);
void sys_draw_put_pixel(int x, int y, uint32_t color);
uint32_t sys_draw_get_pixel(int x, int y);

// Inicialização e Controle
void graphics_init(void);
void graphics_init_app(int width, int height);
void graphics_clear(uint32_t color);
void graphics_update(void);
void graphics_update_rect(int x, int y, int w, int h);

// IPC e Slots
int graphics_get_slot(void);
void graphics_set_slot(int slot);
uint8_t* graphics_get_buffer(void);

// Desenho Direto
void graphics_draw_rect(int x, int y, int w, int h, uint32_t color);
void graphics_fill_rect(int x, int y, int w, int h, uint32_t color);

#endif
