#define INPUT_EVENTS_H
#define EVENTS_H

#include <stddef.h>
#include "wm.h"
#include "z_order.h"
#include "../gui/gui.h" 
#include "../system/graphics.h"
#include "../events/cursor_engine.h"

// Protótipos externos com assinaturas idênticas ao gui.h
extern void gui_process_press(TForm* form, int x, int y);
extern void gui_process_hover(TForm* form, int x, int y);
extern void gui_process_release(TForm* form, int x, int y);
extern void gui_process_key(TForm* form, char key);
extern void gui_render_control(TControl* ctrl); // Corrigido para TControl*

static TForm* g_desktop = NULL; 
TForm* g_active  = NULL; 

static int dragging = 0;
static int drag_offset_x = 0;
static int drag_offset_y = 0;

int wm_mouse_x = 0;
int wm_mouse_y = 0;
int wm_mouse_show = 1;

extern int screen_w;
extern int screen_h;

void wm_init(void) {
    z_order_init();
    g_desktop = NULL;
    g_active  = NULL;
    dragging = 0;
}

void wm_set_desktop(TForm* form) {
    if (!form) return;
    g_desktop = form;
}

void wm_add_window(TForm* form) {
    if (!form || form == g_desktop) return;
    z_order_add(form);
    g_active = form;
}

void wm_remove_window(TForm* form) {
    if (!form || form == g_desktop) return;
    z_order_remove_form(form);
    if (g_active == form) {
        g_active = NULL;
        int count = z_order_get_count();
        if (count > 0) g_active = z_order_get_at(count - 1);
    }
}

void wm_handle_mouse_event(int x, int y, int event) {
    TForm* target = z_order_find_at(x, y);

    if (!target && g_desktop) {
        if (x >= g_desktop->Win.Control.Left && x <= (g_desktop->Win.Control.Left + g_desktop->Win.Control.Width) &&
            y >= g_desktop->Win.Control.Top && y <= (g_desktop->Win.Control.Top + g_desktop->Win.Control.Height)) {
            target = g_desktop;
        }
    }

    switch (event) {
       case EVENT_MOUSE_DOWN: { 
            if (!target) return;

            if (target != g_desktop) {
                g_active = target;
                z_order_bring_to_front(target);
            } else {
                g_active = NULL; 
            }

            if (target->BorderStyle == bsNone) {
                gui_process_press(target, x, y);
            } else {
                int title_height = 25; 
                if (y >= target->Win.Control.Top && y <= target->Win.Control.Top + title_height) {
                    
                    // Mapeia coordenadas locais
                    int local_x = x - target->Win.Control.Left;
                    int local_y = y - target->Win.Control.Top;
                    int win_w = target->Win.Control.Width;

                    // =========================================================
                    // NOVA INTERCEÇÃO: FECHAMENTO DE JANELAS INTERNAS [X]
                    // =========================================================
                    if (local_y >= 6 && local_y <= 22 && local_x >= (win_w - 22) && local_x <= (win_w - 6)) {
                        wm_remove_window(target);
                        // Opcional: Se sua estrutura TForm/TControl exigir free(), limpe aqui.
                    }
                    else if (target->Win.Draggable) { 
                        dragging = 1;
                        target->IsDragging = 1;
                        target->DragOffsetX = local_x;
                        target->DragOffsetY = local_y;
                        drag_offset_x = target->DragOffsetX;
                        drag_offset_y = target->DragOffsetY;
                    }
                } else {
                    gui_process_press(target, x, y);
                }
            }
            break;
        }
        
        case EVENT_MOUSE_MOVE:
            if (dragging && g_active && g_active != g_desktop && g_active->Win.Draggable) {
                g_active->Win.Control.Left = x - drag_offset_x;
                g_active->Win.Control.Top  = y - drag_offset_y;
            } else {
                if (target) {
                    gui_process_hover(target, x, y);
                }
                if (g_active && !g_active->Win.Draggable && g_active != g_desktop) {
                     g_active->Win.Control.Left = 0;
                     g_active->Win.Control.Top  = 0;
                }
            }
            break;

        case EVENT_MOUSE_UP: { // Corrigido: Nome real do evento mapeado
            dragging = 0;
            if (g_active) g_active->IsDragging = 0; 
            
            if (target) {
                gui_process_release(target, x, y);
            } else if (g_active) {
                gui_process_release(g_active, x, y);
            }
            break;
        }
            
        default: break;
    }
}

void gui_render_form(TForm* form) {
    if (!form || !form->Win.Control.Visible) return;
    gui_render_control(&form->Win.Control);
    for (int i = 0; i < form->ControlCount; i++) {
        if (form->Controls[i]) {
            gui_render_control(form->Controls[i]);
        }
    }
}

void wm_render_pipeline(void) {
    if (g_desktop && g_desktop->Win.Control.Visible) {
        gui_render_form(g_desktop);
    }

    int count = z_order_get_count();
    for (int i = 0; i < count; i++) {
        TForm* f = z_order_get_at(i);
        if (f && f != g_desktop && f->Win.Control.Visible) {
            gui_render_form(f);
        }
    }
}

void wm_handle_keyboard_event(char key) {
    if (g_active == NULL) return;
    gui_process_key(g_active, key);
}
