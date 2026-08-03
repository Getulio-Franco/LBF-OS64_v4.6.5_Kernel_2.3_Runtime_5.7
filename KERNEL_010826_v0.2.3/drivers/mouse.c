#include "mouse.h"
#include "desktop.h"
#include "io.h"
#include <stdint.h>

#define MOUSE_DATA_PORT    0x60
#define MOUSE_STATUS_PORT  0x64
#define MOUSE_COMMAND_PORT 0x64

//#define RESOLUCAO_X 1024
//#define RESOLUCAO_Y 768

static uint8_t mouse_cycle = 0;
static uint8_t mouse_byte[4];

// Variáveis globais de estado do mouse
//int mouse_x = 0; 
//int mouse_y = 0;
// Inicia o mouse no meio da tela
int mouse_x = 1024 / 2; // 512
int mouse_y = 768 / 2;  // 384
uint32_t mouse_buttons = 0;

// Variáveis de limite da tela (Padrão inicial, mas mudam no boot)
static int screen_width  = 1024;
static int screen_height = 768;

/**
 * Atualiza os limites da tela para o mouse.
 * Deve ser chamada pelo driver de vídeo (VBE/VESA) sempre que a resolução mudar.
 */
void mouse_set_screen_size(int width, int height) {
    screen_width = width;
    screen_height = height;
    
    // Opcional: Reposiciona o mouse no centro da nova resolução
    mouse_x = width / 2;
    mouse_y = height / 2;
}

void mouse_handler_it(uint8_t status_byte, int8_t delta_x, int8_t delta_y, int8_t delta_z) {
    int real_x = (int)delta_x;
    int real_y = (int)delta_y;

    // Tratamento dos bits de sinal (Movimento negativo)
    if (status_byte & 0x10) real_x -= 256;
    if (status_byte & 0x20) real_y -= 256;

    mouse_x += real_x;
    // No PS/2 o eixo Y é invertido em relação às coordenadas de tela
    mouse_y -= real_y; 

    // LIMITADORES DINÂMICOS (Clamping)
    // Agora usamos screen_width e screen_height em vez de constantes
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    
    if (mouse_x >= screen_width)  mouse_x = screen_width - 1;
    if (mouse_y >= screen_height) mouse_y = screen_height - 1;
}

void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout-- && !(inb(MOUSE_STATUS_PORT) & 1)); 
    } else {
        while (timeout-- && (inb(MOUSE_STATUS_PORT) & 2));      
    }
}

void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0xD4); 
    mouse_wait(1);
    outb(MOUSE_DATA_PORT, data);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(MOUSE_DATA_PORT);
}

void mouse_init() {
    uint8_t status;
    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0xA8); // Ativar mouse

    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0x20); // Ler Command Byte
    mouse_wait(0);
    status = (inb(MOUSE_DATA_PORT) | 2); // Habilitar IRQ12
    
    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0x60); // Escrever Command Byte
    mouse_wait(1);
    outb(MOUSE_DATA_PORT, status);

    mouse_write(0xF6); // Set defaults
    mouse_read(); 
    mouse_write(0xF4); // Enable streaming
    mouse_read(); 
    
    // Enviar estas taxas de amostragem habilita o 4º byte (scroll)
    mouse_write(0xF3); mouse_read(); mouse_write(200); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(100); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(80);  mouse_read();
    // Agora o mouse enviará 4 bytes por pacote!
}

void mouse_handler() {
    uint8_t status = inb(MOUSE_STATUS_PORT);
    if (!(status & 0x01)) return; 
    uint8_t data = inb(MOUSE_DATA_PORT);
    if (!(status & 0x20)) return; 

    switch (mouse_cycle) {
        case 0:
            if (!(data & 0x08)) return; // Sincronia
            mouse_byte[0] = data;
            mouse_cycle = 1;
            break;
        case 1:
            mouse_byte[1] = data;
            mouse_cycle = 2;
            break;
        case 2:
            mouse_byte[2] = data;
            mouse_cycle = 3; // Agora vai para o estágio 3
            break;
        case 3:
            mouse_byte[3] = data; // Este é o byte do Scroll!
            mouse_cycle = 0;

            // 1. Cálculo de rel_x e rel_y
            int32_t rel_x = mouse_byte[1];
            int32_t rel_y = mouse_byte[2];

            // Aplicamos a extensão de sinal apenas se o hardware confirmar
            // que o movimento foi de fato negativo (Bit 4 e 5 do Status Byte)
            if (mouse_byte[0] & 0x10) {rel_x |= 0xFFFFFF00;}
            if (mouse_byte[0] & 0x20) {rel_y |= 0xFFFFFF00;}
            
            // 2. Cálculo do Scroll
            int8_t scroll = (int8_t)(mouse_byte[3] & 0x0F);
            if (mouse_byte[3] & 0x08) { 
                scroll |= 0xF0; 
            }

            // 3. ATUALIZA OS BOTÕES (Syscall lê daqui)
            uint8_t buttons = mouse_byte[0] & 0x07;
            mouse_buttons = buttons; 

            // 4. ATUALIZA AS COORDENADAS GLOBAIS (Syscall lê daqui)
            mouse_x += rel_x;
            mouse_y -= rel_y; // PS/2 inverte o eixo Y

            // 5. LIMITADORES DE TELA (Clamping)
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= screen_width)  mouse_x = screen_width - 1;
            if (mouse_y >= screen_height) mouse_y = screen_height - 1;

            // (Opcional) Se você não usa mais o desktop no Ring 0, pode comentar a linha abaixo:
            // desktop_update_mouse(rel_x, -rel_y, buttons, scroll);
            break;
    }
}
