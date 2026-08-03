/**
 * ============================================================================
 * PS/2 KEYBOARD DRIVER - V3.1 (LFB/VESA COMPATIBLE)
 * ============================================================================
 * Descrição: Gerencia a entrada de dados via IRQ1 (Teclado PS/2).
 * Localização: drivers/keyboard.c
 * ============================================================================
 */

#include "drivers/keyboard.h"
#include "drivers/vfs.h"
#include "drivers/proc.h"
#include "io.h"
#include <stdint.h>
#include <stddef.h>

// --- DEFINIÇÕES E CONSTANTES ---
#define KEYBOARD_BUFFER_SIZE 256
#define SCANCODE_RELEASE_MASK 0x80
#define SCANCODE_LSHIFT 0x2A
#define SCANCODE_RSHIFT 0x36
#define SCANCODE_CAPS_LOCK 0x3A

// --- ESTADO INTERNO DO DRIVER ---
static volatile char circular_buffer[KEYBOARD_BUFFER_SIZE];
static volatile int buffer_head = 0; 
static volatile int buffer_tail = 0;  

static int shift_pressed = 0;
static int caps_lock_active = 0;

/**
 * MAPA DE TECLAS ABNT2 (Português Brasil)
 */

static unsigned char abnt2_map[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '~', '`', 0, // Trocado 0xE7 por ';'
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static unsigned char abnt2_shift_map[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '^', '\"', 0, // Trocado 0xC7 por ':'
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

// --- SECTION 1: VFS INTERFACE ---

/**
 * @brief Implementação de leitura para o stdin (teclado).
 */
uint32_t keyboard_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!buffer || size == 0) return 0;
    
    uint32_t bytes_read = 0;
    
    while (bytes_read < size) {
        char c = keyboard_pop_char();
        
        if (c != 0) {
            buffer[bytes_read++] = (uint8_t)c;
            
            // Para comandos de console, o ENTER finaliza a leitura
            if (c == '\n' || c == '\r') {
                return bytes_read;
            }
        } 
        else {
            // Se o buffer está vazio, cedemos a CPU para outros processos
            // Em vez de um loop infinito de pausa.
            force_reschedule(); 
        }
    }
    
    return bytes_read; 
}

static vfs_ops_t keyboard_ops = { .read = keyboard_vfs_read };

vfs_node_t keyboard_device_node = { 
    .name = "stdin", 
    .type = VFS_TYPE_CHAR_DEVICE,
    .ops = &keyboard_ops 
};

// --- SECTION 2: GERENCIAMENTO DE BUFFER ---

void keyboard_init(void) {
    buffer_head = 0;
    buffer_tail = 0;
    shift_pressed = 0;
    caps_lock_active = 0;
}

char keyboard_pop_char(void) {
    uint64_t flags = cli_save(); 
    
    if (buffer_head == buffer_tail) {
        sti_restore(flags);
        return 0; 
    }
    
    char c = circular_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    
    sti_restore(flags);
    return c;
}

static void keyboard_push_char(char c) {
    int next_pos = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_pos != buffer_tail) { 
        circular_buffer[buffer_head] = c;
        buffer_head = next_pos;
    }
}

// --- SECTION 3: HARDWARE LOGIC ---

void keyboard_handler(void) {
    uint8_t scancode = inb(0x60); 

    if (scancode & SCANCODE_RELEASE_MASK) {
        uint8_t released_key = scancode & 0x7F;
        if (released_key == SCANCODE_LSHIFT || released_key == SCANCODE_RSHIFT) {
            shift_pressed = 0;
        }
    } else {
        if (scancode == SCANCODE_LSHIFT || scancode == SCANCODE_RSHIFT) {
            shift_pressed = 1;
        } else if (scancode == SCANCODE_CAPS_LOCK) {
            caps_lock_active = !caps_lock_active;
        } else if (scancode < 58) { 
            unsigned char key_char = shift_pressed ? abnt2_shift_map[scancode] : abnt2_map[scancode];
            
            if (key_char != 0) {
                if (caps_lock_active && key_char >= 'a' && key_char <= 'z') {
                     key_char -= 32;
                }

                 if (foreground_process != NULL) {
                      // 2. SENÃO, se tem processo de terminal rodando, manda para a fila dele
                      keyboard_push_char(key_char);
                 }
            }
        }
    }

    outb(0x20, 0x20); // EOI
}
