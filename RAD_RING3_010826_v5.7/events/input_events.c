#include "input_events.h"
#include "../gui/gui.h"  // Garante o acesso aos enums originais do seu SO
#include "../gui/wm.h"   // Para acessar wm_handle_mouse_event

static uint8_t last_buttons = 0;
static int last_x = -1;
static int last_y = -1;

void events_process_mouse(int x, int y, uint8_t buttons, int8_t scroll) {
    // Inicialização segura para a primeira leitura do mouse
    if (last_x == -1) { 
        last_x = x; 
        last_y = y; 
    }

    // 1. Verifica se houve MOVIMENTO
    if (x != last_x || y != last_y) {
        wm_handle_mouse_event(x, y, EVENT_MOUSE_MOVE);
    }

    // 2. Verifica MOUSE DOWN (Botão esquerdo pressionado)
    // Se não estava pressionado antes e agora está
    if (!(last_buttons & 1) && (buttons & 1)) {
        wm_handle_mouse_event(x, y, EVENT_MOUSE_DOWN);
    }

    // 3. Verifica MOUSE UP (Botão esquerdo solto)
    // Se estava pressionado antes e agora não está mais
    if ((last_buttons & 1) && !(buttons & 1)) {
        wm_handle_mouse_event(x, y, EVENT_MOUSE_UP);
    }

    // Atualiza o estado anterior para o próximo ciclo
    last_buttons = buttons;
    last_x = x;
    last_y = y;
}

void events_process_keyboard(char key) {
    if (key == 0) return; 
    wm_handle_keyboard_event(key);
}
