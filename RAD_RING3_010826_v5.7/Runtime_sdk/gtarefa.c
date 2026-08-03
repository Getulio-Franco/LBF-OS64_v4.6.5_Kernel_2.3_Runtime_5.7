#include "sdk/libgui.h"
#include "../system/graphics.h"
#include "../gui/wm.h"
#include "../system/string.h"
#include "../system/liblib.h"

#include "components/TOS_IPC.h"     
#include "components/TOSSerial.h"   

void gui_draw_form(TForm* form);
void gui_render_form(TForm* form);

extern void events_process_mouse(int x, int y, int pressed, int button);
extern void* g_focused_control;

extern void GUI_Memo_AddStr(TGUIControl* memo, const char* str);
extern void GUI_Memo_Clear(TGUIControl* memo);

TProcessInfo lista_ps[6];
int my_app_slot = -1;
TGUIEnvironment MyApp;

const int winWidth = 800;
const int winHeight = 600;

TGUIControl* ExeMemo    = NULL;
TGUIControl* EditPID    = NULL;
TGUIControl* EditPath   = NULL;

/* ============================================================================
 * FUNÇÃO: Flush_Grafico_Janela
 * ============================================================================ */
void Flush_Grafico_Janela(void) {
    gui_draw_form((TForm*)MyApp.MainWindow);
    gui_render_form((TForm*)MyApp.MainWindow);
        
    int back_idx = (IPC_WINDOW_LIST[my_app_slot].active_buffer == 0) ? 1 : 0;
    uint8_t* shared_ptr = (back_idx == 0) 
        ? (uint8_t*)(uintptr_t)IPC_WINDOW_LIST[my_app_slot].buffer_ptr_0 
        : (uint8_t*)(uintptr_t)IPC_WINDOW_LIST[my_app_slot].buffer_ptr_1;
        
    uint8_t* local_ptr = graphics_get_buffer();
    if (shared_ptr && local_ptr) {
        memcpy(shared_ptr, local_ptr, winWidth * winHeight * 4); 
    }
    IPC_WINDOW_LIST[my_app_slot].active_buffer = back_idx;
}

/* ============================================================================
 * FUNÇÃO: Tratar_Fechamento_Software
 * ============================================================================ */
void Tratar_Fechamento_Software(void) {
    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_VISIBLE, 0);
    }
    uint32_t* b0 = (uint32_t*)(uintptr_t)IPC_WINDOW_LIST[my_app_slot].buffer_ptr_0;
    uint32_t* b1 = (uint32_t*)(uintptr_t)IPC_WINDOW_LIST[my_app_slot].buffer_ptr_1;
    if (b0) memset(b0, 0, winWidth * winHeight * 4);
    if (b1) memset(b1, 0, winWidth * winHeight * 4);
    IPC_WINDOW_LIST[my_app_slot].is_active = 0;
    sys_sleep(50); 
}

/* ============================================================================
 * CALLBACKS DE EVENTOS RAD
 * ============================================================================ */
void OnBtnAtualizarClick(void* sender) {
    for(int z = 0; z < 6; z++) lista_ps[z].pid = 0;
    int qtd_processos = sys_get_ps_data(lista_ps, 6);
            
    GUI_Memo_AddStr(ExeMemo, "--- LISTA ATUALIZADA ---\n");
    char qtd_str[10];
    itoa((uint64_t)qtd_processos, qtd_str, 10); 
    GUI_Memo_AddStr(ExeMemo, "Processos ativos: ");
    GUI_Memo_AddStr(ExeMemo, qtd_str);
    GUI_Memo_AddStr(ExeMemo, "\n");

    for(int k = 0; k < 6; k++) {
        if(lista_ps[k].pid != 0) {
            char pid_str[10];
            itoa(lista_ps[k].pid, pid_str, 10); 
            GUI_Memo_AddStr(ExeMemo, "PID: ");
            GUI_Memo_AddStr(ExeMemo, pid_str);
            GUI_Memo_AddStr(ExeMemo, " | Nome: ");
            GUI_Memo_AddStr(ExeMemo, lista_ps[k].name);
            GUI_Memo_AddStr(ExeMemo, "\n");
        }
    }
}

void OnBtnKillClick(void* sender) {
    char* texto_pid = GUI_Edit_GetText(EditPID);
    if (!texto_pid || texto_pid[0] == '\0') return;

    int pid_alvo = atoi(texto_pid); 
    if (pid_alvo >= 5) {
        // CORREÇÃO: Varre apenas os slots reais de aplicativos externos (limite idêntico ao TOS_IPC)
        for (int i = 0; i < (MAX_EXTERNAL_APPS - 5); i++) {
            if (IPC_WINDOW_LIST[i].pid == (uint64_t)pid_alvo && IPC_WINDOW_LIST[i].is_active == 1) {
                IPC_WINDOW_LIST[i].is_active = 0; // Finaliza o contêiner de forma segura
                break;
            }
        }
        sys_sleep(20);
        sys_kill((uint64_t)pid_alvo);
        GUI_Edit_SetText(EditPID, "");
    }
}

void OnBtnExecutarClick(void* sender) {
    char* caminho_elf = GUI_Edit_GetText(EditPath);
    if (!caminho_elf || caminho_elf[0] == '\0') return;
    
    int retorno_exec = sys_exec(caminho_elf);
    if (retorno_exec == 0) {
        GUI_Edit_SetText(EditPath, "");
        GUI_Memo_AddStr(ExeMemo, "Executando binario...\n");
    }
}

void OnBtnExecutarClick(void* sender) {
    char* caminho_elf = GUI_Edit_GetText(EditPath);
    if (!caminho_elf || caminho_elf[0] == '\0') return;

    // Dispara a execução assíncrona para a task_d no Ring 0
    sys_exec(caminho_elf);

    // Limpa o campo de texto e notifica a interface
    GUI_Edit_SetText(EditPath, "");
    GUI_Memo_AddStr(ExeMemo, "Solicitacao de execucao enviada...\n");

    // Pequena pausa para dar tempo da task_d instanciar o processo antes da atualização da lista
   // sys_sleep(50);

    // Auto-atualiza a lista de processos na janela do Gerenciador
  //  OnBtnAtualizarClick(NULL);
}

/* ============================================================================
 * FUNÇÃO PRINCIPAL (MAIN)
 * ============================================================================ */
int main(int argc, char* argv[]) {
    static int ultimo_x = 0;
    static int ultimo_y = 0;
    static int mouse_hold_timer = 0; 

    // Variáveis de controle de estado para renderização inteligente
    static bool primeiro_desenho = true;
    static bool ultimo_estado_foco = false;

    graphics_init_app(winWidth, winHeight);
    wm_init();
    
    my_app_slot = OS_IPC_RegisterApp("Gerenciador de Tarefas LBF", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "Gerenciador de Tarefas LBF v0.0.2", winWidth, winHeight);

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x000000); // Fundo preto
    }

    // --- NOVO LAYOUT EXPANDIDO (800x600) ---
    int btnW = 210; int editW = 180; int ctrlH = 30;
    
    // Botão de atualizar no topo
    TGUIControl* BtnAtualizar = GUI_CreateButton(&MyApp, 10, 40, btnW, ctrlH, "ATUALIZAR LISTA", OnBtnAtualizarClick);
    
    // Memo (Lista) agora ocupa uma área gigante no centro da tela
    // Largura: 780 (deixa 10px de margem de cada lado) | Altura: 350
    ExeMemo = GUI_CreateMemo(&MyApp, 10, 80, 780, 350);
    GUI_Memo_AddStr(ExeMemo, "Gerenciador Inicializado em Alta Resolucao (800x600).\n");

    // Controles movidos para a parte inferior da tela
    TGUIControl* BtnKillUnico = GUI_CreateButton(&MyApp, 10, 460, btnW, ctrlH, "FINALIZAR PROCESSO", OnBtnKillClick);
    EditPID = GUI_CreateEdit(&MyApp, 230, 460, editW, ctrlH, "", NULL);

    TGUIControl* BtnExecutar = GUI_CreateButton(&MyApp, 10, 510, btnW, ctrlH, "EXECUTAR PROCESSO", OnBtnExecutarClick);
    EditPath = GUI_CreateEdit(&MyApp, 230, 510, editW, ctrlH, "", NULL);

    // Renderização inicial
    Flush_Grafico_Janela();

    /* =========================================================================
     * LOOP DE EVENTOS REATIVO (Correção de foco e pintura de borda externa)
     * ========================================================================= */
    while(1) {
        if (IPC_WINDOW_LIST[my_app_slot].is_active == 0) {
            Tratar_Fechamento_Software();
            break;
        }

        bool euTenhoFoco = (IPC_CONTROL->active_focus_slot == my_app_slot);
        if (MyApp.MainWindow) {
            ((TForm*)MyApp.MainWindow)->ActiveFocus = euTenhoFoco;
        }

        bool precisa_redesenhar = false;

        // Gatilho do Cold-Start
        if (primeiro_desenho) {
            primeiro_desenho = false;
            precisa_redesenhar = true;
        }

        // CORREÇÃO CRUCIAL: Se o foco mudou (ganhou ou perdeu), força o redesenho imediatamente!
        if (euTenhoFoco != ultimo_estado_foco) {
            ultimo_estado_foco = euTenhoFoco;
            precisa_redesenhar = true;
        }

        if (euTenhoFoco) {
            // --- Teclado ---
            char key = get_key();
            if (key != 0) {
                GUI_ProcessKeyboard(&MyApp, key); 
                precisa_redesenhar = true; 
            }

            // --- Mouse Click ---
            if (IPC_WINDOW_LIST[my_app_slot].has_click_event == 1) {
                if (mouse_hold_timer == 0) {
                    int rel_x = IPC_WINDOW_LIST[my_app_slot].local_click_x;
                    int rel_y = IPC_WINDOW_LIST[my_app_slot].local_click_y;
                    ultimo_x = rel_x;
                    ultimo_y = rel_y;
                    mouse_hold_timer = 2; 

                    events_process_mouse(rel_x, rel_y, 1, 0);
                    
                    if (GUI_ProcessMouseClick(&MyApp, rel_x, rel_y)) {
                        precisa_redesenhar = true;
                    }
                }
                IPC_WINDOW_LIST[my_app_slot].has_click_event = 0;
            }

            // --- Mouse Release ---
            if (mouse_hold_timer > 0) {
                mouse_hold_timer--; 
                if (mouse_hold_timer == 0) {
                    gui_set_prop(BtnAtualizar, PROP_STATE, 0); 
                    gui_set_prop(BtnKillUnico, PROP_STATE, 0); 
                    gui_set_prop(BtnExecutar, PROP_STATE, 0); 
                    events_process_mouse(ultimo_x, ultimo_y, 0, 0); 
                    precisa_redesenhar = true;
                }
            }
        }

        // MUDANÇA ESSENCIAL: Fora do bloco "if (euTenhoFoco)" para capturar a perda de foco!
        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        sys_sleep(euTenhoFoco ? 16 : 32);
    }

    sys_exit(); 
    return 0;
}
