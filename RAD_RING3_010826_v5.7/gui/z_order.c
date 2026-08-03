#include "z_order.h"
#include <stddef.h>

#define MAX_WINDOWS 32

static TForm* window_stack[MAX_WINDOWS];
static int window_count = 0;

void z_order_init() {
    window_count = 0;
    for(int i = 0; i < MAX_WINDOWS; i++) window_stack[i] = NULL;
}

int z_order_get_count() { return window_count; }

TForm* z_order_get_at(int index) { 
    if (index < 0 || index >= window_count) return NULL;
    return window_stack[index]; 
}

void z_order_add(TForm* form) {
    if (form && window_count < MAX_WINDOWS) {
        window_stack[window_count++] = form;
    }
}

void z_order_bring_to_front(TForm* form) {
    if (!form) return;

    int index = -1;
    for (int i = 0; i < window_count; i++) {
        if (window_stack[i] == form) { index = i; break; }
    }
    
    // Se não achou ou já está na frente, libera e sai
    if (index == -1 || index == window_count - 1) {
        return;
    }

    for (int i = index; i < window_count - 1; i++) {
        window_stack[i] = window_stack[i + 1];
    }
    window_stack[window_count - 1] = form;
}

void z_order_remove_form(TForm* form) {
    if (!form) return;

    for (int i = 0; i < window_count; i++) {
        if (window_stack[i] == form) {
            for (int j = i; j < window_count - 1; j++) {
                window_stack[j] = window_stack[j + 1];
            }
            window_stack[--window_count] = NULL;
            return;
        }
    }
}

void z_order_remove_by_pid(int pid) {
    for (int i = 0; i < window_count; i++) {
        if (window_stack[i] && window_stack[i]->OwnerPID == pid) {
            for (int j = i; j < window_count - 1; j++) {
                window_stack[j] = window_stack[j + 1];
            }
            window_stack[--window_count] = NULL;
            i--; 
        }
    }
}

TForm* z_order_find_at(int x, int y) {
    // Aqui não precisa de CLI/STI obrigatoriamente, 
    // pois é apenas leitura, mas é bom ter cuidado.
    for (int i = window_count - 1; i >= 0; i--) {
        TForm* f = window_stack[i];
        if (f && f->Win.Control.Visible) {
            if (x >= f->Win.Control.Left && x <= (f->Win.Control.Left + f->Win.Control.Width) &&
                y >= f->Win.Control.Top && y <= (f->Win.Control.Top + f->Win.Control.Height)) {
                return f;
            }
        }
    }
    return NULL;
}
