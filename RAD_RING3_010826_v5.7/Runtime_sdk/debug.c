#include "sdk/libgui.h"
#include "../system/graphics.h"
#include "../gui/wm.h"
#include "../system/string.h"
#include "../system/liblib.h"

// Componentes do Sistema encapsulados (IPC / Hardware)
#include "components/TOS_IPC.h"     
#include "components/TOSSerial.h"   

// Protótipos de renderização gráfica do ecossistema moderno
void gui_draw_form(TForm* form);
void gui_render_form(TForm* form);

// Funções externas do sistema de foco e componentes
extern void events_process_mouse(int x, int y, int pressed, int button);
extern void* g_focused_control;

// Protótipos de manipulação de Memo na SDK
extern void GUI_Memo_AddStr(TGUIControl* memo, const char* str);
extern void GUI_Memo_Clear(TGUIControl* memo);

// Variáveis de controle da aplicação
int my_app_slot = -1;
TGUIEnvironment MyApp;

// Configurações Globais de Dimensão
const int winWidth = 600;
const int winHeight = 400;

// Componentes RAD do Monitor de Log
TGUIControl* LogMemo = NULL;
TGUIControl* BtnClear = NULL;

// Buffer local para recepção da Syscall 69
#define KLOG_TEMP_BUF_SIZE 512
static char klog_buffer[KLOG_TEMP_BUF_SIZE];

/* ============================================================================
 * FUNÇÃO: Flush_Grafico_Janela
 * ============================================================================ */
void Flush_Grafico_Janela(void) {
    // 1. Desenha os componentes internos do aplicativo (RAD)
    gui_draw_form((TForm*)MyApp.MainWindow);
    gui_render_form((TForm*)MyApp.MainWindow);
        
    // 2. Envia os dados para o compositor gráfico (Flip Buffers)
    OS_IPC_FlipBuffers(my_app_slot, winWidth, winHeight);
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
void OnBtnClearClick(void* sender) {
    GUI_Memo_Clear(LogMemo);
    GUI_Memo_AddStr(LogMemo, "[SYSTEM] Buffer de exibicao limpo.\n");
    GUI_Memo_AddStr(LogMemo, "--------------------------------------------------\n");
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

    // Inicialização do Subsistema Gráfico e Window Manager
    graphics_init_app(winWidth, winHeight);
    wm_init();
    
    my_app_slot = OS_IPC_RegisterApp("Kernel Log Monitor", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "Kernel Live Debug Monitor v1.1", winWidth, winHeight);

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x000000);
    }

    // --- MONTAGEM DA INTERFACE RAD ---
    GUI_CreateLabel(&MyApp, 10, 45, "Logs do Kernel em Tempo Real (Ring 0 -> Ring 3):");
    
    // Adiciona o botão de Limpar Log alinhado à direita
    BtnClear = GUI_CreateButton(&MyApp, 450, 38, 140, 30, "LIMPAR LOG", OnBtnClearClick);
    
    LogMemo = GUI_CreateMemo(&MyApp, 10, 75, 580, 315);
    gui_set_prop(LogMemo, PROP_COLOR, 0x000000); 
    
    GUI_Memo_AddStr(LogMemo, "[SYSTEM] Conectado ao Buffer de Debug do Kernel (Syscall 69)...\n");
    GUI_Memo_AddStr(LogMemo, "--------------------------------------------------\n");

    // Configuração de Foco
    g_focused_control = (void*)LogMemo;
    gui_set_prop(LogMemo, PROP_SET_FOCUS, 1);

    /* =========================================================================
     * LOOP DE EVENTOS EM TEMPO REAL
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

        // Força a renderização inicial
        if (primeiro_desenho) {
            primeiro_desenho = false;
            precisa_redesenhar = true;
        }

        // Se o aplicativo mudou de foco, atualiza a moldura
        if (euTenhoFoco != ultimo_estado_foco) {
            ultimo_estado_foco = euTenhoFoco;
            precisa_redesenhar = true;
        }

        /* =====================================================================
         * CAPTURA DE LOGS DO KERNEL (TEMPO REAL VIA SYSCALL 69)
         * =====================================================================
         * Executa independente de a janela estar em foco ou nao, garantindo
         * que os logs continuem acumulando enquanto o software esta aberto!
         */
        int bytes_lidos = sys_read_kernel_log(klog_buffer, KLOG_TEMP_BUF_SIZE - 1);
        if (bytes_lidos > 0) {
            klog_buffer[bytes_lidos] = '\0'; // Garante o encerramento da string
            GUI_Memo_AddStr(LogMemo, klog_buffer);
            precisa_redesenhar = true; // Se chegou mensagem nova do Ring 0, redesenha!
        }

        // --- Sistema de Roteamento de Clique do Mouse ---
        if (IPC_WINDOW_LIST[my_app_slot].has_click_event == 1) {
            if (mouse_hold_timer == 0) {
                int rel_x = IPC_WINDOW_LIST[my_app_slot].local_click_x;
                int rel_y = IPC_WINDOW_LIST[my_app_slot].local_click_y;
                
                ultimo_x = rel_x;
                ultimo_y = rel_y;
                mouse_hold_timer = 2; 

                // Feedback visual do botão
                if (BtnClear && rel_x >= BtnClear->Left && rel_x < (BtnClear->Left + BtnClear->Width) &&
                    rel_y >= BtnClear->Top && rel_y < (BtnClear->Top + BtnClear->Height)) {
                    gui_set_prop(BtnClear, PROP_STATE, 2);
                }

                events_process_mouse(rel_x, rel_y, 1, 0);

                if (GUI_ProcessMouseClick(&MyApp, rel_x, rel_y)) {
                    precisa_redesenhar = true;
                }
            }
            IPC_WINDOW_LIST[my_app_slot].has_click_event = 0;
        }

        // --- Timer de Liberação do Mouse ---
        if (mouse_hold_timer > 0) {
            mouse_hold_timer--; 
            if (mouse_hold_timer == 0) {
                if (BtnClear) gui_set_prop(BtnClear, PROP_STATE, 0);

                events_process_mouse(ultimo_x, ultimo_y, 0, 0); 
                precisa_redesenhar = true;
            }
        }

        // Se houve nova mensagem do Kernel ou interacao, atualiza o frame na tela
        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        // Foco ativo = 16ms (~60 FPS), Segundo Plano = 32ms (~30 FPS)
        sys_sleep(euTenhoFoco ? 16 : 32);
    }

    sys_exit(); 
    return 0;
}
