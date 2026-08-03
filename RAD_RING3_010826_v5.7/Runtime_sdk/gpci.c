#include "sdk/libgui.h"
#include "../system/graphics.h"
#include "../gui/wm.h"
#include "../system/string.h"
#include "../system/liblib.h"

// Componentes do Sistema encapsulados
#include "components/TOS_IPC.h"     
#include "components/TOSSerial.h"   

// Protótipos obrigatórios de renderização gráfica
void gui_draw_form(TForm* form);
void gui_render_form(TForm* form);

// Inclusão de funções de controle mapeadas diretamente no subsistema do Kernel
extern void events_process_mouse(int x, int y, int pressed, int button);
extern void* g_focused_control;

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor_id;   // Mantido no topo conforme barramento_pci.h
    uint16_t device_id;   // Mantido no topo conforme barramento_pci.h
    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t prog_if;      
    uint8_t encontrado;   // Atualizado para coincidir byte a byte
} __attribute__((packed)) pci_device_t;

// Variáveis de controle do ambiente da aplicação
int my_app_slot = -1;
TGUIEnvironment MyApp;

// Janela expandida para melhor visualização dos dados do hardware
const int winWidth = 560;
const int winHeight = 410;

// Ponteiros globais RAD para acesso no callback
TGUIControl* DevMemo    = NULL;
TGUIControl* BtnEscanear = NULL;

/* ============================================================================
 * FUNÇÃO AUXILIAR: Decodificar_Classe_PCI
 * ============================================================================ */
const char* Decodificar_Classe_PCI(uint8_t class_id) {
    switch (class_id) {
        case 0x00: return "Pre-Classificado";
        case 0x01: return "Armazenamento (SATA/IDE)";
        case 0x02: return "Interface de Rede (Ethernet)";
        case 0x03: return "Controladora de Video (VGA/VESA)";
        case 0x04: return "Dispositivo Multimedia (Audio)";
        case 0x05: return "Controladora de Memoria";
        case 0x06: return "Ponte de Barramento (Bridge Host/PCI)";
        case 0x07: return "Comunicacao Simples (Serial)";
        case 0x08: return "Periferico de Sistema (PIC/DMA)";
        case 0x09: return "Dispositivo de Entrada";
        case 0x0A: return "Controladora Docking Station";
        case 0x0B: return "Processador Embutido";
        case 0x0C: return "Barramento Serial (USB/EHCI/xHCI)";
        default:   return "Periferico Desconhecido";
    }
}

/* ============================================================================
 * FUNÇÃO: Adicionar_Hex_Formatado
 * ============================================================================ */
void Adicionar_Hex_Formatado(TGUIControl* memo, uint64_t valor, int digitos) {
    char tmp[32];
    char pad[32];
    itoa(valor, tmp, 16);
    
    int tam = strlen(tmp);
    int zeros = digitos - tam;
    
    strcpy(pad, "0x");
    for(int i = 0; i < zeros; i++) {
        strcat(pad, "0");
    }
    strcat(pad, tmp);
    GUI_Memo_AddStr(memo, pad); 
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
 * FUNÇÃO: Tratar_Varredura_Hardware
 * ============================================================================ */
void Tratar_Varredura_Hardware(TGUIControl* memo) {
    pci_device_t dev;
    int index = 0;
    int encontrados = 0;
    char total_str[16];

    GUI_Memo_AddStr(memo, "\n==================================================\n");
    GUI_Memo_AddStr(memo, "    RELATORIO DE PERIFERICOS DO SISTEMA (RING 3)\n");
    GUI_Memo_AddStr(memo, "==================================================\n");

    while (1) {
        int resultado = sys_get_pci_device(index, (uintptr_t)&dev);
        if (resultado == -1) break; 

        GUI_Memo_AddStr(memo, "Item ");
        char idx_str[8];
        itoa(index, idx_str, 10);
        GUI_Memo_AddStr(memo, idx_str);
        GUI_Memo_AddStr(memo, " - B:S:F  ");

        Adicionar_Hex_Formatado(memo, dev.bus, 2);
        GUI_Memo_AddStr(memo, ":");
        Adicionar_Hex_Formatado(memo, dev.slot, 2);
        GUI_Memo_AddStr(memo, ":");
        Adicionar_Hex_Formatado(memo, dev.func, 2);
        GUI_Memo_AddStr(memo, "\n");

        GUI_Memo_AddStr(memo, "   Class: ");
        Adicionar_Hex_Formatado(memo, dev.class_id, 2);
        GUI_Memo_AddStr(memo, "   Subclass: ");
        Adicionar_Hex_Formatado(memo, dev.subclass_id, 2);
        GUI_Memo_AddStr(memo, " -> ");
        GUI_Memo_AddStr(memo, Decodificar_Classe_PCI(dev.class_id));
        GUI_Memo_AddStr(memo, "\n");

        GUI_Memo_AddStr(memo, "   Vendor: ");
        Adicionar_Hex_Formatado(memo, dev.vendor_id, 4);
        GUI_Memo_AddStr(memo, " | Device: ");
        Adicionar_Hex_Formatado(memo, dev.device_id, 4);
        GUI_Memo_AddStr(memo, "\n--------------------------------------------------\n");

        index++;
        encontrados++;
    }

    GUI_Memo_AddStr(memo, ">> Fim da varredura. Total de dispositivos: ");
    itoa(encontrados, total_str, 10);
    GUI_Memo_AddStr(memo, total_str);
    GUI_Memo_AddStr(memo, "\n\n");
}

/* ============================================================================
 * CALLBACK RAD: Clique do Botão Escanear
 * ============================================================================ */
void OnBtnEscanearClick(void* sender) {
    Tratar_Varredura_Hardware(DevMemo);
}

/* ============================================================================
 * FUNÇÃO PRINCIPAL (MAIN)
 * ============================================================================ */
int main(int argc, char* argv[]) {
    static int ultimo_x = 0;
    static int ultimo_y = 0;
    static int mouse_hold_timer = 0; 

    // Monitoramento inteligente de renderização e estado do foco
    static bool primeiro_desenho = true;
    static bool ultimo_estado_foco = false;

    // Inicialização do Subsistema Gráfico e Window Manager
    graphics_init_app(winWidth, winHeight);
    wm_init();
    
    my_app_slot = OS_IPC_RegisterApp("Gerenciador de Dispositivos", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "Gerenciador de Dispositivos RAD LBF v0.0.2", winWidth, winHeight);

    // Modifica o plano de fundo do Form principal para preto
    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x000000);
    }

    /* =========================================================================
     * DESIGN LAYOUT RAD MODERNO
     * ========================================================================= */
    BtnEscanear = GUI_CreateButton(&MyApp, 15, 45, 320, 32, "ESCANEAR BARRAMENTO PCI", OnBtnEscanearClick);
    
    GUI_CreateLabel(&MyApp, 15, 87, "Dispositivos Mapeados em Espaco de Usuario (Ring 3):");
    
    DevMemo = GUI_CreateMemo(&MyApp, 15, 107, 530, 285);
    GUI_Memo_AddStr(DevMemo, "Sistema Pronto. Pressione o botao acima para capturar o hardware via Syscall.\n");

    // Renderização Inicial Unificada (Cold Start)
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

        // Alternou o estado de foco? Força o redesenho imediatamente!
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

                    // Feedback visual do botão de escaneamento
                    if (BtnEscanear && rel_x >= BtnEscanear->Left && rel_x < (BtnEscanear->Left + BtnEscanear->Width) &&
                        rel_y >= BtnEscanear->Top && rel_y < (BtnEscanear->Top + BtnEscanear->Height)) {
                        gui_set_prop(BtnEscanear, PROP_STATE, 2);
                    }

                    events_process_mouse(rel_x, rel_y, 1, 0);
                    
                    // Permite interagir estavelmente com o Memo e suas barras de rolagem
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
                    if (BtnEscanear) gui_set_prop(BtnEscanear, PROP_STATE, 0); 
                    events_process_mouse(ultimo_x, ultimo_y, 0, 0); 
                    precisa_redesenhar = true;
                }
            }
        }

        // CORREÇÃO: Fora do bloco para atualizar o visual das janelas mesmo desfocadas
        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        sys_sleep(euTenhoFoco ? 16 : 32);
    }

    sys_exit(); 
    return 0;
}
