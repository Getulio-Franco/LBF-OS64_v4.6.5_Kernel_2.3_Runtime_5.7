#include "window_drag.h"
#include "z_order.h"
#include "wm.h"
#include <stddef.h>
#include <stdbool.h>

// Importa a flag global de atualização de tela
extern int refresh_screen;

// Ponteiro global para rastrear qual janela está sendo arrastada agora.
static TForm* window_currently_dragging = NULL;

void drag_check_start(TForm* f, int mx, int my) {
    if (!f) return;
    
    // Calcula a posição relativa do clique dentro da janela
    int lx = mx - f->Win.Control.Left;
    int ly = my - f->Win.Control.Top;

    // Se o clique foi na barra de título (0 a 25 pixels de altura)
    // Nota: Ajustamos para 20 ou 25 dependendo do seu padrão de UI
    if (ly >= 0 && ly <= 25) { 
        // 1. Para qualquer arraste anterior que tenha ficado "preso"
        drag_stop(); 
        
        // 2. Inicia o arraste para a nova janela
        f->IsDragging = true;
        f->DragOffsetX = lx;
        f->DragOffsetY = ly;
        
        // 3. Registra a janela globalmente
        window_currently_dragging = f; 
        
        // 4. Traz para frente no Z-Order
        z_order_bring_to_front(f);

        // Notifica que a ordem das janelas mudou
        refresh_screen = 1;
    }
}

void drag_update(int mx, int my) {
    // Só atualiza se houver uma janela explicitamente marcada para arraste
    if (window_currently_dragging != NULL && window_currently_dragging->IsDragging) {
        window_currently_dragging->Win.Control.Left = mx - window_currently_dragging->DragOffsetX;
        window_currently_dragging->Win.Control.Top = my - window_currently_dragging->DragOffsetY;
        
        // Em vez de chamar desktop_redraw(), setamos a flag para o Double Buffer
        refresh_screen = 1;
    }
}

/**
 * @brief Para o arraste. Limpa o ponteiro ativo e faz uma varredura de segurança.
 */
void drag_stop() {
    bool changed = false;

    // Cenário ideal: desmarca a janela atual
    if (window_currently_dragging != NULL) {
        window_currently_dragging->IsDragging = false;
        window_currently_dragging = NULL;
        changed = true;
    }
    
    // Fallback de Segurança: Varre todas as janelas para garantir 
    // que nenhuma flag 'IsDragging' ficou para trás
    int count = z_order_get_count();
    for (int i = 0; i < count; i++) {
        TForm* f = z_order_get_at(i);
        if (f && f->IsDragging) {
            f->IsDragging = false; 
            changed = true;
        }
    }

    if (changed) {
        refresh_screen = 1;
    }
}
