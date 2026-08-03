#include "sdk/libgui.h"
#include "../system/graphics.h"
#include "../gui/wm.h"
#include "../system/string.h"
#include "../system/liblib.h"

// Componentes do Sistema encapsulados
#include "components/TOS_IPC.h"     
#include "components/TOSSerial.h"   

// Protótipos obrigatórios de renderização gráfica do subsistema
extern void gui_draw_form(TForm* form);
extern void gui_render_form(TForm* form);

// Inclusão de funções de controle mapeadas diretamente no subsistema do Kernel
extern void events_process_mouse(int x, int y, int pressed, int button);
extern void* g_focused_control;

// Variáveis de controle do ambiente da aplicação
int my_app_slot = -1;
TGUIEnvironment MyApp;

// Janela reduzida para ser apenas um prompt de execução
const int winWidth = 350;
const int winHeight = 130; 

// Apenas os dois componentes solicitados
TGUIControl* ExecEdit    = NULL; 
TGUIControl* BtnExecutar = NULL; 

/* ============================================================================
 * ESTRUTURA AUXILIAR IPC
 * ============================================================================ */
typedef struct {
    uint8_t dummy[sizeof(IPC_WINDOW_LIST[0])]; 
    volatile uint8_t fila_teclado_virtual;
    volatile uint8_t tem_evento_teclado;
} __attribute__((packed)) AppWindowInfoExtended;

/* ============================================================================
 * FUNÇÃO: Obter_Tecla_Entrada (Híbrida: Virtual + Física)
 * ============================================================================ */
char Obter_Tecla_Entrada(void) {
    AppWindowInfoExtended* ext_slot = (AppWindowInfoExtended*)&IPC_WINDOW_LIST[my_app_slot];
    if (ext_slot->tem_evento_teclado == 1) {
        char key = (char)ext_slot->fila_teclado_virtual;
        ext_slot->tem_evento_teclado = 0; 
        return key;
    }
    return get_key();
}

/* ============================================================================
 * FUNÇÃO: Flush_Grafico_Janela
 * ============================================================================ */
void Flush_Grafico_Janela(void) {
    gui_draw_form((TForm*)MyApp.MainWindow);
    gui_render_form((TForm*)MyApp.MainWindow);
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
 * EVENTOS CALLBACKS
 * ============================================================================ */
void Executar_Binario_Seguro(void) {
    char* caminho_elf = GUI_Edit_GetText(ExecEdit);
    if (!caminho_elf || caminho_elf[0] == '\0') return;

    // Dispara a execução assíncrona para a task_d no Ring 0
    sys_exec(caminho_elf);

    // Limpa o campo de texto para o próximo uso (ou você pode optar por fechar o app aqui)
    GUI_Edit_SetText(ExecEdit, "");
}

void OnBtnExecutarClick(void* sender) {
    Executar_Binario_Seguro();
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

    graphics_init_app(winWidth, winHeight);
    wm_init();
    
    my_app_slot = OS_IPC_RegisterApp("Executar", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "Executar App", winWidth, winHeight);

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0xC0C0C0); // Fundo preto
    }

    /* =========================================================================
     * DESIGN LAYOUT RAD ENCAPSULADO (Super Simples)
     * ========================================================================= */
    GUI_CreateLabel(&MyApp, 15, 38, "Digite o caminho do arquivo .elf:");
    
    // Edit centralizado para o caminho
    ExecEdit = GUI_CreateEdit(&MyApp, 15, 60, 320, 25, "", NULL); 
    
    // Botão de execução logo abaixo
    BtnExecutar = GUI_CreateButton(&MyApp, 235, 95, 100, 25, "Abrir", OnBtnExecutarClick);

    // Foco inicial no campo de texto
    g_focused_control = (void*)ExecEdit;
    ultimo_controle_focado = (void*)ExecEdit;
    gui_set_prop(ExecEdit, PROP_SET_FOCUS, 1);

    // Primeiro desenho obrigatório (Sem syscalls de diretório!)
    Flush_Grafico_Janela();

    /* =========================================================================
     * LOOP PRINCIPAL DE EVENTOS
     * ========================================================================= */
    while(1) {
        if (IPC_WINDOW_LIST[my_app_slot].is_active == 0) {
            Tratar_Fechamento_Software();
            break;
        }

        bool precisa_redesenhar = false;

        if (primeiro_desenho) {
            primeiro_desenho = false;
            precisa_redesenhar = true;
        }

        bool euTenhoFoco = (IPC_CONTROL->active_focus_slot == my_app_slot);
        if (euTenhoFoco != ultimo_estado_foco) {
            ultimo_estado_foco = euTenhoFoco;
            if (MyApp.MainWindow) {
                ((TForm*)MyApp.MainWindow)->ActiveFocus = euTenhoFoco;
            }
            precisa_redesenhar = true;
        }

        if (g_focused_control != NULL) {
            ultimo_controle_focado = g_focused_control;
        } else if (ultimo_controle_focado != NULL) {
            g_focused_control = ultimo_controle_focado;
            gui_set_prop((TGUIControl*)ultimo_controle_focado, PROP_SET_FOCUS, 1);
        }

        // --- Entrada de Teclado ---
        if (euTenhoFoco) {
            char key = Obter_Tecla_Entrada();
            if (key != 0) {
                // Se apertar ENTER (código 13 ou '\n'), também executa
                if (key == '\n' || key == 13) {
                    Executar_Binario_Seguro();
                } else {
                    GUI_ProcessKeyboard(&MyApp, key); 
                }
                precisa_redesenhar = true;
            }
        }

        // --- Entrada de Mouse ---
        if (IPC_WINDOW_LIST[my_app_slot].has_click_event == 1) {
            if (mouse_hold_timer == 0) {
                int rel_x = IPC_WINDOW_LIST[my_app_slot].local_click_x;
                int rel_y = IPC_WINDOW_LIST[my_app_slot].local_click_y;
                ultimo_x = rel_x; 
                ultimo_y = rel_y; 
                mouse_hold_timer = 2; 

                // Verifica o clique no único botão da interface
                if (BtnExecutar && rel_x >= BtnExecutar->Left && rel_x < (BtnExecutar->Left + BtnExecutar->Width) && rel_y >= BtnExecutar->Top && rel_y < (BtnExecutar->Top + BtnExecutar->Height)) {
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

        // --- Release do Mouse ---
        if (mouse_hold_timer > 0) {
            mouse_hold_timer--; 
            if (mouse_hold_timer == 0) {
                if (BtnExecutar) gui_set_prop(BtnExecutar, PROP_STATE, 0); 
                events_process_mouse(ultimo_x, ultimo_y, 0, 0); 
                precisa_redesenhar = true;
            }
        }

        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        sys_sleep(euTenhoFoco ? 16 : 32);
    }

    sys_exit(); 
    return 0;
}
