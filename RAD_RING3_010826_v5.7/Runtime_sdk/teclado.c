#include "sdk/libgui.h"
#include "../system/graphics.h"
#include "../gui/wm.h"
#include "../system/string.h"
#include "../system/liblib.h"

// Componentes do Sistema encapsulados
#include "components/TOS_IPC.h"     
#include "components/TOSSerial.h"   

// Protótipos obrigatórios de renderização gráfica do subsistema
void gui_draw_form(TForm* form);
void gui_render_form(TForm* form);
void Tratar_Fechamento_Software(void);

// Inclusão de funções de controle mapeadas diretamente no subsistema do Kernel
extern void events_process_mouse(int x, int y, int pressed, int button);
extern void* g_focused_control;

// Protótipos das funções do TMemo encapsuladas na SDK
extern void GUI_Memo_AddStr(TGUIControl* memo, const char* str);
extern void GUI_Memo_Clear(TGUIControl* memo);

// Variáveis de controle do ambiente da aplicação
int my_app_slot = -1;
TGUIEnvironment MyApp;

// Configurações Globais de Dimensão baseadas no layout do teclado
const int winWidth = 560;
const int winHeight = 420;

// Ponteiros Globais para Referência de Objetos
TGUIControl* ExeMemo = NULL;

// Buffer de texto interno para gerenciar strings dinâmicas no Memo local
char texto_buffer[2048] = "";

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
 * FUNÇÃO: EnviarMensagemTeclado
 * ============================================================================ */
void EnviarMensagemTeclado(char ascii) {
    // 1. Escreve no buffer local da janela para teste visual
    char str_aux[2] = {ascii, '\0'};
    if (strlen(texto_buffer) + 1 < sizeof(texto_buffer) - 1) {
        strcat(texto_buffer, str_aux);
        GUI_Memo_Clear(ExeMemo);
        GUI_Memo_AddStr(ExeMemo, texto_buffer);
    }

    // 2. Envio global via IPC para o slot em foco
    int destino_slot = IPC_CONTROL->active_focus_slot;
    if (destino_slot != -1 && destino_slot != my_app_slot) {
        
        typedef struct {
            // Recriação local temporária da estrutura expandida para burlar a limitação do header da SDK
            uint8_t dummy[sizeof(IPC_WINDOW_LIST[0])]; 
            volatile uint8_t fila_teclado_virtual;
            volatile uint8_t tem_evento_teclado;
        } __attribute__((packed)) AppWindowInfoExtended;

        AppWindowInfoExtended* ext_slot = (AppWindowInfoExtended*)&IPC_WINDOW_LIST[destino_slot];
        
        ext_slot->fila_teclado_virtual = (uint8_t)ascii;
        ext_slot->tem_evento_teclado = 1;
    }
}

/* ============================================================================
 * LÓGICA DE MANIPULAÇÃO LOCAL DE BOTÕES ESPECIAIS
 * ============================================================================ */
void ExecutarBackspace() {
    int len = strlen(texto_buffer);
    if (len > 0) {
        texto_buffer[len - 1] = '\0';
        GUI_Memo_Clear(ExeMemo);
        GUI_Memo_AddStr(ExeMemo, texto_buffer);
    }
}

void ExecutarClear() {
    texto_buffer[0] = '\0';
    GUI_Memo_Clear(ExeMemo);
}

/* ============================================================================
 * CALLBACKS EXPLICITOS PARA EVITAR O ERRO DE COMPILAÇÃO (.Caption)
 * ============================================================================ */
void OnKey1(void* s) { EnviarMensagemTeclado('1'); }
void OnKey2(void* s) { EnviarMensagemTeclado('2'); }
void OnKey3(void* s) { EnviarMensagemTeclado('3'); }
void OnKey4(void* s) { EnviarMensagemTeclado('4'); }
void OnKey5(void* s) { EnviarMensagemTeclado('5'); }
void OnKey6(void* s) { EnviarMensagemTeclado('6'); }
void OnKey7(void* s) { EnviarMensagemTeclado('7'); }
void OnKey8(void* s) { EnviarMensagemTeclado('8'); }
void OnKey9(void* s) { EnviarMensagemTeclado('9'); }
void OnKey0(void* s) { EnviarMensagemTeclado('0'); }
void OnKeyHifen(void* s) { EnviarMensagemTeclado('-'); }
void OnKeyIgual(void* s) { EnviarMensagemTeclado('='); }

void OnKeyQ(void* s) { EnviarMensagemTeclado('Q'); }
void OnKeyW(void* s) { EnviarMensagemTeclado('W'); }
void OnKeyE(void* s) { EnviarMensagemTeclado('E'); }
void OnKeyR(void* s) { EnviarMensagemTeclado('R'); }
void OnKeyT(void* s) { EnviarMensagemTeclado('T'); }
void OnKeyY(void* s) { EnviarMensagemTeclado('Y'); }
void OnKeyU(void* s) { EnviarMensagemTeclado('U'); }
void OnKeyI(void* s) { EnviarMensagemTeclado('I'); }
void OnKeyO(void* s) { EnviarMensagemTeclado('O'); }
void OnKeyP(void* s) { EnviarMensagemTeclado('P'); }
void OnKeyColE(void* s) { EnviarMensagemTeclado('['); }
void OnKeyColD(void* s) { EnviarMensagemTeclado(']'); }
void OnKeyBarra(void* s) { EnviarMensagemTeclado('\\'); }

void OnKeyA(void* s) { EnviarMensagemTeclado('A'); }
void OnKeyS(void* s) { EnviarMensagemTeclado('S'); }
void OnKeyD(void* s) { EnviarMensagemTeclado('D'); }
void OnKeyF(void* s) { EnviarMensagemTeclado('F'); }
void OnKeyG(void* s) { EnviarMensagemTeclado('G'); }
void OnKeyH(void* s) { EnviarMensagemTeclado('H'); }
void OnKeyJ(void* s) { EnviarMensagemTeclado('J'); }
void OnKeyK(void* s) { EnviarMensagemTeclado('K'); }
void OnKeyL(void* s) { EnviarMensagemTeclado('L'); }
void OnKeyPVirgula(void* s) { EnviarMensagemTeclado(';'); }
void OnKeyAspas(void* s) { EnviarMensagemTeclado('\''); }

void OnKeyZ(void* s) { EnviarMensagemTeclado('Z'); }
void OnKeyX(void* s) { EnviarMensagemTeclado('X'); }
void OnKeyC(void* s) { EnviarMensagemTeclado('C'); }
void OnKeyV(void* s) { EnviarMensagemTeclado('V'); }
void OnKeyB(void* s) { EnviarMensagemTeclado('B'); }
void OnKeyN(void* s) { EnviarMensagemTeclado('N'); }
void OnKeyM(void* s) { EnviarMensagemTeclado('M'); }
void OnKeyVirgula(void* s) { EnviarMensagemTeclado(','); }
void OnKeyPonto(void* s) { EnviarMensagemTeclado('.'); }
void OnKeyBarraF(void* s) { EnviarMensagemTeclado('/'); }

void OnSpaceClick(void* sender)     { EnviarMensagemTeclado(' '); }
void OnBackspaceClick(void* sender) { ExecutarBackspace(); }
void OnClearClick(void* sender)     { ExecutarClear(); }
void OnEnterClick(void* sender)     { EnviarMensagemTeclado('\n'); } // NOVO: Evento de Enter

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
    
    my_app_slot = OS_IPC_RegisterApp("Teclado Virtual LBF", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 

    // NOVO: Flag para forçar o S.O. a manter essa janela sempre por cima
   // IPC_WINDOW_LIST[my_app_slot].always_on_top = 1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "Teclado Virtual OS v1.1", winWidth, winHeight);

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x1A1A1A); 
    }

    // --- PARTE SUPERIOR: VISOR DE TEXTO (MEMO) ---
    GUI_CreateLabel(&MyApp, 15, 35, "Terminal de Digitacao:");
    ExeMemo = GUI_CreateMemo(&MyApp, 15, 50, 530, 100);

    /* =========================================================================
     * INICIALIZAÇÃO DA MATRIZ QWERTY
     * ========================================================================= */
    int k_width  = 34;
    int k_height = 35;
    int spacing  = 5;
    
    int start_x  = 15;
    int start_y  = 165;

    // --- LINHA 1 ---
    GUI_CreateButton(&MyApp, start_x + (0 * (k_width + spacing)), start_y, k_width, k_height, "1", OnKey1);
    GUI_CreateButton(&MyApp, start_x + (1 * (k_width + spacing)), start_y, k_width, k_height, "2", OnKey2);
    GUI_CreateButton(&MyApp, start_x + (2 * (k_width + spacing)), start_y, k_width, k_height, "3", OnKey3);
    GUI_CreateButton(&MyApp, start_x + (3 * (k_width + spacing)), start_y, k_width, k_height, "4", OnKey4);
    GUI_CreateButton(&MyApp, start_x + (4 * (k_width + spacing)), start_y, k_width, k_height, "5", OnKey5);
    GUI_CreateButton(&MyApp, start_x + (5 * (k_width + spacing)), start_y, k_width, k_height, "6", OnKey6);
    GUI_CreateButton(&MyApp, start_x + (6 * (k_width + spacing)), start_y, k_width, k_height, "7", OnKey7);
    GUI_CreateButton(&MyApp, start_x + (7 * (k_width + spacing)), start_y, k_width, k_height, "8", OnKey8);
    GUI_CreateButton(&MyApp, start_x + (8 * (k_width + spacing)), start_y, k_width, k_height, "9", OnKey9);
    GUI_CreateButton(&MyApp, start_x + (9 * (k_width + spacing)), start_y, k_width, k_height, "0", OnKey0);
    GUI_CreateButton(&MyApp, start_x + (10 * (k_width + spacing)), start_y, k_width, k_height, "-", OnKeyHifen);
    GUI_CreateButton(&MyApp, start_x + (11 * (k_width + spacing)), start_y, k_width, k_height, "=", OnKeyIgual);
    GUI_CreateButton(&MyApp, start_x + (12 * (k_width + spacing)), start_y, 73, k_height, "Back", OnBackspaceClick);

    // --- LINHA 2 ---
    start_y += k_height + spacing;
    GUI_CreateButton(&MyApp, start_x + (0 * (k_width + spacing)), start_y, k_width, k_height, "Q", OnKeyQ);
    GUI_CreateButton(&MyApp, start_x + (1 * (k_width + spacing)), start_y, k_width, k_height, "W", OnKeyW);
    GUI_CreateButton(&MyApp, start_x + (2 * (k_width + spacing)), start_y, k_width, k_height, "E", OnKeyE);
    GUI_CreateButton(&MyApp, start_x + (3 * (k_width + spacing)), start_y, k_width, k_height, "R", OnKeyR);
    GUI_CreateButton(&MyApp, start_x + (4 * (k_width + spacing)), start_y, k_width, k_height, "T", OnKeyT);
    GUI_CreateButton(&MyApp, start_x + (5 * (k_width + spacing)), start_y, k_width, k_height, "Y", OnKeyY);
    GUI_CreateButton(&MyApp, start_x + (6 * (k_width + spacing)), start_y, k_width, k_height, "U", OnKeyU);
    GUI_CreateButton(&MyApp, start_x + (7 * (k_width + spacing)), start_y, k_width, k_height, "I", OnKeyI);
    GUI_CreateButton(&MyApp, start_x + (8 * (k_width + spacing)), start_y, k_width, k_height, "O", OnKeyO);
    GUI_CreateButton(&MyApp, start_x + (9 * (k_width + spacing)), start_y, k_width, k_height, "P", OnKeyP);
    GUI_CreateButton(&MyApp, start_x + (10 * (k_width + spacing)), start_y, k_width, k_height, "[", OnKeyColE);
    GUI_CreateButton(&MyApp, start_x + (11 * (k_width + spacing)), start_y, k_width, k_height, "]", OnKeyColD);
    GUI_CreateButton(&MyApp, start_x + (12 * (k_width + spacing)), start_y, k_width, k_height, "\\", OnKeyBarra);

    // --- LINHA 3 ---
    start_y += k_height + spacing;
    int offset_row3 = 15;
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (0 * (k_width + spacing)), start_y, k_width, k_height, "A", OnKeyA);
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (1 * (k_width + spacing)), start_y, k_width, k_height, "S", OnKeyS);
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (2 * (k_width + spacing)), start_y, k_width, k_height, "D", OnKeyD);
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (3 * (k_width + spacing)), start_y, k_width, k_height, "F", OnKeyF);
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (4 * (k_width + spacing)), start_y, k_width, k_height, "G", OnKeyG);
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (5 * (k_width + spacing)), start_y, k_width, k_height, "H", OnKeyH);
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (6 * (k_width + spacing)), start_y, k_width, k_height, "J", OnKeyJ);
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (7 * (k_width + spacing)), start_y, k_width, k_height, "K", OnKeyK);
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (8 * (k_width + spacing)), start_y, k_width, k_height, "L", OnKeyL);
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (9 * (k_width + spacing)), start_y, k_width, k_height, ";", OnKeyPVirgula);
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (10 * (k_width + spacing)), start_y, k_width, k_height, "'", OnKeyAspas);
    GUI_CreateButton(&MyApp, start_x + offset_row3 + (11 * (k_width + spacing)), start_y, 82, k_height, "Clear", OnClearClick);

    // --- LINHA 4 ---
    start_y += k_height + spacing;
    int offset_row4 = 30;
    GUI_CreateButton(&MyApp, start_x + offset_row4 + (0 * (k_width + spacing)), start_y, k_width, k_height, "Z", OnKeyZ);
    GUI_CreateButton(&MyApp, start_x + offset_row4 + (1 * (k_width + spacing)), start_y, k_width, k_height, "X", OnKeyX);
    GUI_CreateButton(&MyApp, start_x + offset_row4 + (2 * (k_width + spacing)), start_y, k_width, k_height, "C", OnKeyC);
    GUI_CreateButton(&MyApp, start_x + offset_row4 + (3 * (k_width + spacing)), start_y, k_width, k_height, "V", OnKeyV);
    GUI_CreateButton(&MyApp, start_x + offset_row4 + (4 * (k_width + spacing)), start_y, k_width, k_height, "B", OnKeyB);
    GUI_CreateButton(&MyApp, start_x + offset_row4 + (5 * (k_width + spacing)), start_y, k_width, k_height, "N", OnKeyN);
    GUI_CreateButton(&MyApp, start_x + offset_row4 + (6 * (k_width + spacing)), start_y, k_width, k_height, "M", OnKeyM);
    GUI_CreateButton(&MyApp, start_x + offset_row4 + (7 * (k_width + spacing)), start_y, k_width, k_height, ",", OnKeyVirgula);
    GUI_CreateButton(&MyApp, start_x + offset_row4 + (8 * (k_width + spacing)), start_y, k_width, k_height, ".", OnKeyPonto);
    GUI_CreateButton(&MyApp, start_x + offset_row4 + (9 * (k_width + spacing)), start_y, k_width, k_height, "/", OnKeyBarraF);
    
    // NOVO: Adicionado o botão de Enter no final da linha 4
    GUI_CreateButton(&MyApp, start_x + offset_row4 + (10 * (k_width + spacing)), start_y, 82, k_height, "Enter", OnEnterClick);

    // --- LINHA 5 ---
    start_y += k_height + spacing;
    GUI_CreateButton(&MyApp, start_x + 100, start_y, 350, k_height, "Espaco", OnSpaceClick);

    Flush_Grafico_Janela();

    while(1) {
        if (IPC_WINDOW_LIST[my_app_slot].is_active == 0) {
            Tratar_Fechamento_Software();
            break;
        }

        // NOVO: Verifica se a janela real recebeu foco (útil para o Sleep Dinâmico)
        bool euTenhoFocoJanelaReal = (IPC_CONTROL->active_focus_slot == my_app_slot);

        // O teclado virtual nunca deve reter foco VISUAL no loop principal para não roubar o cursor do TEdit destino
        bool focoGrafico = false; 
        if (MyApp.MainWindow) {
            ((TForm*)MyApp.MainWindow)->ActiveFocus = focoGrafico;
        }

        bool precisa_redesenhar = false;

        if (primeiro_desenho) {
            primeiro_desenho = false;
            precisa_redesenhar = true;
        }

        if (euTenhoFocoJanelaReal != ultimo_estado_foco) {
            ultimo_estado_foco = euTenhoFocoJanelaReal;
            precisa_redesenhar = true;
        }

        // PROCESSAMENTO DE CLIQUE DIRECTO: 
        // Monitoramos cliques mesmo quando a janela do teclado NÃO detém o foco ativo
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

        if (mouse_hold_timer > 0) {
            mouse_hold_timer--; 
            if (mouse_hold_timer == 0) {
                events_process_mouse(ultimo_x, ultimo_y, 0, 0); 
                precisa_redesenhar = true;
            }
        }

        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        // NOVO: Foco ativo = 16ms (~60 FPS), Segundo Plano = 32ms (~30 FPS)
        sys_sleep(euTenhoFocoJanelaReal ? 16 : 32); 
    }

    sys_exit(); 
    return 0;
}
