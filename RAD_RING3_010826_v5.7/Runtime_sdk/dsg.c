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
const int winHeight = 350;

// Ponteiros de Controle RAD
TGUIControl* DsgButton   = NULL;   
TGUIControl* ExeMemo     = NULL;

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
char Obter_Tecla_Entrada(void) {
    AppWindowInfoExtended* ext_slot = (AppWindowInfoExtended*)&IPC_WINDOW_LIST[my_app_slot];

    if (ext_slot->tem_evento_teclado == 1) {
        char key = (char)ext_slot->fila_teclado_virtual;
        ext_slot->tem_evento_teclado = 0; 
        return key;
    }
    return 0;
}

void Flush_Grafico_Janela(void) {
    gui_draw_form((TForm*)MyApp.MainWindow);
    gui_render_form((TForm*)MyApp.MainWindow);
    OS_IPC_FlipBuffers(my_app_slot, winWidth, winHeight);
}

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
 * TESTE DE ESTRESSE DE PILHA (DSG - DYNAMIC STACK GROWTH)
 * ============================================================================ */
void Testar_Expansao_Pilha_DSG(TGUIControl* memo) {
    if (!memo) return;

    GUI_Memo_AddStr(memo, "[DSG Test] Iniciando teste de expansao de pilha (128 KB)...\n");

    // 1. Aloca 128 KB DIRETAMENTE NA PILHA (Variável local)
    // Isso faz o registrador RSP decrementar 128 KB de uma só vez!
    #define TEST_SIZE (128 * 1024)
    volatile char buffer_grande_na_pilha[TEST_SIZE];

    // 2. Toca a memória de 4KB em 4KB para acionar o Page Fault página a página
    int paginas_tocadas = 0;
    for (int i = 0; i < TEST_SIZE; i += 4096) {
        buffer_grande_na_pilha[i] = (char)(i & 0xFF); // Força gravação na página
        paginas_tocadas++;
    }

    // Grava também na última posição do buffer para certificar o limite de 128KB
    buffer_grande_na_pilha[TEST_SIZE - 1] = 0xAA;

    // 3. Se chegou aqui sem dar Crash/Segfault, o DSG funcionou perfeitamente!
    GUI_Memo_AddStr(memo, "[DSG Test] SUCESSO! 128 KB (32 paginas) expandidos via #PF!\n");
}

/* ============================================================================
 * CALLBACKS DE EVENTOS RAD
 * ============================================================================ */
void OnBtnDsgClick(void* sender) {
    // Executa a rotina de estresse da pilha
    Testar_Expansao_Pilha_DSG(ExeMemo);
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

    graphics_init_app(winWidth, winHeight);
    wm_init();
    
    my_app_slot = OS_IPC_RegisterApp("App Teste DSG", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "LBF-OS - Teste de Memoria Dinamica (DSG)", winWidth, winHeight);

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x000000);
    }

    // --- DESIGN DO LAYOUT ---
    int btnW = 240;
    int ctrlH = 30;

    DsgButton = GUI_CreateButton(&MyApp, 10, 40, btnW, ctrlH, "Testar Expansao de Pilha (DSG)", OnBtnDsgClick);
    
    GUI_CreateLabel(&MyApp, 10, 90, "Console de Saida do Teste de Memoria:");
    ExeMemo = GUI_CreateMemo(&MyApp, 10, 110, 530, 220);
    gui_set_prop(ExeMemo, PROP_COLOR, 0x000000);
    GUI_Memo_AddStr(ExeMemo, "Pronto. Clique no botao para testar o DSG...\n");

    Flush_Grafico_Janela();

    // --- LOOP DE EVENTOS CONTINUO ---
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

        bool euTenhoFocoJanelaReal = (IPC_CONTROL->active_focus_slot == my_app_slot);
        if (euTenhoFocoJanelaReal != ultimo_estado_foco) {
            ultimo_estado_foco = euTenhoFocoJanelaReal;
            if (MyApp.MainWindow) {
                ((TForm*)MyApp.MainWindow)->ActiveFocus = euTenhoFocoJanelaReal; 
            }
            precisa_redesenhar = true;
        }

        // Processamento de Teclado
        char key = Obter_Tecla_Entrada();
        if (key != 0) {
            GUI_ProcessKeyboard(&MyApp, key); 
            precisa_redesenhar = true; 
        }

        // Processamento de Mouse
        if (IPC_WINDOW_LIST[my_app_slot].has_click_event == 1) {
            if (mouse_hold_timer == 0) {
                int rel_x = IPC_WINDOW_LIST[my_app_slot].local_click_x;
                int rel_y = IPC_WINDOW_LIST[my_app_slot].local_click_y;
                ultimo_x = rel_x;
                ultimo_y = rel_y;
                mouse_hold_timer = 2; 

                // Feedback visual para o botão DSG
                if (DsgButton && rel_x >= DsgButton->Left && rel_x < (DsgButton->Left + DsgButton->Width) &&
                    rel_y >= DsgButton->Top && rel_y < (DsgButton->Top + DsgButton->Height)) {
                    gui_set_prop(DsgButton, PROP_STATE, 2);
                }

                events_process_mouse(rel_x, rel_y, 1, 0);
                
                if (GUI_ProcessMouseClick(&MyApp, rel_x, rel_y)) {
                    precisa_redesenhar = true;
                }
            }
            IPC_WINDOW_LIST[my_app_slot].has_click_event = 0;
        }

        if (mouse_hold_timer > 0) {
            mouse_hold_timer--; 
            if (mouse_hold_timer == 0) {
                if (DsgButton) gui_set_prop(DsgButton, PROP_STATE, 0); 
                events_process_mouse(ultimo_x, ultimo_y, 0, 0); 
                precisa_redesenhar = true;
            }
        }

        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        sys_sleep(euTenhoFocoJanelaReal ? 16 : 32);
    }

    sys_exit(); 
    return 0;
}
