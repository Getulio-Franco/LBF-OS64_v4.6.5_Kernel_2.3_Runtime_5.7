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

// Inclusão de funções de controle mapeadas diretamente no subsistema do Kernel
extern void events_process_mouse(int x, int y, int pressed, int button);
extern void* g_focused_control;

// Protótipos para manipulação do TEdit (Visor da Calculadora)
extern char* GUI_Edit_GetText(TGUIControl* edit);
extern void GUI_Edit_SetText(TGUIControl* edit, const char* text); 

// Variáveis de controle do ambiente da aplicação
int my_app_slot = -1;
TGUIEnvironment MyApp;

// Configurações da Janela da Calculadora
const int winWidth = 260;
const int winHeight = 350;

// Ponteiros dos Controles da Calculadora
TGUIControl* CalcDisplay = NULL;
TGUIControl* Btn[10]; 
TGUIControl* BtnAdd, *BtnSub, *BtnMul, *BtnDiv, *BtnEq, *BtnClear;
TGUIControl* BackButton = NULL; 

// Constantes do Motor de Ponto Fixo (4 casas decimais)
#define FIXED_SCALE 10000

// Variáveis de Lógica Matemática em Ponto Fixo
char display_buffer[64] = "0";
int operand1 = 0;              // Armazenado na escala FIXED_SCALE
char current_operator = 0;
bool is_new_number = true;

/* ============================================================================
 * ESTRUTURA AUXILIAR IPC (Mapeamento de Eventos Estendidos)
 * ============================================================================ */
typedef struct {
    uint8_t dummy[sizeof(IPC_WINDOW_LIST[0])]; 
    volatile uint8_t fila_teclado_virtual;
    volatile uint8_t tem_evento_teclado;
} __attribute__((packed)) AppWindowInfoExtended;

/* ============================================================================
 * CONVERSORES DE PONTO FIXO
 * ============================================================================ */
int string_to_ponto_fixo(const char* str) {
    int parte_inteira = 0;
    int parte_decimal = 0;
    int divisor_decimal = 1;
    bool sinal = false;
    bool na_decimal = false;
    int i = 0;

    if (str[0] == '-') {
        sinal = true;
        i++;
    }

    for (; str[i] != '\0'; i++) {
        if (str[i] == ',' || str[i] == '.') {
            na_decimal = true;
            continue;
        }
        if (!na_decimal) {
            parte_inteira = (parte_inteira * 10) + (str[i] - '0');
        } else {
            if (divisor_decimal < FIXED_SCALE) {
                parte_decimal = (parte_decimal * 10) + (str[i] - '0');
                divisor_decimal *= 10;
            }
        }
    }

    while (divisor_decimal < FIXED_SCALE) {
        parte_decimal *= 10;
        divisor_decimal *= 10;
    }

    int resultado = (parte_inteira * FIXED_SCALE) + parte_decimal;
    return sinal ? -resultado : resultado;
}

void ponto_fixo_to_string(int valor, char* buffer) {
    int sinal = 0;
    if (valor < 0) {
        sinal = 1;
        valor = -valor;
    }

    int parte_inteira = valor / FIXED_SCALE;
    int parte_decimal = valor % FIXED_SCALE;

    char temp_inteira[32];
    int_to_string(parte_inteira, temp_inteira);

    if (sinal) {
        strcpy(buffer, "-");
        strcat(buffer, temp_inteira);
    } else {
        strcpy(buffer, temp_inteira);
    }

    if (parte_decimal > 0) {
        strcat(buffer, ",");
        char temp_dec[8];
        int_to_string(parte_decimal, temp_dec);

        int casas_detectadas = 0;
        int dec_copy = parte_decimal;
        if (dec_copy == 0) casas_detectadas = 1;
        while (dec_copy > 0) { casas_detectadas++; dec_copy /= 10; }
        
        int preencher = 4 - casas_detectadas;
        if(parte_decimal < 10) preencher = 3;
        else if(parte_decimal < 100) preencher = 2;
        else if(parte_decimal < 1000) preencher = 1;
        else preencher = 0;

        for (int z = 0; z < preencher; z++) {
            strcat(buffer, "0");
        }
        strcat(buffer, temp_dec);

        int len = strlen(buffer);
        while (len > 0 && buffer[len - 1] == '0') {
            buffer[len - 1] = '\0';
            len--;
        }
        if (len > 0 && buffer[len - 1] == ',') {
            buffer[len - 1] = '\0';
        }
    }
}

/* ============================================================================
 * INTERFACE GRÁFICA & OPERAÇÃO DE BUFFER DE TELA
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

/* ============================================================================
 * FUNÇÃO: Flush_Grafico_Janela
 * ============================================================================ */
void Flush_Grafico_Janela(void) {
    // 1. Desenha os componentes internos do aplicativo (RAD)
    gui_draw_form((TForm*)MyApp.MainWindow);
    gui_render_form((TForm*)MyApp.MainWindow);
        
    // 2. CHAMA A FUNÇÃO DO COMPONENTE!
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

void AtualizarVisor() {
    GUI_Edit_SetText(CalcDisplay, display_buffer);
}

void ProcessarDigito(int digito) {
    if (is_new_number) {
        display_buffer[0] = digito + '0';
        display_buffer[1] = '\0';
        is_new_number = false;
    } else {
        int len = strlen(display_buffer);
        if (len < 15) { 
            display_buffer[len] = digito + '0';
            display_buffer[len + 1] = '\0';
        }
    }
    AtualizarVisor();
}

void ProcessarOperador(char op) {
    operand1 = string_to_ponto_fixo(display_buffer);
    current_operator = op;
    is_new_number = true;
}

void CalcularResultado() {
    if (current_operator == 0) return;
    
    int operand2 = string_to_ponto_fixo(display_buffer);
    int resultado = 0;

    switch (current_operator) {
        case '+': 
            resultado = operand1 + operand2; 
            ponto_fixo_to_string(resultado, display_buffer);
            break;
        case '-': 
            resultado = operand1 - operand2; 
            ponto_fixo_to_string(resultado, display_buffer);
            break;
        case '*': 
            resultado = (int)(((long long)operand1 * operand2) / FIXED_SCALE); 
            ponto_fixo_to_string(resultado, display_buffer);
            break;
        case '/': 
            if (operand2 != 0) {
                resultado = (int)(((long long)operand1 * FIXED_SCALE) / operand2);
                ponto_fixo_to_string(resultado, display_buffer);
            } else {
                strcpy(display_buffer, "Erro");
            }
            break;
    }
    
    AtualizarVisor();
    is_new_number = true;
    current_operator = 0;
}

void LimparCalculadora() {
    strcpy(display_buffer, "0");
    operand1 = 0;
    current_operator = 0;
    is_new_number = true;
    AtualizarVisor();
}

void ProcessarBackspace() {
    int len = strlen(display_buffer);
    if (len > 0 && !is_new_number) {
        display_buffer[len - 1] = '\0';
        if (strlen(display_buffer) == 0 || (strlen(display_buffer) == 1 && display_buffer[0] == '-')) {
            strcpy(display_buffer, "0");
            is_new_number = true;
        }
        AtualizarVisor();
    }
}

/* ============================================================================
 * EVENTOS (CALLBACKS) DOS BOTÕES
 * ============================================================================ */
void OnBtn0Click(void* s) { ProcessarDigito(0); }
void OnBtn1Click(void* s) { ProcessarDigito(1); }
void OnBtn2Click(void* s) { ProcessarDigito(2); }
void OnBtn3Click(void* s) { ProcessarDigito(3); }
void OnBtn4Click(void* s) { ProcessarDigito(4); }
void OnBtn5Click(void* s) { ProcessarDigito(5); }
void OnBtn6Click(void* s) { ProcessarDigito(6); }
void OnBtn7Click(void* s) { ProcessarDigito(7); }
void OnBtn8Click(void* s) { ProcessarDigito(8); }
void OnBtn9Click(void* s) { ProcessarDigito(9); }

void OnBtnAddClick(void* s) { ProcessarOperador('+'); }
void OnBtnSubClick(void* s) { ProcessarOperador('-'); }
void OnBtnMulClick(void* s) { ProcessarOperador('*'); }
void OnBtnDivClick(void* s) { ProcessarOperador('/'); }

void OnBtnEqClick(void* s)  { CalcularResultado(); }
void OnBtnClearClick(void* s){ LimparCalculadora(); }
void OnBtnBackClick(void* s) { ProcessarBackspace(); } 

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
    
    my_app_slot = OS_IPC_RegisterApp("Calculadora LBF", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "Calculadora", winWidth, winHeight);

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x1E1E1E); 
    }

    /* Layout da Matriz de Botões */
    int btnW = 50;  
    int btnH = 40;
    int sp = 8; 
    
    int col1 = 15;
    int col2 = col1 + btnW + sp;
    int col3 = col2 + btnW + sp;
    int col4 = col3 + btnW + sp;

    int row1 = 90;
    int row2 = row1 + btnH + sp;
    int row3 = row2 + btnH + sp;
    int row4 = row3 + btnH + sp;

    // Visor
    GUI_CreateLabel(&MyApp, 15, 40, "LBF OS - FixedPoint Calc");
    CalcDisplay = GUI_CreateEdit(&MyApp, 15, 55, 225, 30, "0", NULL);

    // Linha 1
    Btn[7]  = GUI_CreateButton(&MyApp, col1, row1, btnW, btnH, "7", OnBtn7Click);
    Btn[8]  = GUI_CreateButton(&MyApp, col2, row1, btnW, btnH, "8", OnBtn8Click);
    Btn[9]  = GUI_CreateButton(&MyApp, col3, row1, btnW, btnH, "9", OnBtn9Click);
    BtnDiv  = GUI_CreateButton(&MyApp, col4, row1, btnW, btnH, "/", OnBtnDivClick);

    // Linha 2
    Btn[4]  = GUI_CreateButton(&MyApp, col1, row2, btnW, btnH, "4", OnBtn4Click);
    Btn[5]  = GUI_CreateButton(&MyApp, col2, row2, btnW, btnH, "5", OnBtn5Click);
    Btn[6]  = GUI_CreateButton(&MyApp, col3, row2, btnW, btnH, "6", OnBtn6Click);
    BtnMul  = GUI_CreateButton(&MyApp, col4, row2, btnW, btnH, "*", OnBtnMulClick);

    // Linha 3
    Btn[1]  = GUI_CreateButton(&MyApp, col1, row3, btnW, btnH, "1", OnBtn1Click);
    Btn[2]  = GUI_CreateButton(&MyApp, col2, row3, btnW, btnH, "2", OnBtn2Click);
    Btn[3]  = GUI_CreateButton(&MyApp, col3, row3, btnW, btnH, "3", OnBtn3Click);
    BtnSub  = GUI_CreateButton(&MyApp, col4, row3, btnW, btnH, "-", OnBtnSubClick);

    // Linha 4
    BtnClear = GUI_CreateButton(&MyApp, col1, row4, btnW, btnH, "C", OnBtnClearClick);
    Btn[0]   = GUI_CreateButton(&MyApp, col2, row4, btnW, btnH, "0", OnBtn0Click);
    BtnEq    = GUI_CreateButton(&MyApp, col3, row4, btnW, btnH, "=", OnBtnEqClick);
    BtnAdd   = GUI_CreateButton(&MyApp, col4, row4, btnW, btnH, "+", OnBtnAddClick);

    // Botão inferior Backspace
    BackButton = GUI_CreateButton(&MyApp, 15, row4 + btnH + 15, 225, 30, "Backspace", OnBtnBackClick);

    // Mantém o visor focado por padrão para reter o cursor/estado
    g_focused_control = (void*)CalcDisplay;
    ultimo_controle_focado = (void*)CalcDisplay;
    gui_set_prop(CalcDisplay, PROP_SET_FOCUS, 1);

    Flush_Grafico_Janela();

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

        // Manutenção persistente do controle focado (Força o foco de volta para o Visor se esvaziar)
        if (g_focused_control != NULL) {
            ultimo_controle_focado = g_focused_control;
        } else if (ultimo_controle_focado != NULL) {
            g_focused_control = ultimo_controle_focado;
            gui_set_prop((TGUIControl*)ultimo_controle_focado, PROP_SET_FOCUS, 1);
        }

        // --- Entrada de Teclado Unificada ---
        char key = Obter_Tecla_Entrada();
        if (key == 0 && euTenhoFocoJanelaReal) {
            key = get_key(); // Fallback teclado físico
        }

        if (key != 0) {
            // Mapeamento direto de atalhos e suporte nativo ao Teclado Virtual
            if (key >= '0' && key <= '9') {
                ProcessarDigito(key - '0');
            } else if (key == '+' || key == '-' || key == '*' || key == '/') {
                ProcessarOperador(key);
            } else if (key == '=' || key == '\n' || key == '\r') {
                CalcularResultado();
            } else if (key == 8 || key == 127) { // Backspace / Delete
                ProcessarBackspace();
            } else if (key == 'c' || key == 'C') {
                LimparCalculadora();
            } else {
                GUI_ProcessKeyboard(&MyApp, key); // Processamento genérico SDK
            }
            precisa_redesenhar = true; 
        }

        // --- Sistema de Roteamento de Clique do Mouse ---
        if (IPC_WINDOW_LIST[my_app_slot].has_click_event == 1) {
            if (mouse_hold_timer == 0) {
                int rel_x = IPC_WINDOW_LIST[my_app_slot].local_click_x;
                int rel_y = IPC_WINDOW_LIST[my_app_slot].local_click_y;
                ultimo_x = rel_x;
                ultimo_y = rel_y;
                mouse_hold_timer = 2; 

                // Feedback visual
                if (BackButton && rel_x >= BackButton->Left && rel_x < (BackButton->Left + BackButton->Width) &&
                    rel_y >= BackButton->Top && rel_y < (BackButton->Top + BackButton->Height)) {
                    gui_set_prop(BackButton, PROP_STATE, 2);
                }
                for (int i = 0; i < 10; i++) {
                    if (Btn[i] && rel_x >= Btn[i]->Left && rel_x < (Btn[i]->Left + Btn[i]->Width) &&
                        rel_y >= Btn[i]->Top && rel_y < (Btn[i]->Top + Btn[i]->Height)) {
                        gui_set_prop(Btn[i], PROP_STATE, 2);
                    }
                }
                if (BtnAdd && rel_x >= BtnAdd->Left && rel_x < (BtnAdd->Left + BtnAdd->Width) && rel_y >= BtnAdd->Top && rel_y < (BtnAdd->Top + BtnAdd->Height)) gui_set_prop(BtnAdd, PROP_STATE, 2);
                if (BtnSub && rel_x >= BtnSub->Left && rel_x < (BtnSub->Left + BtnSub->Width) && rel_y >= BtnSub->Top && rel_y < (BtnSub->Top + BtnSub->Height)) gui_set_prop(BtnSub, PROP_STATE, 2);
                if (BtnMul && rel_x >= BtnMul->Left && rel_x < (BtnMul->Left + BtnMul->Width) && rel_y >= BtnMul->Top && rel_y < (BtnMul->Top + BtnMul->Height)) gui_set_prop(BtnMul, PROP_STATE, 2);
                if (BtnDiv && rel_x >= BtnDiv->Left && rel_x < (BtnDiv->Left + BtnDiv->Width) && rel_y >= BtnDiv->Top && rel_y < (BtnDiv->Top + BtnDiv->Height)) gui_set_prop(BtnDiv, PROP_STATE, 2);
                if (BtnEq  && rel_x >= BtnEq->Left  && rel_x < (BtnEq->Left + BtnEq->Width)   && rel_y >= BtnEq->Top  && rel_y < (BtnEq->Top + BtnEq->Height))  gui_set_prop(BtnEq,  PROP_STATE, 2);
                if (BtnClear && rel_x >= BtnClear->Left && rel_x < (BtnClear->Left + BtnClear->Width) && rel_y >= BtnClear->Top && rel_y < (BtnClear->Top + BtnClear->Height)) gui_set_prop(BtnClear, PROP_STATE, 2);

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

        // Release do clique do mouse
        if (mouse_hold_timer > 0) {
            mouse_hold_timer--; 
            if (mouse_hold_timer == 0) {
                if (BackButton) gui_set_prop(BackButton, PROP_STATE, 0); 
                for (int i = 0; i < 10; i++) {
                    if (Btn[i]) gui_set_prop(Btn[i], PROP_STATE, 0);
                }
                if (BtnAdd) gui_set_prop(BtnAdd, PROP_STATE, 0);
                if (BtnSub) gui_set_prop(BtnSub, PROP_STATE, 0);
                if (BtnMul) gui_set_prop(BtnMul, PROP_STATE, 0);
                if (BtnDiv) gui_set_prop(BtnDiv, PROP_STATE, 0);
                if (BtnEq)  gui_set_prop(BtnEq, PROP_STATE, 0);
                if (BtnClear) gui_set_prop(BtnClear, PROP_STATE, 0);

                events_process_mouse(ultimo_x, ultimo_y, 0, 0); 
                precisa_redesenhar = true;
            }
        }

        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        // Foco ativo = 16ms (~60 FPS), Segundo Plano = 32ms (~30 FPS)
        sys_sleep(euTenhoFocoJanelaReal ? 16 : 32); 
    }

    sys_exit(); 
    return 0;
}
