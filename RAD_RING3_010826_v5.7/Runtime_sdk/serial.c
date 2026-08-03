#include "sdk/libgui.h"
#include "../system/graphics.h"
#include "../gui/wm.h"
#include "../system/liblib.h"
#include "../system/string.h"

// Inclusão dos novos componentes encapsulados
#include "components/TOS_IPC.h"     
#include "components/TOSSerial.h"   

void gui_draw_form(TForm* form);
void gui_render_form(TForm* form);

// Inclusão de funções de controle mapeadas diretamente no subsistema do Kernel
extern void events_process_mouse(int x, int y, int pressed, int button);
extern void* g_focused_control;

// Protótipos das funções do TMemo encapsuladas na SDK
extern void GUI_Memo_AddStr(TGUIControl* memo, const char* str);
extern void GUI_Memo_Clear(TGUIControl* memo);

int my_app_slot = -1;
TGUIEnvironment MyApp;

// Dimensões globais estáveis
const int winWidth = 520;
const int winHeight = 320;

// Ponteiros Globais dos Componentes da Nova SDK RAD
TOSSerial* serial       = NULL;
TGUIControl* btnOpen    = NULL;
TGUIControl* btnWrite   = NULL;
TGUIControl* btnRead    = NULL;
TGUIControl* btnClose   = NULL;
TGUIControl* editCmd    = NULL;
TGUIControl* cbBaud     = NULL;
TGUIControl* memoRx     = NULL;

char string_resposta[32] = "Recebido: [ ]";

/* ============================================================================
 * FUNÇÃO: Flush_Grafico_Janela
 * ============================================================================ */
void Flush_Grafico_Janela(void) {
    // 1. Desenha os componentes internos do aplicativo (RAD)
    gui_draw_form((TForm*)MyApp.MainWindow);
    gui_render_form((TForm*)MyApp.MainWindow);
        
    // 2. CHAMA A FUNÇÃO DO COMPONENTE! Toda aquela bagunça de ponteiros some daqui.
    OS_IPC_FlipBuffers(my_app_slot, winWidth, winHeight);
}

/* ============================================================================
 * FUNÇÃO: Tratar_Fechamento_Software
 * ============================================================================ */
void Tratar_Fechamento_Software(void) {
    if (serial && serial->Active) {
        OS_Serial_Close(serial);
    }

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
 * CALLBACKS NATIVOS RAD - TRATAMENTO DE EVENTOS UNIFICADO
 * ============================================================================ */
void OnCbBaudChange(void* sender) {
    int item_idx = (int)gui_get_prop(cbBaud, PROP_ITEM_INDEX);
    int novo_baud = (item_idx == 0) ? 9600 : 115200;
    
    OS_Serial_SetBaud(serial, novo_baud);
    
    if (novo_baud == 9600) {
        GUI_Memo_AddStr(memoRx, "Velocidade alterada: 9600 bps\n");
    } else {
        GUI_Memo_AddStr(memoRx, "Velocidade alterada: 115200 bps\n");
    }

    if (serial->Active) {
        GUI_Memo_AddStr(memoRx, "[SISTEMA]: Porta reiniciada com novo Baud.\n");
    }
}

void OnBtnOpenClick(void* sender) {
    if (!serial->Active) {
        if (OS_Serial_Open(serial)) {
            GUI_Memo_AddStr(memoRx, "Porta COM1 Inicializada com Sucesso!\n");
            gui_set_prop(btnOpen, PROP_COLOR, 0xAAAAAA);  
            gui_set_prop(btnClose, PROP_COLOR, 0xCCCCCC); 
        } else {
            GUI_Memo_AddStr(memoRx, "Erro critico: Falha ao abrir COM1.\n");
        }
    }
}

void OnBtnWriteClick(void* sender) {
    if (serial->Active) {
        char* comando = (char*)GUI_Edit_GetText((void*)editCmd);
        if (OS_Serial_Write(serial, comando) > 0) {
            GUI_Memo_AddStr(memoRx, "TX -> ");
            GUI_Memo_AddStr(memoRx, comando);
            GUI_Memo_AddStr(memoRx, "\n");
        }
    } else {
        GUI_Memo_AddStr(memoRx, "Erro: Abra a porta antes de transmitir.\n");
    }
}

void OnBtnReadClick(void* sender) {
    if (serial->Active) {
        char caractere_recebido = 0;
        if (OS_Serial_Read(serial, (uint8_t*)&caractere_recebido, 1) > 0) {
            string_resposta[11] = caractere_recebido;
            GUI_Memo_AddStr(memoRx, "RX <- ");
            GUI_Memo_AddStr(memoRx, string_resposta);
            GUI_Memo_AddStr(memoRx, "\n");
        } else {
            GUI_Memo_AddStr(memoRx, "Buffer RX Vazio.\n");
        }
    } else {
        GUI_Memo_AddStr(memoRx, "Erro: Porta inativa.\n");
    }
}

void OnBtnCloseClick(void* sender) {
    if (serial->Active) {
        OS_Serial_Close(serial);
        GUI_Memo_AddStr(memoRx, "Porta COM1 Fechada e Desalocada.\n");
        gui_set_prop(btnOpen, PROP_COLOR, 0xCCCCCC);  
        gui_set_prop(btnClose, PROP_COLOR, 0xAAAAAA); 
    }
}

/* ============================================================================
 * FUNÇÃO PRINCIPAL (MAIN)
 * ============================================================================ */
int main(int argc, char* argv[]) {
    static int ultimo_x = 0;
    static int ultimo_y = 0;
    static int mouse_hold_timer = 0; 

    // Variáveis de monitoramento para renderização inteligente de foco
    static bool primeiro_desenho = true;
    static bool ultimo_estado_foco = false;

    graphics_init_app(winWidth, winHeight);
    wm_init();
    
    my_app_slot = OS_IPC_RegisterApp("UART Lab Runtimer", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "UART Lab Runtimer v0.0.2", winWidth, winHeight);

    // Configura o plano de fundo preto no formulário
    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x000000);
    }

    serial = OS_CreateSerial(1, 9600);

    /* =========================================================================
     * LAYOUT RAD
     * ========================================================================= */
    btnOpen  = GUI_CreateButton(&MyApp, 10, 40,  210, 30, "Testar OPEN Serial", OnBtnOpenClick);
    btnWrite = GUI_CreateButton(&MyApp, 10, 80,  210, 30, "Transmitir UART (Texto)", OnBtnWriteClick);
    btnRead  = GUI_CreateButton(&MyApp, 10, 120, 210, 30, "Capturar RX Arduino", OnBtnReadClick);
    btnClose = GUI_CreateButton(&MyApp, 10, 160, 210, 30, "Fechar Porta Serial", OnBtnCloseClick);

    GUI_CreateLabel(&MyApp, 10, 205, "Texto para Envio:");
    editCmd  = GUI_CreateEdit(&MyApp, 10, 225, 210, 25, "a", NULL);

    GUI_CreateLabel(&MyApp, 10, 270, "Baud Rate:");
    cbBaud   = GUI_CreateComboBox(&MyApp, 95, 266, 125, 25, OnCbBaudChange);
    
    GUI_ComboBox_AddItem(cbBaud, "9600");
    GUI_ComboBox_AddItem(cbBaud, "115200");

    GUI_CreateLabel(&MyApp, 235, 40, "Terminal Receptor (RX):");
    memoRx   = GUI_CreateMemo(&MyApp, 235, 60, 275, 230);
    GUI_Memo_AddStr(memoRx, "Terminal carregado. Pronto para receber dados...\n");

    // Primeiro desenho obrigatório (Cold Start)
    Flush_Grafico_Janela();

    /* =========================================================================
     * LOOP PRINCIPAL DE EVENTOS REVISADO E ULTRA-FLUIDO
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

        // Gatilho do Cold-Start inicial
        if (primeiro_desenho) {
            primeiro_desenho = false;
            precisa_redesenhar = true;
        }

        // Forçar atualização imediata se o estado do foco alternou (ganhou ou perdeu)
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

                    // Define efeito de pressionado com base nas coordenadas
                    if (btnOpen && rel_x >= btnOpen->Left && rel_x < (btnOpen->Left + btnOpen->Width) &&
                        rel_y >= btnOpen->Top && rel_y < (btnOpen->Top + btnOpen->Height)) {
                        gui_set_prop(btnOpen, PROP_STATE, 2);
                    }
                    else if (btnWrite && rel_x >= btnWrite->Left && rel_x < (btnWrite->Left + btnWrite->Width) &&
                             rel_y >= btnWrite->Top && rel_y < (btnWrite->Top + btnWrite->Height)) {
                        gui_set_prop(btnWrite, PROP_STATE, 2);
                    }
                    else if (btnRead && rel_x >= btnRead->Left && rel_x < (btnRead->Left + btnRead->Width) &&
                             rel_y >= btnRead->Top && rel_y < (btnRead->Top + btnRead->Height)) {
                        gui_set_prop(btnRead, PROP_STATE, 2);
                    }
                    else if (btnClose && rel_x >= btnClose->Left && rel_x < (btnClose->Left + btnClose->Width) &&
                             rel_y >= btnClose->Top && rel_y < (btnClose->Top + btnClose->Height)) {
                        gui_set_prop(btnClose, PROP_STATE, 2);
                    }

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
                    if (btnOpen)  gui_set_prop(btnOpen, PROP_STATE, 0); 
                    if (btnWrite) gui_set_prop(btnWrite, PROP_STATE, 0); 
                    if (btnRead)  gui_set_prop(btnRead, PROP_STATE, 0); 
                    if (btnClose) gui_set_prop(btnClose, PROP_STATE, 0); 
                    events_process_mouse(ultimo_x, ultimo_y, 0, 0); 
                    precisa_redesenhar = true;
                }
            }
        }

        // CORREÇÃO: Fora do bloco de foco para atualizar as cores da borda instantaneamente
        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        sys_sleep(euTenhoFoco ? 16 : 32);
    }

    sys_exit(); 
    return 0;
}
