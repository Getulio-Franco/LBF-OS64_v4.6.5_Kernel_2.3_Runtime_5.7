#include "gui.h"
#include "system/graphics.h" // Usa a liblib/syscalls para desenhar
#include "system/string.h"
#include "system/malloc.h"   // Usa o malloc do Ring3!

#define MAX_GUI_WINDOWS 32
TForm* window_stack[MAX_GUI_WINDOWS];
int window_count = 0;

static uint64_t gui_event_queue[32];
static int ev_head = 0;
static int ev_tail = 0;

TControl* g_focused_control = NULL;

/* --- AUXILIARES E BORDAS --- */
void gui_get_abs_pos(TControl* ctrl, int* x, int* y) {
    if (!ctrl) return;
    *x = ctrl->Left;
    *y = ctrl->Top;
    TWinControl* parent = ctrl->Parent;
    while (parent) {
        *x += parent->Control.Left;
        *y += parent->Control.Top;
        parent = parent->Control.Parent;
    }
}

/* =========================================================================
 * 1. FUNÇÕES AUXILIARES E DE BORDAS
 * ========================================================================= */

void draw_sunken_border(int x, int y, int w, int h) {
    sys_draw_rect(x, y, w, 1, 0x808080);         // Topo escuro
    sys_draw_rect(x, y, 1, h, 0x808080);         // Esquerda escura
    sys_draw_rect(x, y + h - 1, w, 1, 0xFFFFFF); // Base clara
    sys_draw_rect(x + w - 1, y, 1, h, 0xFFFFFF); // Direita clara
}

void gui_draw_raised_border(int x, int y, int w, int h) {
    sys_draw_rect(x, y, w, 1, 0xFFFFFF);         // Topo claro
    sys_draw_rect(x, y, 1, h, 0xFFFFFF);         // Esquerda clara
    sys_draw_rect(x, y + h - 1, w, 1, 0x404040); // Base escura
    sys_draw_rect(x + w - 1, y, 1, h, 0x404040); // Direita escura
}

void gui_add_to_parent(TWinControl* parent, TControl* child) {
    if (!parent || !child) return;
    TForm* form = (TForm*)parent;
    if (form->ControlCount < 100) {
        form->Controls[form->ControlCount] = child;
        form->ControlCount++;
        child->Parent = parent;
    }
}

/* =========================================================================
 * 2. MOTOR DE RENDERIZAÇÃO
 * ========================================================================= */

void gui_render_control(TControl* ctrl) {
    if (!ctrl || !ctrl->Visible) return;

    // 1. Desenha o componente (Botão, Edit, etc) via seu OnPaint ponteiro
    if (ctrl->OnPaint) {
        ctrl->OnPaint(ctrl);
    }

    // 2. Se estiver selecionado (Modo Design), desenha as alças por cima
    if (ctrl->IsSelected) {
        draw_selection_handles(ctrl->Left, ctrl->Top, ctrl->Width, ctrl->Height);
    }
}

/* =========================================================================
 * 3. IMPLEMENTAÇÃO DOS DESENHOS
 * ========================================================================= */

void draw_selection_handles(int x, int y, int w, int h) {
    int s = 4; // Tamanho do quadradinho (4x4 pixels)
    uint32_t color = 0x000000; // Preto

    // Cantos
    sys_draw_rect(x - s/2,     y - s/2,     s, s, color); // Topo-Esquerda
    sys_draw_rect(x + w - s/2, y - s/2,     s, s, color); // Topo-Direita
    sys_draw_rect(x - s/2,     y + h - s/2, s, s, color); // Base-Esquerda
    sys_draw_rect(x + w - s/2, y + h - s/2, s, s, color); // Base-Direita

    // Meios das bordas
    sys_draw_rect(x + w/2 - s/2, y - s/2,         s, s, color); // Meio-Topo
    sys_draw_rect(x + w/2 - s/2, y + h - s/2,     s, s, color); // Meio-Base
    sys_draw_rect(x - s/2,       y + h/2 - s/2,     s, s, color); // Meio-Esquerda
    sys_draw_rect(x + w - s/2,   y + h/2 - s/2,     s, s, color); // Meio-Direita
}

static void draw_bs_none(TForm* form) {
    TControl* ctrl = &form->Win.Control;
    
    sys_draw_rect(ctrl->Left, ctrl->Top, ctrl->Width, ctrl->Height, ctrl->Color);
    sys_draw_rect(ctrl->Left, ctrl->Top, ctrl->Width, 1, 0x000000); 
    sys_draw_rect(ctrl->Left, ctrl->Top + ctrl->Height - 1, ctrl->Width, 1, 0x000000); 
    
    for (int i = 0; i < form->ControlCount; i++) {
        if (form->Controls[i]) gui_render_control(form->Controls[i]);
    }
}

static void draw_bs_single(TForm* form) {
    TControl* ctrl = &form->Win.Control;
    int x = ctrl->Left, y = ctrl->Top, w = ctrl->Width, h = ctrl->Height;

    sys_draw_rect(x, y, w, h, ctrl->Color);
    gui_draw_raised_border(x, y, w, h);

    if (h > 25) {
        uint32_t title_color = form->ActiveFocus ? 0x000080 : 0x707070;
        uint32_t text_color = form->ActiveFocus ? 0xFFFFFF : 0xD0D0D0;

        sys_draw_rect(x + 4, y + 4, w - 8, 20, title_color); 
        sys_draw_string(x + 8, y + 6, ctrl->Caption, text_color, 1); 

        // Botão Fechar [X]
        sys_draw_rect(x + w - 22, y + 6, 16, 16, 0xC0C0C0);
        gui_draw_raised_border(x + w - 22, y + 6, 16, 16); 
        sys_draw_string(x + w - 18, y + 7, "x", 0x000000, 1);
    }

    for (int i = 0; i < form->ControlCount; i++) {
        if (form->Controls[i]) gui_render_control(form->Controls[i]);
    }
}

void gui_draw_form(TForm* form) {
    if (!form || !form->Win.Control.Visible) return;

    switch (form->BorderStyle) {
        case bsNone:
            draw_bs_none(form);
            break;
        case bsSingle:
        case bsSizeable: 
        case bsDialog:
        default:
            draw_bs_single(form);
            break;
    }
}

void gui_set_border_style(TForm* form, TBorderStyle style) {
    if (form) {
        form->BorderStyle = style;
    }
}


/* --- FÁBRICAS (CONSTRUTORES) --- */

TForm* gui_create_form(char* name, char* caption, int pid) {
    TForm* form = (TForm*)malloc(sizeof(TForm)); 
    if (!form) return NULL;
    memset(form, 0, sizeof(TForm));
    
    form->BorderStyle = bsSingle;
    strcpy(form->Win.Control.Name, name);
    strcpy(form->Win.Control.Caption, caption);
    
    form->Win.Control.Left = 50;
    form->Win.Control.Top = 50;
    form->Win.Control.Width = 400;
    form->Win.Control.Height = 300;
    form->Win.Control.Visible = true;
    form->Win.Control.Color = 0x00C0C0C0;
    form->Win.Control.OnPaint = (void*)gui_draw_form;
    form->Win.Draggable = true;

    form->OwnerPID = pid;
    form->IsDragging = false;
    form->ControlCount = 0;
    return form;
}

// CORREÇÃO LD: Ponte VCL para criação de Window/Form
TForm* gui_create_window(const char* name, const char* caption, int x, int y, int w, int h) {
    TForm* form = gui_create_form((char*)name, (char*)caption, 0);
    if (form) {
        form->Win.Control.Left = x;
        form->Win.Control.Top = y;
        form->Win.Control.Width = w;
        form->Win.Control.Height = h;
    }
    return form;
}

TMemo* gui_create_memo(TWinControl* parent, char* name) {
    TMemo* memo = (TMemo*)malloc(sizeof(TMemo)); 
    if (!memo) return NULL;
    memset(memo, 0, sizeof(TMemo));
    
    strcpy(memo->Win.Control.Name, name);
    memo->Win.Control.Width = 150;
    memo->Win.Control.Height = 100;
    memo->Win.Control.Visible = true;
    memo->Win.Control.Color = 0xFFFFFF;
    memo->Win.Control.OnPaint = (void*)gui_draw_memo;
    memo->Win.Control.Type = TYPE_MEMO; 
    
    // CORREÇÃO AQUI: Esvazia o array estático em vez de tentar setar NULL
    memo->Win.Control.Caption[0] = '\0'; 
    
    gui_add_to_parent(parent, (TControl*)memo);
    return memo;
}

TEdit* gui_create_edit(TWinControl* parent, char* name) {
    TEdit* edit = (TEdit*)malloc(sizeof(TEdit)); 
    if (!edit) return NULL;
    memset(edit, 0, sizeof(TEdit));

    strcpy(edit->Win.Control.Name, name);
    edit->Win.Control.Type = TYPE_EDIT;
    edit->Win.Control.Width = 120;
    edit->Win.Control.Height = 22;
    edit->Win.Control.Visible = true;
    edit->Win.Control.Color = 0xFFFFFF;
    edit->Win.Control.OnPaint = (void*)gui_draw_edit;

    edit->MaxLength = 255; 
    edit->CursorPos = 0;
    
    gui_add_to_parent(parent, (TControl*)edit);
    return edit;
}

TButton* gui_create_button(TWinControl* parent, char* name, char* caption) {
    TButton* btn = (TButton*)malloc(sizeof(TButton));
    memset(btn, 0, sizeof(TButton));
    strcpy(btn->Win.Control.Name, name);
    strcpy(btn->Win.Control.Caption, caption);
    btn->Win.Control.Width = 75;
    btn->Win.Control.Height = 25;
    btn->Win.Control.Visible = true;
    btn->Win.Control.Color = 0xC0C0C0;
    btn->Win.Control.OnPaint = (void*)gui_draw_button;
    
    gui_add_to_parent(parent, (TControl*)btn);
    return btn;
}

TCheckBox* gui_create_checkbox(TWinControl* parent, char* name, char* caption) {
    TCheckBox* cb = (TCheckBox*)malloc(sizeof(TCheckBox));
    memset(cb, 0, sizeof(TCheckBox));
    strcpy(cb->Win.Control.Name, name);
    strcpy(cb->Win.Control.Caption, caption);
    
    cb->Win.Control.Type = TYPE_CHECKBOX; 
    cb->Win.Control.Width = 100;
    cb->Win.Control.Height = 20;
    cb->Win.Control.Visible = true;
    cb->Win.Control.OnPaint = (void*)gui_draw_checkbox;
    
    gui_add_to_parent(parent, (TControl*)cb);
    return cb;
}

TRadioButton* gui_create_radiobutton(TWinControl* parent, char* name, char* caption) {
    TRadioButton* rb = (TRadioButton*)malloc(sizeof(TRadioButton));
    memset(rb, 0, sizeof(TRadioButton));
    strcpy(rb->Win.Control.Name, name);
    strcpy(rb->Win.Control.Caption, caption);
    
    rb->Win.Control.Type = TYPE_RADIOBUTTON; 
    rb->Win.Control.Visible = true;
    rb->Win.Control.OnPaint = (void*)gui_draw_radiobutton;
    
    gui_add_to_parent(parent, (TControl*)rb);
    return rb;
}

// CORREÇÃO LD: Ponte VCL para criação do Radio Button antigo
TRadioButton* gui_create_radio(TWinControl* parent, int x, int y, const char* text) {
    TRadioButton* rb = gui_create_radiobutton(parent, "Radio", (char*)text);
    if (rb) {
        rb->Win.Control.Left = x;
        rb->Win.Control.Top = y;
    }
    return rb;
}

TComboBox* gui_create_combobox(TWinControl* parent, char* name) {
    TComboBox* combo = (TComboBox*)malloc(sizeof(TComboBox));
    memset(combo, 0, sizeof(TComboBox));
    
    // Configura o tipo do controle para identificação na propriedade
    combo->Win.Control.Type = TYPE_COMBOBOX; 
    
    strcpy(combo->Win.Control.Name, name);
    combo->Win.Control.Width = 100;
    combo->Win.Control.Height = 22;
    combo->Win.Control.Visible = true;
    combo->Win.Control.Color = 0xFFFFFF;
    combo->Win.Control.OnPaint = (void*)gui_draw_combobox;
    
    // Inicializa a string do Caption como vazia por segurança
    combo->Win.Control.Caption[0] = '\0';
    
    gui_add_to_parent(parent, (TControl*)combo);
    return combo;
}

TLabel* gui_create_label(TWinControl* parent, char* name, char* caption) {
    TLabel* lbl = (TLabel*)malloc(sizeof(TLabel));
    memset(lbl, 0, sizeof(TLabel));
    strcpy(lbl->Graphic.Control.Name, name);
    strcpy(lbl->Graphic.Control.Caption, caption);
    lbl->Graphic.Control.Visible = true;
    lbl->Graphic.Control.OnPaint = (void*)gui_draw_label;
    
    gui_add_to_parent(parent, (TControl*)lbl);
    return lbl;
}

TPanel* gui_create_panel(TWinControl* parent, char* name) {
    TPanel* pnl = (TPanel*)malloc(sizeof(TPanel));
    memset(pnl, 0, sizeof(TPanel));
    strcpy(pnl->Win.Control.Name, name);
    pnl->Win.Control.Width = 150;
    pnl->Win.Control.Height = 100;
    pnl->Win.Control.Visible = true;
    pnl->Win.Control.Color = 0xC0C0C0;
    pnl->Win.Control.OnPaint = (void*)gui_draw_panel;
    
    gui_add_to_parent(parent, (TControl*)pnl);
    return pnl;
}

TImage* gui_create_image(TWinControl* parent, char* name) {
    TImage* img = (TImage*)malloc(sizeof(TImage));
    memset(img, 0, sizeof(TImage));
    strcpy(img->Graphic.Control.Name, name);
    img->Graphic.Control.Visible = true;
    img->Graphic.Control.OnPaint = (void*)gui_draw_image;
    
    gui_add_to_parent(parent, (TControl*)img);
    return img;
}

/* --- HANDLERS DE DESENHO (DRAW) --- */

void gui_draw_panel(TPanel* panel) {
    int x, y;
    gui_get_abs_pos((TControl*)panel, &x, &y);
    int w = panel->Win.Control.Width, h = panel->Win.Control.Height;

    sys_draw_rect(x, y, w, h, panel->Win.Control.Color);
    gui_draw_raised_border(x, y, w, h);
}

void gui_draw_label(TLabel* label) {
    int x, y;
    gui_get_abs_pos((TControl*)label, &x, &y);
    sys_draw_string(x, y, label->Graphic.Control.Caption, 0x000000, 1);
}

void gui_draw_image(TImage* img) {
    (void)img;
}

void gui_draw_combobox(TComboBox* combo) {
    int x, y;
    gui_get_abs_pos((TControl*)combo, &x, &y);
    int w = combo->Win.Control.Width;

    // 1. Desenha o fundo branco e a borda clássica para dentro (sunken)
    sys_draw_rect(x, y, w, 22, combo->Win.Control.Color);
    draw_sunken_border(x, y, w, 22);
    
    // 2. Desenha o botão da setinha "V" na direita
    sys_draw_rect(x + w - 20, y + 2, 18, 18, 0xC0C0C0);
    sys_draw_string(x + w - 15, y + 5, "V", 0x000000, 1);

    // 3. NOVIDADE: Desenha o texto do item selecionado atualmente
    // Se houver algum texto no Caption, nós renderizamos ele dentro da área branca
    if (combo->Win.Control.Caption[0] != '\0') {
        // x + 5 para dar um pequeno espaçamento da borda esquerda
        // y + 5 para alinhar verticalmente com o botão
        sys_draw_string(x + 5, y + 5, combo->Win.Control.Caption, 0x000000, 1);
    }
}

void gui_draw_radiobutton(TRadioButton* rb) {
    int x, y;
    gui_get_abs_pos((TControl*)rb, &x, &y);

    int cx = x + 6; 
    int cy = y + 8; 

    sys_draw_rect(cx - 3, cy - 6, 6, 1, 0xFFFFFF);
    sys_draw_rect(cx - 5, cy - 5, 10, 2, 0xFFFFFF);
    sys_draw_rect(cx - 6, cy - 3, 12, 6, 0xFFFFFF);
    sys_draw_rect(cx - 5, cy + 3, 10, 2, 0xFFFFFF);
    sys_draw_rect(cx - 3, cy + 5, 6, 1, 0xFFFFFF);

    if (rb->Checked) {
        sys_draw_rect(cx - 2, cy - 3, 4, 1, 0x000000);
        sys_draw_rect(cx - 3, cy - 2, 6, 4, 0x000000);
        sys_draw_rect(cx - 2, cy + 2, 4, 1, 0x000000);
    }

    sys_draw_string(x + 20, y + 3, rb->Win.Control.Caption, 0x000000, 1);
}

void gui_draw_button(TButton* btn) {
    int x, y;
    gui_get_abs_pos((TControl*)btn, &x, &y);
    int w = btn->Win.Control.Width;
    int h = btn->Win.Control.Height;

    sys_draw_rect(x, y, w, h, btn->Win.Control.Color);
    if (btn->Win.Control.State == 2) draw_sunken_border(x, y, w, h);
    else gui_draw_raised_border(x, y, w, h);            
    
    sys_draw_string(x + (w/4), y + (h/4), btn->Win.Control.Caption, 0, 1);
}

void gui_draw_checkbox(TCheckBox* cb) {
    int x, y;
    gui_get_abs_pos((TControl*)cb, &x, &y);
    
    sys_draw_rect(x, y + 2, 14, 14, 0xFFFFFF);
    draw_sunken_border(x, y + 2, 14, 14);
    if (cb->Checked) sys_draw_string(x + 3, y + 3, "X", 0x000000, 1);
    
    sys_draw_string(x + 20, y + 3, cb->Win.Control.Caption, 0x000000, 1);
}

void gui_draw_memo(TMemo* memo) {
    if (!memo) return;

    int x, y;
    gui_get_abs_pos((TControl*)memo, &x, &y);
    int w = memo->Win.Control.Width;
    int h = memo->Win.Control.Height;

    // Fundo e borda
    sys_draw_rect(x, y, w, h, memo->Win.Control.Color);
    draw_sunken_border(x, y, w, h);

    int scrollbar_w = 16;
    int scrollbar_x = x + w - scrollbar_w - 2;
    int scrollbar_y = y + 2;
    int scrollbar_h = h - 4;

    int max_chars_per_line = (w - scrollbar_w - 15) / 8;
    if (max_chars_per_line <= 0) max_chars_per_line = 1;

    char* text = memo->TextPointer;
    int total_lines = 0;
    
    // --- 1. CONTAGEM EXATA DE LINHAS ---
    if (text != NULL && text[0] != '\0') {
        int buf_idx = 0;
        int i = 0;
        while (text[i] != '\0') {
            if (text[i] == '\n' || buf_idx >= max_chars_per_line) {
                total_lines++;
                buf_idx = 0;
                if (text[i] != '\n') buf_idx++;
            } else {
                buf_idx++;
            }
            i++;
        }
        if (buf_idx > 0) total_lines++;
    }
    
    // Se estiver vazio, a linha atual conta como 1
    if (total_lines == 0) total_lines = 1;

    int max_visible_lines = (h - 10) / 16;
    if (max_visible_lines <= 0) max_visible_lines = 1;

    // Trava de segurança do ScrollY
    if (memo->ScrollY < 0) memo->ScrollY = 0;
    if (memo->ScrollY > (total_lines - max_visible_lines) && total_lines > max_visible_lines) {
        memo->ScrollY = total_lines - max_visible_lines;
    } else if (total_lines <= max_visible_lines) {
        memo->ScrollY = 0;
    }

    // --- 2. RENDERIZAÇÃO DO TEXTO ---
    int current_line = 0;
    int cursor_x_offset = 0;
    int cursor_y_offset = 0;
    int buf_idx = 0;

    if (text != NULL && text[0] != '\0') {
        char line_buffer[256];
        int i = 0;

        while (text[i] != '\0') {
            if (text[i] == '\n' || buf_idx >= max_chars_per_line || buf_idx >= 255) {
                line_buffer[buf_idx] = '\0';
                
                // Desenha apenas se estiver na zona visível do scroll
                if (current_line >= memo->ScrollY && (current_line - memo->ScrollY) < max_visible_lines) {
                    int draw_y = y + 5 + ((current_line - memo->ScrollY) * 16);
                    sys_draw_string(x + 5, draw_y, line_buffer, 0x000000, 1);
                }
                
                current_line++;
                buf_idx = 0;

                if (text[i] != '\n') {
                    line_buffer[buf_idx++] = text[i];
                }
            } else {
                line_buffer[buf_idx++] = text[i];
            }
            i++;
        }

        // Desenha a última linha
        if (buf_idx > 0) {
            line_buffer[buf_idx] = '\0';
            if (current_line >= memo->ScrollY && (current_line - memo->ScrollY) < max_visible_lines) {
                int draw_y = y + 5 + ((current_line - memo->ScrollY) * 16);
                sys_draw_string(x + 5, draw_y, line_buffer, 0x000000, 1);
            }
        }
    }

    // =========================================================================
    // CORREÇÃO: O CÁLCULO DO CURSOR AGORA FICA AQUI FORA!
    // =========================================================================
    cursor_x_offset = buf_idx * 8;
    cursor_y_offset = (current_line - memo->ScrollY) * 16;

    // --- 3. BARRA DE ROLAGEM VISUAL ---
    sys_draw_rect(scrollbar_x, scrollbar_y, scrollbar_w, scrollbar_h, 0xD4D0C8); // Fundo (Parte Escura/Canal)
    sys_draw_rect(scrollbar_x, scrollbar_y, 1, scrollbar_h, 0x808080); // Borda

    int thumb_h = scrollbar_h;
    int thumb_y = scrollbar_y;
    
    if (total_lines > max_visible_lines) {
        thumb_h = (max_visible_lines * scrollbar_h) / total_lines;
        if (thumb_h < 15) thumb_h = 15;
        
        int max_scroll_track = scrollbar_h - thumb_h;
        int max_scroll_val = total_lines - max_visible_lines;
        if (max_scroll_val > 0) {
            thumb_y = scrollbar_y + ((memo->ScrollY * max_scroll_track) / max_scroll_val);
        }
    }

    // Desenha o Bloco (Parte Clara)
    sys_draw_rect(scrollbar_x, thumb_y, scrollbar_w, thumb_h, 0xC0C0C0);
    sys_draw_rect(scrollbar_x, thumb_y, scrollbar_w, 1, 0xFFFFFFFF);
    sys_draw_rect(scrollbar_x, thumb_y, 1, thumb_h, 0xFFFFFFFF);
    sys_draw_rect(scrollbar_x, thumb_y + thumb_h - 1, scrollbar_w, 1, 0x404040);
    sys_draw_rect(scrollbar_x + scrollbar_w - 1, thumb_y, 1, thumb_h, 0x404040);

    // --- 4. DESENHO DO CURSOR PISCANTE ---
    if (g_focused_control == (TControl*)memo) {
        // Agora o cursor acompanha perfeitamente a quebra de linha sem sumir
        if (cursor_x_offset + 10 < (w - scrollbar_w) && cursor_y_offset >= 0 && cursor_y_offset < (max_visible_lines * 16)) {
            sys_draw_rect(x + 5 + cursor_x_offset, y + 5 + cursor_y_offset, 2, 14, 0x000000);
        }
    }
}

void gui_draw_edit(TEdit* edit) {
    if (!edit) return;

    int x, y;
    gui_get_abs_pos((TControl*)edit, &x, &y);
    int w = edit->Win.Control.Width;
    int h = edit->Win.Control.Height;

    // Fundo dinâmico: Amarelo se focado, senão cor base (branco)
    uint32_t cor_fundo = edit->Win.Control.Color;
    if (g_focused_control == (TControl*)edit) {
        cor_fundo = 0xFFFFFF; 
    }

    sys_draw_rect(x, y, w, h, cor_fundo);
    draw_sunken_border(x, y, w, h);

    char* texto_físico = edit->Win.Control.Caption;

    // Alinhamento vertical fixo em (y + 4) para nunca mais pular
    if (texto_físico && texto_físico[0] != '\0') {
        sys_draw_string(x + 5, y + 4, texto_físico, 0x000000, 1);
    }

    // Desenha o cursor no final do texto
    if (g_focused_control == (TControl*)edit) {
        int text_width = texto_físico ? (strlen(texto_físico) * 8) : 0; 
        if (text_width + 10 < w) {
            sys_draw_rect(x + 5 + text_width, y + 4, 2, h - 8, 0x000000);
        }
    }
}

TScrollBar* gui_create_scrollbar(TWinControl* parent, char* name, TScrollOrientation orientation) {
    TScrollBar* sb = (TScrollBar*)malloc(sizeof(TScrollBar));
    if (!sb) return NULL;
    memset(sb, 0, sizeof(TScrollBar));

    strcpy(sb->Win.Control.Name, name);
    sb->Win.Control.Type = TYPE_SCROLLBAR;
    sb->Win.Control.Visible = true;
    sb->Win.Control.Color = 0xC0C0C0; // Cinza clássico
    sb->Win.Control.OnPaint = (void*)gui_draw_scrollbar;
    sb->Orientation = orientation;
    
    gui_add_to_parent(parent, (TControl*)sb);
    return sb;
}

void gui_draw_scrollbar(TScrollBar* sb) {
    int x, y;
    gui_get_abs_pos((TControl*)sb, &x, &y);
    int w = sb->Win.Control.Width;
    int h = sb->Win.Control.Height;

    // Desenha o canal (fundo escuro da barra)
    sys_draw_rect(x, y, w, h, 0xD4D0C8);
    draw_sunken_border(x, y, w, h);

    // Desenha o "Thumb" (o bloquinho arrastável) baseado no Position/Max
    // (Aqui reaproveitamos a lógica visual exata que você criou dentro do TMemo!)
    int thumb_y = y + 2;
    int thumb_h = h - 4;
    
    if (sb->Max > 0) {
        thumb_h = (sb->PageSize * h) / sb->Max;
        if (thumb_h < 15) thumb_h = 15;
        thumb_y = y + ((sb->Position * (h - thumb_h)) / sb->Max);
    }

    sys_draw_rect(x + 1, thumb_y, w - 2, thumb_h, sb->Win.Control.Color);
    gui_draw_raised_border(x + 1, thumb_y, w - 2, thumb_h);
}

TListView* gui_create_listview(TWinControl* parent, char* name) {
    TListView* lv = (TListView*)malloc(sizeof(TListView));
    if (!lv) return NULL;
    memset(lv, 0, sizeof(TListView));

    strcpy(lv->Win.Control.Name, name);
    lv->Win.Control.Type = TYPE_LISTVIEW;
    lv->Win.Control.Visible = true;
    lv->Win.Control.Color = 0xFFFFFF; // Fundo Branco padrão
    lv->Win.Control.OnPaint = (void*)gui_draw_listview;
    lv->ItemIndex = -1; // Nenhum selecionado

    gui_add_to_parent(parent, (TControl*)lv);
    return lv;
}

void gui_draw_listview(TListView* lv) {
    int x, y;
    gui_get_abs_pos((TControl*)lv, &x, &y);
    int w = lv->Win.Control.Width;
    int h = lv->Win.Control.Height;

    // 1. Fundo e Borda para dentro
    sys_draw_rect(x, y, w, h, lv->Win.Control.Color);
    draw_sunken_border(x, y, w, h);

    int item_height = 18;
    int max_visible = (h - 4) / item_height;

    // 2. Renderiza as linhas visíveis de arquivos
    for (int i = 0; i < max_visible; i++) {
        int idx = lv->ScrollY + i;
        if (idx >= lv->ItemCount || !lv->Items) break;

        int row_y = y + 2 + (i * item_height);

        // Se for o item selecionado pelo usuário, desenha o fundo azul de seleção
        if (idx == lv->ItemIndex) {
            sys_draw_rect(x + 2, row_y, w - 4, item_height, 0x000080); // Azul Escuro
            // Desenha o texto em Branco
            sys_draw_string(x + 6, row_y + 3, lv->Items[idx].Name, 0xFFFFFF, 1);
        } else {
            // Desenha o texto em Preto normal
            sys_draw_string(x + 6, row_y + 3, lv->Items[idx].Name, 0x000000, 1);
        }
    }
}

void gui_push_event(uint64_t event_id) {
    gui_event_queue[ev_head] = event_id;
    ev_head = (ev_head + 1) % 32;
}

uint64_t gui_pop_event(void) {
    if (ev_head == ev_tail) return 0;
    uint64_t ev = gui_event_queue[ev_tail];
    ev_tail = (ev_tail + 1) % 32;
    return ev;
}

/* =========================================================================
 * SUBSISTEMA DE PROPRIEDADES DINÂMICAS PARA O DESIGNER VCL
 * ========================================================================= */

/* =========================================================================
 * FUNÇÃO: gui_set_prop (MANTENHA ELA)
 * ========================================================================= */
void gui_set_prop(void* control_ptr, TGUIProperty prop, uint64_t value) {
    if (prop == PROP_SET_FOCUS && control_ptr == NULL) {
        if (g_focused_control) {
            TWinControl* old_win = (TWinControl*)g_focused_control;
            old_win->Focused = false;
        }
        g_focused_control = NULL;
        return;
    }
    
    if (!control_ptr) return;
    TControl* ctrl = (TControl*)control_ptr;

    switch (prop) {
        case PROP_STATE: {
            ctrl->State = (int)value; 
            if (ctrl->Type == TYPE_LISTVIEW) {
                TListView* lv = (TListView*)control_ptr;
                lv->ItemCount = (int)value;
            }
            break;
        }
        case PROP_LEFT:    ctrl->Left = (int)value;   break;
        case PROP_TOP:     ctrl->Top = (int)value;    break;
        case PROP_WIDTH:   ctrl->Width = (int)value;  break;
        case PROP_HEIGHT:  ctrl->Height = (int)value; break;
        case PROP_VISIBLE: ctrl->Visible = (bool)value; break;
        case PROP_COLOR:   ctrl->Color = (uint32_t)value; break;
        
        case PROP_SET_FOCUS: {
            if (g_focused_control) {
                TWinControl* old_win = (TWinControl*)g_focused_control;
                old_win->Focused = false;
            }
            g_focused_control = ctrl;
            TWinControl* new_win = (TWinControl*)control_ptr;
            new_win->Focused = true;
            break;
        }
        case PROP_ITEM_INDEX: {
            if (ctrl->Type == TYPE_LISTVIEW) {
                TListView* lv = (TListView*)control_ptr;
                lv->ItemIndex = (int)value;
            }
            break;
        }
        case PROP_CAPTION: {
            char* src = (char*)value;
            if (src) {
                if (ctrl->Type == TYPE_LABEL) {
                    TLabel* lbl = (TLabel*)control_ptr;
                    strcpy(lbl->Graphic.Control.Caption, src);
                } 
                else if (ctrl->Type == TYPE_MEMO) {
                    TMemo* memo = (TMemo*)control_ptr;
                    memo->TextPointer = src; 
                } 
                else if (ctrl->Type == TYPE_LISTVIEW) {
                    TListView* lv = (TListView*)control_ptr;
                    lv->Items = (TListViewItem*)(uintptr_t)value;
                }
                else {
                    strcpy(ctrl->Caption, src);
                }
            }
            break;
        }
        case PROP_SCROLL_Y: { 
            if (ctrl->Type == TYPE_MEMO) {
                TMemo* memo = (TMemo*)control_ptr;
                memo->ScrollY = (int)value;
            }
            else if (ctrl->Type == TYPE_LISTVIEW) {
                TListView* lv = (TListView*)control_ptr;
                lv->ScrollY = (int)value;
            }
            break;
        }
        default: break;
    }
}

/* =========================================================================
 * FUNÇÃO: gui_get_prop (ADICIONE ESTA LOGO ABAIXO)
 * ========================================================================= */
int64_t gui_get_prop(void* control_ptr, TGUIProperty prop) {
    if (!control_ptr) return 0;
    TControl* ctrl = (TControl*)control_ptr;

    switch (prop) {
        case PROP_LEFT:        return ctrl->Left;
        case PROP_TOP:         return ctrl->Top;
        case PROP_WIDTH:       return ctrl->Width;
        case PROP_HEIGHT:      return ctrl->Height;
        case PROP_VISIBLE:     return ctrl->Visible;
        case PROP_COLOR:       return ctrl->Color;
        case PROP_STATE:       return ctrl->State;

        case PROP_ITEM_INDEX: {
            if (ctrl->Type == TYPE_LISTVIEW) {
                TListView* lv = (TListView*)control_ptr;
                return lv->ItemIndex;
            }
            return -1;
        }
        case PROP_SCROLL_Y: {
            if (ctrl->Type == TYPE_MEMO) {
                TMemo* memo = (TMemo*)control_ptr;
                return memo->ScrollY;
            }
            else if (ctrl->Type == TYPE_LISTVIEW) {
                TListView* lv = (TListView*)control_ptr;
                return lv->ScrollY;
            }
            return 0;
        }
        case PROP_CAPTION: {
            if (ctrl->Type == TYPE_MEMO) {
                TMemo* memo = (TMemo*)control_ptr;
                return (uintptr_t)memo->TextPointer;
            } 
            else if (ctrl->Type == TYPE_LISTVIEW) {
                TListView* lv = (TListView*)control_ptr;
                return (uintptr_t)lv->Items;
            }
            return (uintptr_t)ctrl->Caption;
        }
        case PROP_SET_FOCUS: {
            return (g_focused_control == control_ptr) ? 1 : 0;
        }
        default: return 0;
    }
}

void gui_set_prop_string(void* control_ptr, TGUIProperty prop, uint64_t str_ptr, int extra_val) {
    if (!control_ptr) return;
    TControl* ctrl = (TControl*)control_ptr;
    char* texto_recebido = (char*)(uintptr_t)str_ptr;
    
    // Para a roleta, o Ring 3 (VCL) só precisa atualizar o texto visível (PROP_CAPTION)
    if (prop == PROP_CAPTION && ctrl->Type == TYPE_COMBOBOX && texto_recebido != NULL) {
        TComboBox* combo = (TComboBox*)control_ptr;
        
        // Copia o texto enviado pela roleta da VCL para o Caption do controle
        strncpy(combo->Win.Control.Caption, texto_recebido, 15);
        combo->Win.Control.Caption[15] = '\0'; // Garante o terminador nulo
        return;
    }
    
    (void)extra_val;
}

void gui_render(uint64_t form_ptr) {
    TForm* form = (TForm*)form_ptr;
    if (form) {
        gui_draw_form(form);
    }
}
