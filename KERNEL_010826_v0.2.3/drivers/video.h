#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// --- Definições de Cores (Formato ARGB 32-bit) ---
#define COLOR_BLACK       0x00000000
#define COLOR_WHITE       0x00FFFFFF
#define COLOR_RED         0x00FF0000
#define COLOR_GREEN       0x0000FF00
#define COLOR_BLUE        0x000000FF
#define COLOR_YELLOW      0x00FFFF00
#define COLOR_GREY        0x00222222
#define COLOR_DARK_GREY   0x00111111
#define COLOR_WINDOWS_BLUE 0x00005A9E
#define COLOR_START_GREEN 0x0000A300

// --- Gerenciamento de Buffer (Double Buffering) ---

/**
 * @brief Inicializa o driver de vídeo, detecta LFB e aloca o Backbuffer (RAM).
 */
void video_init(void* fb_address, int width, int height, int pitch_bytes, int bpp);

/**
 * @brief Transfere todo o conteúdo do Backbuffer (RAM) para a VRAM (Tela).
 * Esta função deve ser chamada após terminar todos os desenhos do frame.
 */
void video_flush(void);

/**
 * @brief Retorna a cor de um pixel específico no Backbuffer.
 */
uint32_t get_pixel(int x, int y);

// --- Funções de Desenho Primitivas (Escrevem na RAM) ---

void put_pixel(int x, int y, uint32_t color);
void clear_screen(uint32_t color);
void draw_rect(int x, int y, int w, int h, uint32_t color);
void draw_bitmap(int x, int y, int w, int h, uint32_t* data);

// --- Funções de Texto e Fontes ---

void draw_char(int x, int y, char c, uint32_t color, int scale);
void draw_string(int x, int y, const char* str, uint32_t color, int scale);
void draw_hex(int x, int y, uint64_t val, uint32_t color);
void draw_dec(int x, int y, uint32_t val, uint32_t color);

// --- Interface do Terminal (Legacy/Console) ---

void terminal_putc(char c);
void terminal_clear(void);
void terminal_print(const char* str);
void vga_print_string(const char* str, int col, int row);

// --- Elementos de UI (Desktop Shell) ---

/**
 * @brief Desenha a barra de tarefas clássica no rodapé da tela.
 */
void draw_windows_bar(uint16_t width, uint16_t height);

// --- Getters de Estado ---

int video_get_width(void);
int video_get_height(void);
int video_get_pitch(void);
int video_get_bpp(void);

// --- Variáveis Globais Externas ---

extern uint32_t global_ui_color;
extern uint8_t* ram_buffer; // Exposto para caso o kernel precise de acesso direto à RAM de vídeo
void video_flush_to_screen(void* buffer, size_t size);
void vga_print_hex_64(uint64_t val, int col, int row);
uint32_t kdebug_read_bytes(char* dest_buf, uint32_t max_bytes);
void kdebug_print(const char* str);
void kdebug_putc(char c);

#endif // VIDEO_H
