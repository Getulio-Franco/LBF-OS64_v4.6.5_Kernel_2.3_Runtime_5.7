#include "video.h"
#include "fonts.h"
#include <stddef.h>
#include <stdint.h>
#include "util/string.h"
#include "mem/heap.h" 

uint8_t* lfb_ptr = NULL;       // Era vram_buffer
uint8_t* ram_buffer = NULL;    

uint32_t screen_width = 0;     // Era screen_w
uint32_t screen_height = 0;    // Era screen_h
uint32_t screen_bpp = 32;      // Adicionado (valor em bits: 24 ou 32)
int screen_pitch = 0; 
int bpp_bytes = 0;

int term_cursor_x = 0;
int term_cursor_y = 0;
uint32_t term_color = 0xFFFFFFFF; 
uint32_t global_ui_color = 0xFFFFFFFF;

extern volatile int vga_ring0_enabled; // trava o envio do ram_buffer para o VRAM

/* ============================================================================
 * SISTEMA DE BUFFER DE DEBUG (RING 0 -> RING 3)
 * ============================================================================ */
#define DEBUG_RING_BUFFER_SIZE 4096

static char g_debug_buffer[DEBUG_RING_BUFFER_SIZE];
static uint32_t g_debug_head = 0; // Índice onde o Kernel ESCREVE
static uint32_t g_debug_tail = 0; // Índice de onde o Ring 3 LE (se necessário)

/**
 * @brief Adiciona um caractere ao buffer circular de debug do Kernel.
 */
void kdebug_putc(char c) {
    g_debug_buffer[g_debug_head] = c;
    g_debug_head = (g_debug_head + 1) % DEBUG_RING_BUFFER_SIZE;
}

/**
 * @brief Adiciona uma string completa ao buffer circular de debug.
 */
/*void kdebug_print(const char* str) {
    if (!str) return;
    while (*str) {
        kdebug_putc(*str++);
    }
}*/

/**
 * @brief Função que será chamada pela Syscall para ler os caracteres pendentes.
 * Retorna a quantidade de bytes copiados para o buffer do usuário.
 */
uint32_t kdebug_read_bytes(char* dest_buf, uint32_t max_bytes) {
    if (!dest_buf || max_bytes == 0) return 0;

    uint32_t count = 0;
    while (g_debug_tail != g_debug_head && count < max_bytes) {
        dest_buf[count] = g_debug_buffer[g_debug_tail];
        g_debug_tail = (g_debug_tail + 1) % DEBUG_RING_BUFFER_SIZE;
        count++;
    }

    return count; // Retorna quantos caracteres novos foram entregues
}


// Atualize a função video_init para usar os novos nomes:
void video_init(void* fb_address, int width, int height, int pitch_bytes, int bpp) {
    if (!fb_address || width <= 0 || height <= 0 || bpp <= 0) return;

    lfb_ptr = (uint8_t*)fb_address;
    screen_width = width;
    screen_height = height;
    screen_bpp = bpp; // Salva em bits
    bpp_bytes = bpp / 8;

    int min_pitch = width * bpp_bytes;
    screen_pitch = (pitch_bytes >= min_pitch) ? pitch_bytes : min_pitch;
    
    // --- A GRANDE MUDANÇA: Alocando o Double Buffer direto no video.c ---
    size_t buffer_size = (size_t)screen_pitch * screen_height;
    ram_buffer = (uint8_t*) kmalloc(buffer_size);

    // Se faltar memória, o S.O. deve parar aqui (Kernel Panic)
    if (ram_buffer == NULL) {
        while(1); 
    }

    // Limpa a RAM com a tela preta e já joga para a VRAM física
    clear_screen(0x000000);
    video_flush();
}

void video_flush(void) {
    if (!lfb_ptr || !ram_buffer) return;
    video_flush_to_screen(ram_buffer, (size_t)screen_pitch * screen_height);
}

// --- FUNÇÕES DE DESENHO INTELIGENTES (Suporta 24 e 32 bits) ---

static inline void put_pixel_fast(int x, int y, uint32_t color) {
    size_t offset = (size_t)y * screen_pitch + (size_t)x * bpp_bytes;
    
    if (bpp_bytes == 4) {
        *(uint32_t*)(ram_buffer + offset) = color;
    } else if (bpp_bytes == 3) {
        // Grava pixel a pixel para evitar invadir a memória vizinha (riscos)
        ram_buffer[offset]     = color & 0xFF;           // Blue
        ram_buffer[offset + 1] = (color >> 8) & 0xFF;    // Green
        ram_buffer[offset + 2] = (color >> 16) & 0xFF;   // Red
    }
}

uint32_t get_pixel(int x, int y) {
    if (!ram_buffer || x < 0 || x >= screen_width || y < 0 || y >= screen_height) return 0;
    size_t offset = (size_t)y * screen_pitch + (size_t)x * bpp_bytes;
    if (bpp_bytes == 4) return *(uint32_t*)(ram_buffer + offset);
    
    // Leitura 24-bit
    return (ram_buffer[offset]) | (ram_buffer[offset+1] << 8) | (ram_buffer[offset+2] << 16);
}

void put_pixel(int x, int y, uint32_t color) {
    if (!ram_buffer || x < 0 || x >= screen_width || y < 0 || y >= screen_height) return;
    put_pixel_fast(x, y, color);
}

void clear_screen(uint32_t color) {
    if (!ram_buffer) return;

    if (bpp_bytes == 4) {
        uint32_t *fb32 = (uint32_t*)ram_buffer;
        uint32_t total_pixels = (uint32_t)(screen_pitch * screen_height) / 4;
        for (uint32_t i = 0; i < total_pixels; i++) {
            fb32[i] = color;
        }
    } else {
        // Preenchimento seguro para 24-bit
        for (int y = 0; y < screen_height; y++) {
            size_t row_offset = y * screen_pitch;
            for (int x = 0; x < screen_width; x++) {
                size_t offset = row_offset + x * 3;
                ram_buffer[offset]     = color & 0xFF;
                ram_buffer[offset + 1] = (color >> 8) & 0xFF;
                ram_buffer[offset + 2] = (color >> 16) & 0xFF;
            }
        }
    }
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (!ram_buffer) return;
    for (int iy = 0; iy < h; iy++) {
        int py = y + iy;
        if (py < 0 || py >= screen_height) continue;
        for (int ix = 0; ix < w; ix++) {
            int px = x + ix;
            if (px < 0 || px >= screen_width) continue;
            put_pixel_fast(px, py, color);
        }
    }
}

void draw_char(int x, int y, char c, uint32_t color, int scale) {
    if (!ram_buffer) return;
    uint8_t index = (uint8_t)c;
    for (int row = 0; row < 8; row++) {
        uint8_t row_data = font_8x8[index][row];
        for (int col = 0; col < 8; col++) {
            if ((row_data >> (7 - col)) & 1) {
                if (scale <= 1) put_pixel(x + col, y + row, color);
                else draw_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void draw_string(int x, int y, const char* str, uint32_t color, int scale) {
    if (!ram_buffer || !str) return;
    int cur_x = x;
    int cur_y = y;
    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            cur_y += 8 * scale;
        } else {
            draw_char(cur_x, cur_y, *str, color, scale);
            cur_x += 8 * scale;
        }
        str++;
    }
}

// --- UTILITÁRIOS E TERMINAL ---
int video_get_width()  { return screen_width; }
int video_get_height() { return screen_height; }
int video_get_pitch()  { return screen_pitch; }

/**
 * @brief vga_print_string refatorada para estabilidade do sys_exec.
 */
void vga_print_string(const char* str, int col, int row) {
    if (!str) return;

    // Redireciona o log imediatamente para a Serial/Ring Buffer
    kdebug_print(str);
    
    /**
     * 🛡️ CORREÇÃO DE ESTABILIDADE:
     * Comente a linha abaixo (draw_string) enquanto estiver depurando 
     * o sys_exec, escalonador ou rotinas críticas de memória.
     * Desenhar pixels no meio do ring0 desestabiliza as interrupções.
     * Descomente apenas quando for depurar algo visual ou quando o S.O. estiver estável.
     */
    // draw_string(col * 8, row * 16, str, 0xFFFFFFFF, 1);
}

/*void vga_print_string(const char* str, int col, int row) {
    // Captura a string no buffer de debug
    kdebug_print(str);
    
    draw_string(col * 8, row * 16, str, 0xFFFFFFFF, 1);
}*/  // trava o S.O. No debug vou melhor e alterar


/**
 * @brief Envia para a VRAM de forma segura. O Triple Fault acaba aqui.
 */
/*void video_flush_to_screen(void* buffer, size_t size) {
    if (lfb_ptr != NULL && buffer != NULL) {
        uint8_t* dest = lfb_ptr;
        uint8_t* src  = (uint8_t*)buffer;
        
        // Copia linha por linha ignorando os paddings com lixo
        for (int y = 0; y < screen_height; y++) {
            memcpy(dest, src, screen_width * bpp_bytes);
            dest += screen_pitch;
            src  += screen_pitch;
        }
    }
}*/

/*void terminal_clear() {
    if (!ram_buffer) return;
    // Limpa o buffer de memória com a cor preta (ou de fundo)
    memset(ram_buffer, 0x00, screen_width * screen_height * 4);
    term_cursor_x = 0;
    term_cursor_y = 0;
    video_flush();
}*/

void terminal_clear() {
    if (!ram_buffer) return;
    
    // CORREÇÃO: Limpa usando o pitch real da tela para cobrir todo o espaço de memória alocado
    memset(ram_buffer, 0x00, screen_pitch * screen_height);
    
    term_cursor_x = 0;
    term_cursor_y = 0;
    video_flush();
}

void video_flush_to_screen(void* buffer, size_t size) {
    if (lfb_ptr != NULL && buffer != NULL) {
        uint8_t* dest = lfb_ptr;
        uint8_t* src  = (uint8_t*)buffer;
        
        uint32_t bytes_per_line = screen_width * bpp_bytes;

        for (int y = 0; y < screen_height; y++) {
            // Copia apenas os pixels visíveis
            memcpy(dest, src, bytes_per_line);
            
            // Avança para a próxima linha física no frame buffer do hardware
            dest += screen_pitch;
            
            // CORREÇÃO: Se o seu ram_buffer foi alocado usando screen_pitch, avance por screen_pitch.
            // Se o ram_buffer foi alocado estrito (width * bpp), mude aqui para: src += bytes_per_line;
            src += screen_pitch; 
        }
    }
}

void terminal_putc(char c) {
    if (!vga_ring0_enabled) {
        return; // trava o envio do ram_buffer para o VRAM adicionando 01/07/26
    }
    
    // CAPTURA EM TEMPO REAL PARA O BUFFER DE DEBUG:
    kdebug_putc(c);
    
    if (!ram_buffer) return;

    // 1. Tratamento de caracteres de controle
    if (c == '\n') {
        term_cursor_x = 0;
        term_cursor_y += 16;
    } else if (c == '\r') {
        term_cursor_x = 0;
    } else if (c == '\t') {
        term_cursor_x = (term_cursor_x + 32) & ~31; // Tabulação alinhada
    } else if (c == '\b') { // BACKSPACE (Importante para o Shell!)
         // Dentro do "else if (c == '\b')" da função terminal_putc:
         if (term_cursor_x >= 8) {
           term_cursor_x -= 8;
           // Use o nome exato da sua função: draw_rect
           draw_rect(term_cursor_x, term_cursor_y, 8, 16, 0x000000); 
         }
    } else {
        // 2. DESENHO: Sempre no ram_buffer para o video_flush() pegar depois
        draw_char(term_cursor_x, term_cursor_y, c, term_color, 1);
        term_cursor_x += 8;
    }

    // 3. Quebra de linha automática (Word Wrap)
    if (term_cursor_x + 8 > (int)screen_width) {
        term_cursor_x = 0;
        term_cursor_y += 16;
    }

    // 4. Scroll ou Limpeza
    if (term_cursor_y + 16 > (int)screen_height) {
        terminal_clear(); // Limpa o ram_buffer
        term_cursor_y = 0; 
        term_cursor_x = 0;
    }

    // 5. O PONTO CHAVE: Avisa o Double Buffer para atualizar a tela!
    extern int refresh_screen;
    refresh_screen = 1;
}

void terminal_print(const char* str) {
    while (*str) {
        terminal_putc(*str++);
    }
}

// Desenha um número em Hexadecimal (Ex: 0x000000000000C3C7)
void draw_hex(int x, int y, uint64_t val, uint32_t color) {
    char hex_chars[] = "0123456789ABCDEF";
    
    // Desenha o prefixo "0x" na posição exata x, y
    draw_char(x, y, '0', color, 1);
    draw_char(x + 8, y, 'x', color, 1);
    
    int current_x = x + 16;
    for (int i = 60; i >= 0; i -= 4) {
        int nibble = (val >> i) & 0xF;
        // Desenha cada dígito sem mexer nas variáveis globais do terminal
        draw_char(current_x, y, hex_chars[nibble], color, 1);
        current_x += 8;
    }
}

// Desenha um número Decimal (Ex: PID 10)
void draw_dec(int x, int y, uint32_t val, uint32_t color) {
    if (val == 0) {
        draw_char(x, y, '0', color, 1);
        return;
    }

    char buf[11];
    int i = 10;
    buf[i] = '\0';
    uint32_t temp_val = val;
    
    while (temp_val > 0) {
        buf[--i] = (temp_val % 10) + '0';
        temp_val /= 10;
    }

    // Em vez de terminal_print, desenha caractere por caractere na posição x, y
    int current_x = x;
    char* s = &buf[i];
    while (*s) {
        draw_char(current_x, y, *s++, color, 1);
        current_x += 8;
    }
}

void draw_bitmap(int x, int y, int w, int h, uint32_t* data) {
    if (!ram_buffer || !data) return;

    for (int j = 0; j < h; j++) {
        int py = y + j;
        if (py < 0 || py >= (int)screen_height) continue;

        // Usa o screen_pitch real em bytes fornecido pela BIOS VESA
        uint8_t* row_ptr = ram_buffer + (size_t)py * screen_pitch;

        for (int i = 0; i < w; i++) {
            int px = x + i;
            if (px < 0 || px >= (int)screen_width) continue;

            uint32_t color = data[j * w + i];
            if (color != 0xFF00FF) {
                // Multiplica px por 4 (bytes por pixel em 32-bit) para alinhar perfeitamente
                uint32_t* pixel = (uint32_t*)(row_ptr + (size_t)px * 4);
                *pixel = color;
            }
        }
    }
}

void vga_print_hex_64(uint64_t val, int col, int row) {
    // Em vez de desenhar na tela, converte para string e manda para a porta serial
    // (Você pode usar sua função hex_to_string do string.c e passar para kdebug_print)
    
    // Desativado temporariamente para não quebrar a tela durante o boot
    // draw_hex(col * 8, row * 16, val, 0xFFFFFFFF);
}

/*void vga_print_hex_64(uint64_t val, int col, int row) {
    draw_hex(col * 8, row * 16, val, 0xFFFFFFFF);
}*/


/* ============================================================================
 * ============================================================================
 * SEÇÃO DE DEBUG ISOLADA (SERIAL COM1 -> C:\os_debug.txt no VirtualBox)
 * Comente ou remova este bloco inteiro quando o S.O. estiver finalizado.
 * ============================================================================
 * ============================================================================ */

// Funções de I/O de portas em Assembly Inline (caso já não estejam declaradas globalmente)
static inline void kdebug_outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t kdebug_inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * @brief Envia um caractere para a COM1 (0x3F8) aguardando o buffer esvaziar.
 */
void serial_putchar(char c) {
    // 0x3F8 + 5 é o registrador de status (LSR). O bit 5 (0x20) indica que o THR está vazio.
    while ((kdebug_inb(0x3F8 + 5) & 0x20) == 0);
    kdebug_outb(0x3F8, (uint8_t)c);
}

/**
 * @brief Envia uma string completa para a porta serial do VirtualBox.
 */
void kdebug_serial_print(const char* str) {
    if (!str) return;
    while (*str) {
        serial_putchar(*str++);
    }
}

// ============================================================================
// ROTINAS DE DEBUG SIMPLIFICADAS E BLINDADAS
// ============================================================================

/**
 * @brief kdebug_print seguro: Envia APENAS para a Serial e para o Ring Buffer.
 * Evita totalmente tocar na memória de vídeo (ram_buffer).
 */
void kdebug_print(const char* str) {
    if (!str) return;

    // 1. Envia para o log do VirtualBox (Serial COM1) de forma atômica/rápida
    const char* s_ptr = str;
    while (*s_ptr) {
        serial_putchar(*s_ptr++);
    }

    // 2. Alimenta o Ring Buffer interno (sem travar o sistema)
    while (*str) {
        g_debug_buffer[g_debug_head] = *str;
        g_debug_head = (g_debug_head + 1) % DEBUG_RING_BUFFER_SIZE;
        str++;
    }
}


/**
 * @brief SUBSTITUA ou ATUALIZE a sua função kdebug_print original por esta,
 * para que todo log enviado ao kernel vá automaticamente para o arquivo de texto no Host.
 */
/*void kdebug_print(const char* str) {
    if (!str) return;

    // 1. Envia uma cópia exata direto para a porta serial (gravando no log do VirtualBox)
    kdebug_serial_print(str);

    // 2. Continua alimentando o buffer circular interno normal do S.O.
    const char* temp = str;
    while (*temp) {
        kdebug_putc(*temp++);
    }
}*/ // trava o S.O. No debug vou melhorar
/* ============================================================================
 * FIM DA SEÇÃO DE DEBUG SERIAL
 * ============================================================================ */
