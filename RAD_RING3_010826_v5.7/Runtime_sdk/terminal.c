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

#define CMD_BUFFER_SIZE 128

// Variáveis de controle da aplicação usando a nova TGUIEnvironment estável
int my_app_slot = -1;
TGUIEnvironment MyApp;

// Configurações Globais de Dimensão
const int winWidth = 600;
const int winHeight = 400;

// Componente RAD do Terminal
TGUIControl* TerminalMemo = NULL;

char cmd_buffer[CMD_BUFFER_SIZE];
int cmd_idx = 0;

/* ============================================================================
 * ESTRUTURA AUXILIAR IPC (Mapeamento de Eventos Estendidos para Teclado Virtual)
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
    // 1. Tenta ler do Teclado Virtual (Via área estendida IPC)
    AppWindowInfoExtended* ext_slot = (AppWindowInfoExtended*)&IPC_WINDOW_LIST[my_app_slot];
    if (ext_slot->tem_evento_teclado == 1) {
        char key = (char)ext_slot->fila_teclado_virtual;
        ext_slot->tem_evento_teclado = 0; // Consome o evento
        return key;
    }
    
    // 2. Se não houver tecla virtual, fallback para o teclado físico do Kernel
    return get_key();
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
 * FUNÇÃO: Processar_Comando_Terminal
 * ============================================================================ */
void Processar_Comando_Terminal(char* input) {
    if (strcmp(input, "clear") == 0 || strcmp(input, "cls") == 0) {
        GUI_Memo_Clear(TerminalMemo);
    }
    else if (strcmp(input, "ps") == 0) {
        TProcessInfo p_info[32];
        int total = sys_get_ps_data(p_info, 32);
    
        if (total <= 0) {
            GUI_Memo_AddStr(TerminalMemo, "Nenhum processo ativo retornado pelo Kernel.\n");
            return;
        }

        GUI_Memo_AddStr(TerminalMemo, "PID     STATUS      NOME DO PROCESSO\n");
        GUI_Memo_AddStr(TerminalMemo, "--------------------------------------------------\n");
    
        for (int i = 0; i < total; i++) {
            char pid_str[16];
            char item_linha[128];
        
            itoa((uint64_t)p_info[i].pid, pid_str, 10);
            strcpy(item_linha, pid_str);
        
            while(strlen(item_linha) < 8) strcat(item_linha, " ");
        
            strcat(item_linha, p_info[i].state == 1 ? "RUNNING     " : "READY       ");
            strcat(item_linha, p_info[i].name);
            strcat(item_linha, "\n");
        
            GUI_Memo_AddStr(TerminalMemo, item_linha);
        }
    }
    else if (strcmp(input, "explorer") == 0) {
        GUI_Memo_AddStr(TerminalMemo, "Erro: explorer.elf (Compositor/Desktop) ja esta em execucao.\n");
    }
    else if (strcmp(input, "gfile") == 0) {
        GUI_Memo_AddStr(TerminalMemo, "Iniciando Gerenciador de Arquivos (gfile.elf)...\n");
        sys_exec("gfile.elf");
    }
    else if (strncmp(input, "ls", 2) == 0 || strcmp(input, "dir") == 0) {
        char nome_arquivo[16];
        file_info_t info;
        int idx = 0;
        int encontrados = 0;
        
        GUI_Memo_AddStr(TerminalMemo, "Diretorio Atual (FAT32):\n");
        GUI_Memo_AddStr(TerminalMemo, "--------------------------------------------------\n");

        while (sys_fat_readdir(idx, nome_arquivo, &info) == 1) {
            char item_linha[64];
            encontrados++;
            
            strcpy(item_linha, " ");
            strcat(item_linha, nome_arquivo);
            
            if (info.attributes & 0x10) {
                strcat(item_linha, "\t\t<DIR>\n");
            } else {
                char tam_str[16];
                itoa((uint64_t)info.size, tam_str, 10);
    
                strcat(item_linha, "  -> "); 
                strcat(item_linha, tam_str);
                strcat(item_linha, " Bytes\n");
            }
            
            GUI_Memo_AddStr(TerminalMemo, item_linha);
            idx++;
        }
        
        if (encontrados == 0) {
            GUI_Memo_AddStr(TerminalMemo, "Diretorio vazio ou sistema de arquivos nao montado.\n");
        }
    }
    else {
        GUI_Memo_AddStr(TerminalMemo, "Comando desconhecido ou indisponivel neste modo.\n");
    }
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
    
    my_app_slot = OS_IPC_RegisterApp("Terminal LBF OS", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "Terminal LBF OS v2.6", winWidth, winHeight);

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x000000);
    }

    // --- MONTAGEM DA INTERFACE RAD ---
    GUI_CreateLabel(&MyApp, 10, 45, "LBF OS Prompt de Comando:");
    
    TerminalMemo = GUI_CreateMemo(&MyApp, 10, 75, 580, 315);
    gui_set_prop(TerminalMemo, PROP_COLOR, 0x000000); 
    
    GUI_Memo_AddStr(TerminalMemo, "LBF OS Terminal v2.6 [Modo RAD Limpo]\n");
    GUI_Memo_AddStr(TerminalMemo, "C:\\>");

    // Configuração correta de Foco passando o objeto da SDK diretamente
    g_focused_control = (void*)TerminalMemo;
    gui_set_prop(TerminalMemo, PROP_SET_FOCUS, 1);
    
    memset(cmd_buffer, 0, CMD_BUFFER_SIZE);

    /* =========================================================================
     * LOOP DE EVENTOS 100% REATIVO (Padronizado)
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

        // Se o aplicativo acabou de ganhar ou perder o foco, atualiza a janela
        if (euTenhoFoco != ultimo_estado_foco) {
            ultimo_estado_foco = euTenhoFoco;
            precisa_redesenhar = true;
        }

        if (euTenhoFoco) {
            // --- ENTRADA CENTRALIZADA DE TECLADO (Padrão Unificado) ---
            char key = Obter_Tecla_Entrada();

            if (key != 0) {
                precisa_redesenhar = true; // Evento de digitação: Redesenha!
                
                if (key == '\n' || key == '\r') {
                    cmd_buffer[cmd_idx] = '\0';
                    GUI_Memo_AddStr(TerminalMemo, "\n");
                    
                    if (cmd_idx > 0) {
                        Processar_Comando_Terminal(cmd_buffer);
                    }
                    
                    GUI_Memo_AddStr(TerminalMemo, "C:\\>");
                    cmd_idx = 0;
                    memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
                }
                else if ((key == '\b' || key == 8) && cmd_idx > 0) {
                    cmd_idx--;
                    cmd_buffer[cmd_idx] = '\0';
                    
                    char backspace_str[2] = {'\b', '\0'};
                    GUI_Memo_AddStr(TerminalMemo, backspace_str);
                }
                else if (cmd_idx < CMD_BUFFER_SIZE - 1 && key >= 32 && key <= 126) {
                    cmd_buffer[cmd_idx++] = key;
                    char temp[2] = {key, '\0'};
                    GUI_Memo_AddStr(TerminalMemo, temp); 
                }
            }

            // --- Captura de Cliques de Mouse ---
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

            // --- Timer de Liberação do Mouse ---
            if (mouse_hold_timer > 0) {
                mouse_hold_timer--; 
                if (mouse_hold_timer == 0) {
                    events_process_mouse(ultimo_x, ultimo_y, 0, 0); 
                    precisa_redesenhar = true;
                }
            }
        }

        // Só envia dados para o compositor gráfico se o estado interno mudou
        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        sys_sleep(euTenhoFoco ? 16 : 32);
    }

    sys_exit(); 
    return 0;
}
