#include "sdk/libgui.h"
#include "../system/graphics.h"
#include "../gui/wm.h"
#include "../system/string.h"
#include "../system/liblib.h"

// Componentes do Sistema encapsulados
#include "components/TOS_IPC.h"     

// Protótipos de renderização gráfica
void gui_draw_form(TForm* form);
void gui_render_form(TForm* form);

// Funções externas de controle de hardware e foco
extern void events_process_mouse(int x, int y, int pressed, int button);
extern void* g_focused_control;

// Protótipos de manipulação de Memo na SDK
extern void GUI_Memo_AddStr(TGUIControl* memo, const char* str);
extern void GUI_Memo_Clear(TGUIControl* memo);

// Variáveis de controle do ambiente da aplicação
TProcessInfo lista_ps[6];
int my_app_slot = -1;
TGUIEnvironment MyApp;

// Configurações de Dimensão (Tamanho menor padrão)
const int winWidth = 550;
const int winHeight = 410;

// Ponteiros de Controle RAD
TGUIControl* ExeMemo      = NULL;
TGUIControl* EditPID      = NULL;
TGUIControl* EditPath     = NULL;
TGUIControl* BtnAtualizar = NULL;
TGUIControl* BtnKillUnico = NULL;
TGUIControl* BtnExecutar  = NULL;

/* ============================================================================
 * ESTRUTURA AUXILIAR IPC (Mapeamento de Eventos Estendidos)
 * ============================================================================ */
typedef struct {
    uint8_t dummy[sizeof(IPC_WINDOW_LIST[0])]; 
    volatile uint8_t fila_teclado_virtual;
    volatile uint8_t tem_evento_teclado;
} __attribute__((packed)) AppWindowInfoExtended;

/* ============================================================================
 * FUNÇÕES AUXILIARES E DE AMBIENTE
 * ============================================================================ */

/**
 * Captura as teclas injetadas na caixa de correio IPC pelo explorer.elf
 */
char Obter_Tecla_Entrada(void) {
    AppWindowInfoExtended* ext_slot = (AppWindowInfoExtended*)&IPC_WINDOW_LIST[my_app_slot];

    if (ext_slot->tem_evento_teclado == 1) {
        char key = (char)ext_slot->fila_teclado_virtual;
        ext_slot->tem_evento_teclado = 0; 
        return key;
    }
    return 0;
}

/* ============================================================================
 * FUNÇÃO: Flush_Grafico_Janela
 * ============================================================================ */
void Flush_Grafico_Janela(void) {
    // 1. Desenha os componentes internos do aplicativo (RAD)
    gui_draw_form((TForm*)MyApp.MainWindow);
    gui_render_form((TForm*)MyApp.MainWindow);
        
    // 2. Transfere os buffers via componente IPC
    OS_IPC_FlipBuffers(my_app_slot, winWidth, winHeight);
}

/**
 * Executa o encerramento seguro limpando buffers compartilhados da memória
 */
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
            
    GUI_Memo_Clear(ExeMemo);
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
        for (int i = 0; i < (MAX_EXTERNAL_APPS - 5); i++) {
            if (IPC_WINDOW_LIST[i].pid == (uint64_t)pid_alvo && IPC_WINDOW_LIST[i].is_active == 1) {
                IPC_WINDOW_LIST[i].is_active = 0; 
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

    // Dispara a execução assíncrona para a task_d no Ring 0
    sys_exec(caminho_elf);

    // Limpa o campo de texto e notifica a interface
    GUI_Edit_SetText(EditPath, "");
    GUI_Memo_AddStr(ExeMemo, "Solicitacao de execucao enviada...\n");
}

/* ============================================================================
 * FUNÇÃO PRINCIPAL (MAIN)
 * ============================================================================ */
int main(int argc, char* argv[]) {
    static int ultimo_x = 0;
    static int ultimo_y = 0;
    static int mouse_hold_timer = 0; 

    static bool primeiro_desenho = true;
    static bool ultimo_estado_foco = false;
    void* ultimo_controle_focado = NULL;

    // Inicialização de subsistemas gráficos e registro IPC
    graphics_init_app(winWidth, winHeight);
    wm_init();
    
    my_app_slot = OS_IPC_RegisterApp("Gerenciador de Tarefas LBF", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "Gerenciador de Tarefas LBF v0.0.3", winWidth, winHeight);

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x000000);
    }

    /* =========================================================================
     * DESIGN DO LAYOUT DE COMPONENTES
     * ========================================================================= */
    int btnW = 210; 
    int editW = 180; 
    int ctrlH = 30;

    BtnAtualizar = GUI_CreateButton(&MyApp, 10, 40, btnW, ctrlH, "ATUALIZAR LISTA", OnBtnAtualizarClick);
    
    ExeMemo = GUI_CreateMemo(&MyApp, 10, 80, 530, 190);
    gui_set_prop(ExeMemo, PROP_COLOR, 0x000000);
    GUI_Memo_AddStr(ExeMemo, "Clique em 'ATUALIZAR LISTA' para carregar os processos.\n");

    BtnKillUnico = GUI_CreateButton(&MyApp, 10, 300, btnW, ctrlH, "FINALIZAR PROCESSO", OnBtnKillClick);
    EditPID      = GUI_CreateEdit(&MyApp, 290, 300, editW, ctrlH, "", NULL);

    BtnExecutar  = GUI_CreateButton(&MyApp, 10, 360, btnW, ctrlH, "EXECUTAR PROCESSO", OnBtnExecutarClick);
    EditPath     = GUI_CreateEdit(&MyApp, 290, 360, editW, ctrlH, "", NULL);

    // Configuração e gravação do Foco Inicial Padrão no Edit de Execução
    g_focused_control = (void*)EditPath;
    ultimo_controle_focado = (void*)EditPath;
    gui_set_prop(EditPath, PROP_SET_FOCUS, 1);

    // REMOVIDO: OnBtnAtualizarClick(NULL); 
    // A busca de processos agora é feita estritamente manual via clique no botão.

    Flush_Grafico_Janela();

    /* =========================================================================
     * LOOP DE EVENTOS CONTINUO
     * ========================================================================= */
    while(1) {
        if (IPC_WINDOW_LIST[my_app_slot].is_active == 0) {
            Tratar_Fechamento_Software();
            break;
        }

        bool precisa_redesenhar = false;

        // Força redesenho no frame inicial
        if (primeiro_desenho) {
            primeiro_desenho = false;
            precisa_redesenhar = true;
        }

        // Verifica dinamicamente se a janela real recebeu ou perdeu foco no S.O.
        bool euTenhoFocoJanelaReal = (IPC_CONTROL->active_focus_slot == my_app_slot);
        if (euTenhoFocoJanelaReal != ultimo_estado_foco) {
            ultimo_estado_foco = euTenhoFocoJanelaReal;
            
            // Sincroniza a propriedade da janela interna para acompanhar o Frame do S.O.
            if (MyApp.MainWindow) {
                ((TForm*)MyApp.MainWindow)->ActiveFocus = euTenhoFocoJanelaReal; 
            }
            precisa_redesenhar = true;
        }

        // --- SISTEMA DE MANUTENÇÃO E PROTEÇÃO DO CURSOR PISCANDO ---
        if (g_focused_control != NULL) {
            ultimo_controle_focado = g_focused_control;
        } else if (ultimo_controle_focado != NULL) {
            g_focused_control = ultimo_controle_focado;
            gui_set_prop((TGUIControl*)ultimo_controle_focado, PROP_SET_FOCUS, 1);
        }

        // --- ENTRADA CENTRALIZADA DE TECLADO (IPC) ---
        char key = Obter_Tecla_Entrada();
        if (key != 0) {
            GUI_ProcessKeyboard(&MyApp, key); 
            precisa_redesenhar = true; 
        }

        // --- SISTEMA DE ROTEAMENTO DE CLIQUE DO MOUSE ---
        if (IPC_WINDOW_LIST[my_app_slot].has_click_event == 1) {
            if (mouse_hold_timer == 0) {
                int rel_x = IPC_WINDOW_LIST[my_app_slot].local_click_x;
                int rel_y = IPC_WINDOW_LIST[my_app_slot].local_click_y;
                ultimo_x = rel_x;
                ultimo_y = rel_y;
                mouse_hold_timer = 2; 

                // Feedback visual de clique pressionado nos botões
                if (BtnAtualizar && rel_x >= BtnAtualizar->Left && rel_x < (BtnAtualizar->Left + BtnAtualizar->Width) &&
                    rel_y >= BtnAtualizar->Top && rel_y < (BtnAtualizar->Top + BtnAtualizar->Height)) {
                    gui_set_prop(BtnAtualizar, PROP_STATE, 2);
                }
                else if (BtnKillUnico && rel_x >= BtnKillUnico->Left && rel_x < (BtnKillUnico->Left + BtnKillUnico->Width) &&
                         rel_y >= BtnKillUnico->Top && rel_y < (BtnKillUnico->Top + BtnKillUnico->Height)) {
                    gui_set_prop(BtnKillUnico, PROP_STATE, 2);
                }
                else if (BtnExecutar && rel_x >= BtnExecutar->Left && rel_x < (BtnExecutar->Left + BtnExecutar->Width) &&
                         rel_y >= BtnExecutar->Top && rel_y < (BtnExecutar->Top + BtnExecutar->Height)) {
                    gui_set_prop(BtnExecutar, PROP_STATE, 2);
                }

                events_process_mouse(rel_x, rel_y, 1, 0);
                
                if (GUI_ProcessMouseClick(&MyApp, rel_x, rel_y)) {
                    precisa_redesenhar = true;
                    if (g_focused_control != NULL) {
                        ultimo_controle_focado = g_focused_control;
                    }
                }
            }
            IPC_WINDOW_LIST[my_app_slot].has_click_event = 0;
        }

        // Timer para gerenciar o estado desfeito do clique do mouse (Release)
        if (mouse_hold_timer > 0) {
            mouse_hold_timer--; 
            if (mouse_hold_timer == 0) {
                if (BtnAtualizar) gui_set_prop(BtnAtualizar, PROP_STATE, 0); 
                if (BtnKillUnico) gui_set_prop(BtnKillUnico, PROP_STATE, 0); 
                if (BtnExecutar)  gui_set_prop(BtnExecutar, PROP_STATE, 0); 
                events_process_mouse(ultimo_x, ultimo_y, 0, 0); 
                precisa_redesenhar = true;
            }
        }

        // Atualização gráfica da janela na tela
        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        // Foco ativo = 16ms (~60 FPS), Segundo Plano = 32ms (~30 FPS)
        sys_sleep(euTenhoFocoJanelaReal ? 16 : 32); 
    }

    sys_exit(); 
    return 0;
}
