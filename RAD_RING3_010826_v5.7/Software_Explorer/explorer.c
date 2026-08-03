#include "../gui/gui.h"
#include "../system/graphics.h" 
#include "../system/liblib.h"
#include "../gui/wm.h"
#include "../system/string.h" 
#include "../events/cursor_engine.h"

// Protótipos para renderização manual de overlays
extern void gui_draw_form(TForm* form);
extern void gui_render_form(TForm* form);

extern uint8_t* ram_buffer; 


// Vetor para obter a lista de processos ativos do kernel
//TProcessInfo lista_verificacao[32];

// z_stack gerencia os 15 slots externos (5 a 19)
int z_stack[MAX_EXTERNAL_APPS - 5];
int dragging_slot = -1;
int offX = 0, offY = 0;

// Elementos Nativos da Interface
TForm* frmTaskbar  = NULL;
TLabel* lblStatus  = NULL;
TButton* btnStart  = NULL;

// Novos Elementos: Menu Iniciar
TForm* frmStartMenu = NULL;
TButton* btnAppTarefa = NULL;
TButton* btnAppGpci = NULL;
TButton* btnAppSerial = NULL;
TButton* btnAppTerminal = NULL;
TButton* btnAppGfile = NULL;

int is_start_menu_open = 0;

/* ============================================================================
 * ESTRUTURA ESTENDIDA DO IPC (Preservada para Teclado Virtual)
 * ============================================================================ */
typedef struct {
    uint8_t dummy[sizeof(AppWindowInfo)]; 
    volatile uint8_t fila_teclado_virtual;
    volatile uint8_t tem_evento_teclado;
} __attribute__((packed)) AppWindowInfoExtended;

/* ============================================================================
 * GERENCIAMENTO DE Z-INDEX (Isolado para slots 5 a 19)
 * ============================================================================ */
void init_z_stack() {
    // Inicializa a pilha apontando apenas para os slots físicos 5 a 19
    for(int i = 0; i < (MAX_EXTERNAL_APPS - 5); i++) {
        z_stack[i] = i + 5;
    }
}

void bring_to_front(int slot_index) {
    // Só traz para a frente se for um slot externo válido
    if (slot_index < 5 || slot_index >= MAX_EXTERNAL_APPS) return;

    int current_pos = -1;
    for(int i = 0; i < (MAX_EXTERNAL_APPS - 5); i++) {
        if(z_stack[i] == slot_index) { current_pos = i; break; }
    }
    if(current_pos == -1) return;
    
    for(int i = current_pos; i < (MAX_EXTERNAL_APPS - 5) - 1; i++) {
        z_stack[i] = z_stack[i+1];
    }
    z_stack[(MAX_EXTERNAL_APPS - 5) - 1] = slot_index;
}

/* ============================================================================
 * MOTOR DO COMPOSITOR
 * ============================================================================ */
void compose_app_window(int slot) {
    int active_idx = IPC_WINDOW_LIST[slot].active_buffer;
    uint32_t* app_fb = (active_idx == 0)
        ? (uint32_t*)(uintptr_t)IPC_WINDOW_LIST[slot].buffer_ptr_0
        : (uint32_t*)(uintptr_t)IPC_WINDOW_LIST[slot].buffer_ptr_1;
    
    if (!app_fb || !ram_buffer) return;

    int win_x = IPC_WINDOW_LIST[slot].x;
    int win_y = IPC_WINDOW_LIST[slot].y;
    int win_w = IPC_WINDOW_LIST[slot].width;
    int win_h = IPC_WINDOW_LIST[slot].height;

    int start_y = (win_y < 0) ? -win_y : 0;
    int end_y   = (win_y + win_h > screen_h) ? (screen_h - win_y) : win_h;

    int start_x = (win_x < 0) ? -win_x : 0;
    int end_x   = (win_x + win_w > screen_w) ? (screen_w - win_x) : win_w;

    int copy_w = end_x - start_x;
    if (copy_w <= 0) return; 

    for (int y = start_y; y < end_y; y++) {
        int target_y = win_y + y;
        uint8_t* dest_line = ram_buffer + (target_y * screen_pitch) + ((win_x + start_x) * bpp_bytes);
        uint32_t* src_line = app_fb + (y * win_w) + start_x;

        if (bpp_bytes == 4) {
            memcpy(dest_line, src_line, copy_w * 4);
        } else if (bpp_bytes == 3) {
            for (int x = 0; x < copy_w; x++) {
                uint32_t color = src_line[x];
                int off = x * 3;
                dest_line[off]   = color & 0xFF;
                dest_line[off+1] = (color >> 8) & 0xFF;
                dest_line[off+2] = (color >> 16) & 0xFF;
            }
        }
    }
}

void Coletar_Slots_Inativos(void) {
    ipc_lock(&IPC_CONTROL->lock);

    // Coleta apenas lixo dos slots de usuário (5 a 19)
    for (int i = 5; i < MAX_EXTERNAL_APPS; i++) {
        if (IPC_WINDOW_LIST[i].pid != 0 && IPC_WINDOW_LIST[i].is_active == 0) {
            if (IPC_CONTROL->active_focus_slot == i) IPC_CONTROL->active_focus_slot = -1;
            if (dragging_slot == i) dragging_slot = -1;

            IPC_WINDOW_LIST[i].pid = 0;             
            IPC_WINDOW_LIST[i].buffer_ptr_0 = 0;    
            IPC_WINDOW_LIST[i].buffer_ptr_1 = 0;    
            IPC_WINDOW_LIST[i].width = 0;
            IPC_WINDOW_LIST[i].height = 0;
            IPC_WINDOW_LIST[i].has_click_event = 0;
            IPC_WINDOW_LIST[i].title[0] = '\0';     
        }
    }

    int algum_app_ativo = 0;
    for (int i = 5; i < MAX_EXTERNAL_APPS; i++) {
        if (IPC_WINDOW_LIST[i].pid != 0 && IPC_WINDOW_LIST[i].is_active == 1) {
            algum_app_ativo = 1;
            break;
        }
    }

    if (!algum_app_ativo && lblStatus) {
        if (strcmp(lblStatus->Graphic.Control.Caption, "LBF-OS RUNTIME v5.7 [KERNEL 2.3 D.S.G]") != 0) {
            strcpy(lblStatus->Graphic.Control.Caption, "LBF-OS RUNTIME v5.7 [KERNEL 2.3 D.S.G]");
        }
    }
    ipc_unlock(&IPC_CONTROL->lock);
}

/* ============================================================================
 * PROCESSAR CLIQUE JANELA (Com active_click_slot inteligente)
 * ============================================================================ */
int Processar_Clique_Janela(mouse_t* m) {
    int clicked_slot = -1;
    ipc_lock(&IPC_CONTROL->lock);

    // Varre a pilha do Z-index correspondente aos 15 slots de usuário
    for (int i = (MAX_EXTERNAL_APPS - 5) - 1; i >= 0; i--) {
        int s = z_stack[i];
        if (IPC_WINDOW_LIST[s].is_active) {
            
            int win_x = (int)IPC_WINDOW_LIST[s].x;
            int win_y = (int)IPC_WINDOW_LIST[s].y;
            int win_w = (int)IPC_WINDOW_LIST[s].width;
            int win_h = (int)IPC_WINDOW_LIST[s].height;
            int mouse_x = (int)m->x;
            int mouse_y = (int)m->y;
            
            if (mouse_x >= win_x && mouse_x <= (win_x + win_w) &&
                mouse_y >= win_y && mouse_y <= (win_y + win_h)) {
                
                clicked_slot = s; // Salva o alvo exato do clique físico
                
                int local_x = mouse_x - win_x;
                int local_y = mouse_y - win_y;

                int eh_teclado = (strcmp((char*)IPC_WINDOW_LIST[s].title, "Teclado Virtual LBF") == 0 || 
                                  strcmp((char*)IPC_WINDOW_LIST[s].title, "Teclado Virtual OS v1.0") == 0);

                if (eh_teclado) {
                    if (local_y >= 6 && local_y <= 22 && local_x >= (win_w - 22) && local_x <= (win_w - 6)) {
                        IPC_WINDOW_LIST[s].is_active = 0; 
                        break; 
                    }
                    if (local_y > 25) { 
                        IPC_WINDOW_LIST[s].local_click_x = local_x;
                        IPC_WINDOW_LIST[s].local_click_y = local_y;
                        IPC_WINDOW_LIST[s].has_click_event = 1; 
                    } else {
                        dragging_slot = s;
                        offX = mouse_x - win_x;
                        offY = mouse_y - win_y;
                    }
                } 
                else {
                    if (IPC_CONTROL->active_focus_slot != s) {
                        IPC_CONTROL->active_focus_slot = s;
                        bring_to_front(s);
                        
                        if (lblStatus) {
                            char caption[128];
                            strcpy(caption, "Foco: ");
                            strcat(caption, (char*)IPC_WINDOW_LIST[s].title);
                            strcpy(lblStatus->Graphic.Control.Caption, caption);
                        }
                    }

                    if (local_y >= 6 && local_y <= 22 && local_x >= (win_w - 22) && local_x <= (win_w - 6)) {
                        IPC_WINDOW_LIST[s].is_active = 0; 
                        break; 
                    }

                    if (local_y <= 25) {
                        dragging_slot = s;
                        offX = mouse_x - win_x;
                        offY = mouse_y - win_y;
                    } 
                    else {
                        IPC_WINDOW_LIST[s].local_click_x = local_x;
                        IPC_WINDOW_LIST[s].local_click_y = local_y;
                        IPC_WINDOW_LIST[s].has_click_event = 1; 
                    }
                }
                break; 
            }
        }
    }
    ipc_unlock(&IPC_CONTROL->lock);
    return clicked_slot;
}

void Executar_Pipeline_Composicao(mouse_t* m) {
    graphics_clear(0x003A6EA5); 
    
    ipc_lock(&IPC_CONTROL->lock);
    // Renderiza as janelas dos apps na ordem correta da pilha z_stack (0 a 14)
    for (int i = 0; i < (MAX_EXTERNAL_APPS - 5); i++) {
        int s = z_stack[i];
        if (IPC_WINDOW_LIST[s].is_active) {
            compose_app_window(s);
        }
    }
    ipc_unlock(&IPC_CONTROL->lock);

    wm_render_pipeline();     

    if (is_start_menu_open && frmStartMenu) {
        gui_draw_form(frmStartMenu);
        gui_render_form(frmStartMenu);
    }
    
    cursor_draw(m->x, m->y, screen_w, screen_h);
    video_flip(ram_buffer);
}

void Setup_Interface_Nativa(void) {
    frmTaskbar = gui_create_form("frmTaskbar", "Taskbar", 1);
    if (frmTaskbar) {
        frmTaskbar->BorderStyle = bsNone;
        frmTaskbar->Win.Control.Left = 0;
        frmTaskbar->Win.Control.Top = screen_h - 40;
        frmTaskbar->Win.Control.Width = screen_w;
        frmTaskbar->Win.Control.Height = 40;
        frmTaskbar->Win.Control.Color = 0xC0C0C0;
        frmTaskbar->Win.Control.Visible = 1;
        
        btnStart = gui_create_button(&frmTaskbar->Win, "btnStart", "Iniciar");
        if (btnStart) {
            btnStart->Win.Control.Left = 2;
            btnStart->Win.Control.Top = 2;
            btnStart->Win.Control.Width = 80;
            btnStart->Win.Control.Height = 36;
        }
        wm_set_desktop(frmTaskbar); 
    }
    
    lblStatus = gui_create_label((TWinControl*)frmTaskbar, "lblStatus", "LBF-OS RUNTIME v5.7 [KERNEL 2.3 D.S.G]");
    if (lblStatus) {
        lblStatus->Graphic.Control.Left = 100;
        lblStatus->Graphic.Control.Top = 12;
    }

    frmStartMenu = gui_create_form("frmStartMenu", "StartMenu", 1);
    if (frmStartMenu) {
        frmStartMenu->BorderStyle = bsNone;
        frmStartMenu->Win.Control.Width = 320;  
        frmStartMenu->Win.Control.Height = 220; 
        frmStartMenu->Win.Control.Left = 0;
        frmStartMenu->Win.Control.Top = screen_h - 40 - frmStartMenu->Win.Control.Height;
        frmStartMenu->Win.Control.Color = 0xC0C0C0;
        frmStartMenu->Win.Control.Visible = 0; 
        
        btnAppTarefa = gui_create_button(&frmStartMenu->Win, "btnAppTarefa", "Gerenciador de Tarefas");
        btnAppTarefa->Win.Control.Left = 5; btnAppTarefa->Win.Control.Top = 5;
        btnAppTarefa->Win.Control.Width = 310; btnAppTarefa->Win.Control.Height = 35;

        btnAppGpci = gui_create_button(&frmStartMenu->Win, "btnAppGpci", "Gerenciador Dispositivos");
        btnAppGpci->Win.Control.Left = 5; btnAppGpci->Win.Control.Top = 45;
        btnAppGpci->Win.Control.Width = 310; btnAppGpci->Win.Control.Height = 35;

        btnAppSerial = gui_create_button(&frmStartMenu->Win, "btnAppSerial", "Monitor Serial");
        btnAppSerial->Win.Control.Left = 5; btnAppSerial->Win.Control.Top = 85;
        btnAppSerial->Win.Control.Width = 310; btnAppSerial->Win.Control.Height = 35;

        btnAppTerminal = gui_create_button(&frmStartMenu->Win, "btnAppTerminal", "Terminal OS");
        btnAppTerminal->Win.Control.Left = 5; btnAppTerminal->Win.Control.Top = 125;
        btnAppTerminal->Win.Control.Width = 310; btnAppTerminal->Win.Control.Height = 35;

        btnAppGfile = gui_create_button(&frmStartMenu->Win, "btnAppGfile", "Gerenciador de Arquivos");
        btnAppGfile->Win.Control.Left = 5; btnAppGfile->Win.Control.Top = 165;
        btnAppGfile->Win.Control.Width = 310; btnAppGfile->Win.Control.Height = 35;
    }
}

/* ============================================================================
 * LOOP PRINCIPAL
 * ============================================================================ */
int main() {
    graphics_init();  
    if (screen_w == 0 || screen_h == 0) sys_exit();
    
    wm_init();
    init_z_stack();
    Setup_Interface_Nativa();

    static int was_clicked = 0;
    static int active_click_slot = -1; // Guarda quem sofreu o clique inicial

    while(1) {
        mouse_t m; 
        get_mouse(&m);
        
        /* =========================================================================
         * VERIFICAÇÃO ATIVA DO PROCESSO (Gatilho de Bypass do Slot 16)
         * ========================================================================= */
        // Limpa o vetor temporário local antes da consulta do Kernel
      /*  for (int z = 0; z < 32; z++) {
           lista_verificacao[z].pid = 0;
        }*/

        // --- ROTEADOR CENTRAL DE TECLADO ---
        char key = get_key();
        if (key != 0) {
            ipc_lock(&IPC_CONTROL->lock);
            int focus_slot = IPC_CONTROL->active_focus_slot;
            if (focus_slot != -1 && IPC_WINDOW_LIST[focus_slot].is_active) {
                AppWindowInfoExtended* ext_slot = (AppWindowInfoExtended*)&IPC_WINDOW_LIST[focus_slot];
                ext_slot->fila_teclado_virtual = (uint8_t)key;
                ext_slot->tem_evento_teclado = 1;
            }
            ipc_unlock(&IPC_CONTROL->lock);
        }

        Coletar_Slots_Inativos();

        if (m.buttons & 1) { 
            if (!was_clicked) {
                was_clicked = 1;
                active_click_slot = -1; 
                int click_handled = 0;

                int startBtn_X = frmTaskbar->Win.Control.Left + btnStart->Win.Control.Left;
                int startBtn_Y = frmTaskbar->Win.Control.Top + btnStart->Win.Control.Top;
                
                if (m.x >= startBtn_X && m.x <= (startBtn_X + btnStart->Win.Control.Width) &&
                    m.y >= startBtn_Y && m.y <= (startBtn_Y + btnStart->Win.Control.Height)) {
                    is_start_menu_open = !is_start_menu_open;
                    frmStartMenu->Win.Control.Visible = is_start_menu_open;
                    click_handled = 1;
                }
                else if (is_start_menu_open && 
                         m.x >= frmStartMenu->Win.Control.Left && m.x <= (frmStartMenu->Win.Control.Left + frmStartMenu->Win.Control.Width) &&
                         m.y >= frmStartMenu->Win.Control.Top && m.y <= (frmStartMenu->Win.Control.Top + frmStartMenu->Win.Control.Height)) {
                    
                    int local_y = m.y - frmStartMenu->Win.Control.Top;
                    if (local_y >= btnAppTarefa->Win.Control.Top && local_y <= (btnAppTarefa->Win.Control.Top + btnAppTarefa->Win.Control.Height)) { sys_exec("tarefa.elf"); }
                    else if (local_y >= btnAppGpci->Win.Control.Top && local_y <= (btnAppGpci->Win.Control.Top + btnAppGpci->Win.Control.Height)) { sys_exec("gpci.elf"); }
                    else if (local_y >= btnAppSerial->Win.Control.Top && local_y <= (btnAppSerial->Win.Control.Top + btnAppSerial->Win.Control.Height)) { sys_exec("serial.elf"); }
                    else if (local_y >= btnAppTerminal->Win.Control.Top && local_y <= (btnAppTerminal->Win.Control.Top + btnAppTerminal->Win.Control.Height)) { sys_exec("terminal.elf"); }
                    else if (local_y >= btnAppGfile->Win.Control.Top && local_y <= (btnAppGfile->Win.Control.Top + btnAppGfile->Win.Control.Height)) { sys_exec("gfile.elf"); }
                    
                    is_start_menu_open = 0;
                    frmStartMenu->Win.Control.Visible = 0;
                    click_handled = 1;
                }
                else if (is_start_menu_open) {
                    is_start_menu_open = 0;
                    frmStartMenu->Win.Control.Visible = 0;
                }

                if (!click_handled && dragging_slot == -1) {
                    active_click_slot = Processar_Clique_Janela(&m);
                }

            } else {
                ipc_lock(&IPC_CONTROL->lock);
                if (dragging_slot != -1) {
                    IPC_WINDOW_LIST[dragging_slot].x = (int)m.x - offX;
                    IPC_WINDOW_LIST[dragging_slot].y = (int)m.y - offY;
                } else if (active_click_slot != -1) {
                    int s = active_click_slot;
                    if (IPC_WINDOW_LIST[s].is_active) {
                        IPC_WINDOW_LIST[s].local_click_x = (int)m.x - (int)IPC_WINDOW_LIST[s].x;
                        IPC_WINDOW_LIST[s].local_click_y = (int)m.y - (int)IPC_WINDOW_LIST[s].y;
                        IPC_WINDOW_LIST[s].has_click_event = 1; 
                    }
                }
                ipc_unlock(&IPC_CONTROL->lock);
            }
        } else {
            was_clicked = 0;
            dragging_slot = -1;
            active_click_slot = -1; 
        }

        Executar_Pipeline_Composicao(&m);
        sys_sleep(10);
    }
}
