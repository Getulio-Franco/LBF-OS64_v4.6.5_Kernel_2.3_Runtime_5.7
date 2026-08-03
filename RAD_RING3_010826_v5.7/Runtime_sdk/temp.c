#include "sdk/libgui.h"
#include "../system/graphics.h"
#include "../gui/wm.h"
#include "../system/string.h"
#include "../system/liblib.h"

// Componentes do Sistema encapsulados
#include "components/TOS_IPC.h"     
#include "components/TOSSerial.h"   

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
int my_app_slot = -1;
TGUIEnvironment MyApp;

// Configurações de Dimensão
const int winWidth = 550;
const int winHeight = 410;

// Ponteiros de Controle RAD
TGUIControl* ExeButton    = NULL;
TGUIControl* ExeEdit      = NULL;
TGUIControl* ExeComboBox  = NULL;
TGUIControl* ExeCheckBox  = NULL;
TGUIControl* CloseButton  = NULL;
TGUIControl* ExeMemo      = NULL;

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
        
    // 2. CHAMA A FUNÇÃO DO COMPONENTE! Toda aquela bagunça de ponteiros some daqui.
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
void OnBtnPrincipalClick(void* sender) {
    GUI_Memo_AddStr(ExeMemo, "Botao principal clicado!\n");
}

void OnBtnFecharClick(void* sender) {
    Tratar_Fechamento_Software();
}

void OnComboBoxChange(void* sender) {
    GUI_Memo_AddStr(ExeMemo, "ComboBox alterado.\n");
}

void OnCheckBoxClick(void* sender) {
    GUI_Memo_AddStr(ExeMemo, "CheckBox interagido.\n");
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
    
    my_app_slot = OS_IPC_RegisterApp("Template Simplificado", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "Template RAD Simplificado v1.0", winWidth, winHeight);

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x000000);
    }

    /* =========================================================================
     * DESIGN DO LAYOUT DE COMPONENTES
     * ========================================================================= */
    int btnW = 210;
    int ctrlH = 30;

    ExeButton   = GUI_CreateButton(&MyApp, 10, 40, btnW, ctrlH, "Exemplo de TButton", OnBtnPrincipalClick);
    ExeEdit     = GUI_CreateEdit(&MyApp, 10, 80, btnW, ctrlH, "Texto do TEdit", NULL);
    ExeComboBox = GUI_CreateComboBox(&MyApp, 10, 120, btnW, ctrlH, OnComboBoxChange);
    
    if (ExeComboBox) {
        GUI_ComboBox_AddItem(ExeComboBox, "Opcao A");
        GUI_ComboBox_AddItem(ExeComboBox, "Opcao B");
    }

    ExeCheckBox = GUI_CreateCheckBox(&MyApp, 250, 40, "Exemplo de TCheckBox", OnCheckBoxClick);
    CloseButton = GUI_CreateButton(&MyApp, 250, 80, btnW, ctrlH, "Fechar Software", OnBtnFecharClick);
    
    GUI_CreateLabel(&MyApp, 10, 185, "Console de Saida/Digitacao (Focado):");
    ExeMemo = GUI_CreateMemo(&MyApp, 10, 205, 530, 160);
    gui_set_prop(ExeMemo, PROP_COLOR, 0x000000);
    GUI_Memo_AddStr(ExeMemo, "Pronto para receber entrada de dados...\n");

    // Configuração e gravação do Foco Inicial Padrão
    g_focused_control = (void*)ExeEdit;
    ultimo_controle_focado = (void*)ExeEdit;
    gui_set_prop(ExeEdit, PROP_SET_FOCUS, 1);

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
                if (ExeButton && rel_x >= ExeButton->Left && rel_x < (ExeButton->Left + ExeButton->Width) &&
                    rel_y >= ExeButton->Top && rel_y < (ExeButton->Top + ExeButton->Height)) {
                    gui_set_prop(ExeButton, PROP_STATE, 2);
                }
                else if (CloseButton && rel_x >= CloseButton->Left && rel_x < (CloseButton->Left + CloseButton->Width) &&
                         rel_y >= CloseButton->Top && rel_y < (CloseButton->Top + CloseButton->Height)) {
                    gui_set_prop(CloseButton, PROP_STATE, 2);
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
                if (ExeButton)   gui_set_prop(ExeButton, PROP_STATE, 0); 
                if (CloseButton) gui_set_prop(CloseButton, PROP_STATE, 0); 
                events_process_mouse(ultimo_x, ultimo_y, 0, 0); 
                precisa_redesenhar = true;
            }
        }

        // Atualização e disparo gráfico da janela na tela do SO
        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        // Foco ativo = 16ms (~60 FPS), Segundo Plano = 32ms (~30 FPS)
        sys_sleep(euTenhoFocoJanelaReal ? 16 : 32);
    }

    sys_exit(); 
    return 0;
}
