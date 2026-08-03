#include "gui_interaction.h"
#include "z_order.h"
#include "gui.h"
#include <stddef.h>      // Define o NULL
#include <stdbool.h>     // Define o bool (caso seu compilador peça)

extern TControl* g_focused_control;

TForm* gui_find_form_at(int mx, int my) {
    // Busca do topo (fim do array) para o fundo
    for (int i = z_order_get_count() - 1; i >= 0; i--) {
        TForm* f = z_order_get_at(i);
        if (f && f->Win.Control.Visible) {
            int x1 = f->Win.Control.Left;
            int y1 = f->Win.Control.Top;
            if (mx >= x1 && mx <= (x1 + f->Win.Control.Width) &&
                my >= y1 && my <= (y1 + f->Win.Control.Height)) {
                return f;
            }
        }
    }
    return NULL;
}

bool gui_is_inside_control(int lx, int ly, TControl* ctrl) {
    return (lx >= ctrl->Left && lx <= (ctrl->Left + ctrl->Width) &&
            ly >= ctrl->Top  && ly <= (ctrl->Top + ctrl->Height));
}

void gui_process_hover(TForm* f, int mx, int my) {
    if (!f) return;
    
    // Converte para coordenadas locais
    int lx = mx - f->Win.Control.Left;
    int ly = my - f->Win.Control.Top;

    for (int i = 0; i < f->ControlCount; i++) {
        TControl* ctrl = f->Controls[i];
        if (!ctrl->Visible) continue;

        // Se o mouse está sobre o controle
        if (lx >= ctrl->Left && lx <= (ctrl->Left + ctrl->Width) &&
            ly >= ctrl->Top && ly <= (ctrl->Top + ctrl->Height)) {
            
            if (ctrl->State != 2) { // Se não estiver pressionado
                ctrl->State = 1;   // Estado Hover
            }
        } else {
            if (ctrl->State == 1) { 
                ctrl->State = 0;   // Volta ao normal
            }
        }
    }
}

// Altere apenas a parte final da sua gui_process_click:
void gui_process_click(TForm* f, int mx, int my) {
    if (!f) return;
    int lx = mx - f->Win.Control.Left;
    int ly = my - f->Win.Control.Top;

    for (int i = 0; i < f->ControlCount; i++) {
        TControl* ctrl = f->Controls[i];
        if (ctrl->Visible && gui_is_inside_control(lx, ly, ctrl)) {
            
            // O ERRO ESTAVA AQUI: Não chame a função, envie o EVENTO
            gui_push_event((uint64_t)ctrl); 
            
            ctrl->State = 2; // Visual de pressionado
            break; 
        }
    }
}

void gui_process_release(TForm* f, int mx, int my) {
    if (!f) return;
    int lx = mx - f->Win.Control.Left;
    int ly = my - f->Win.Control.Top;

    for (int i = 0; i < f->ControlCount; i++) {
        TControl* ctrl = f->Controls[i];
        if (ctrl->State == 2) { // Estava pressionado?
            ctrl->State = 1;    // Volta para hover
            
            // Se soltou o mouse AINDA dentro do botão, considera um clique válido
            if (gui_is_inside_control(lx, ly, ctrl)) {
                gui_push_event((uint64_t)ctrl); 
            }
        }
    }
}

void gui_process_press(TForm* f, int mx, int my) {
    if (!f) return;

    // Converte para coordenadas locais (relativas à janela)
    int lx = mx - f->Win.Control.Left;
    int ly = my - f->Win.Control.Top;

    // Percorre de TRÁS PARA FRENTE (N-1 até 0) para respeitar o Z-Order dos componentes
    for (int i = f->ControlCount - 1; i >= 0; i--) {
        TControl* ctrl = f->Controls[i];
        
        // Verifica se o componente é visível e se o clique foi dentro dele
        if (ctrl->Visible && gui_is_inside_control(lx, ly, ctrl)) {
            
            // 1. Define o foco global para este componente
            g_focused_control = ctrl; 

            // 2. Lógica específica para CHECKBOX
            if (ctrl->Type == TYPE_CHECKBOX) {
                TCheckBox* cb = (TCheckBox*)ctrl;
                cb->Checked = !cb->Checked; // Inverte o estado
                if (cb->Win.Control.OnPaint) cb->Win.Control.OnPaint(cb);
                return; // Clique processado e encerrado
            } 
            
            // 3. LÓGICA DO RADIOBUTTON (Agora no lugar certo!)
            else if (ctrl->Type == TYPE_RADIOBUTTON) {
                TRadioButton* rb_clicado = (TRadioButton*)ctrl;

                // Se já estiver marcado, não faz nada (padrão VCL/Delphi)
                if (!rb_clicado->Checked) {
                    // Percorre TODOS os controles para desmarcar os irmãos do mesmo grupo
                    for (int j = 0; j < f->ControlCount; j++) {
                        TControl* c_aux = f->Controls[j];
                        if (c_aux && c_aux->Type == TYPE_RADIOBUTTON) {
                            TRadioButton* rb_irmao = (TRadioButton*)c_aux;
                            // Se for do mesmo grupo e estiver checado, desmarca
                            if (rb_irmao->GroupIndex == rb_clicado->GroupIndex && rb_irmao->Checked) {
                                rb_irmao->Checked = false;
                                if (rb_irmao->Win.Control.OnPaint) rb_irmao->Win.Control.OnPaint(rb_irmao);
                            }
                        }
                    }
                    // Marca o atual
                    rb_clicado->Checked = true;
                    if (rb_clicado->Win.Control.OnPaint) rb_clicado->Win.Control.OnPaint(rb_clicado);
                }
                return; // Clique processado e encerrado
            }

            // 4. Lógica padrão para outros componentes (Botões, etc)
            else {
                ctrl->State = 2; // Estado Visual: Pressionado
                if (ctrl->OnPaint) ctrl->OnPaint(ctrl);
                return;
            }
        }
    }

    // Se clicou na janela mas não em nenhum controle, limpa o foco
    g_focused_control = NULL;
}

// gui_interaction.c ou gui.c
void gui_process_key(TForm* form, char key) {
    if (!form || !form->Win.Control.Visible) return;

    // Lógica para enviar a tecla para o controle focado dentro do form
    // Por enquanto, você pode apenas imprimir para testar
    // printf("Tecla %c enviada para o formulário %s\n", key, form->Win.Control.Name);
}

/*void gui_process_key(TForm* form, char key) {
    if (!form || !form->Win.Control.Visible) return;

    // Se houver um controle focado (ex: um TEdit), mandamos a tecla para ele
    // Por enquanto, como você está montando a base, podemos focar no tratamento global
    if (form->OnKeyPress) {
        // Se você tiver um sistema de ponteiro de função para eventos:
        // form->OnKeyPress(form, key);
    }
}*/


