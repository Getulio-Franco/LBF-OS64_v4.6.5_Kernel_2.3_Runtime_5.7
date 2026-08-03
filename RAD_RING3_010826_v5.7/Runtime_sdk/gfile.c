#include "sdk/libgui.h"
#include "../system/graphics.h"
#include "../gui/wm.h"
#include "../system/string.h"
#include "../system/liblib.h"

// Componentes do Sistema encapsulados
#include "components/TOS_IPC.h"     

// Protótipos obrigatórios de renderização gráfica do subsistema
void gui_draw_form(TForm* form);
void gui_render_form(TForm* form);

// Inclusão de funções de controle mapeadas diretamente no subsistema do Kernel
extern void events_process_mouse(int x, int y, int pressed, int button);
extern void* g_focused_control;

// Variáveis de controle do ambiente da aplicação
int my_app_slot = -1;
TGUIEnvironment MyApp;

// Dimensões globais da janela
const int winWidth = 500;
const int winHeight = 490; 

// Ponteiros Globais dos Componentes para Acesso nos Eventos RAD
TGUIControl* PathEdit       = NULL;
TGUIControl* ActionEdit     = NULL; 
TGUIControl* ExecEdit       = NULL; 
TGUIControl* FileList       = NULL;

TGUIControl* GoButton       = NULL;
TGUIControl* BtnRename      = NULL;
TGUIControl* BtnDel         = NULL;
TGUIControl* BtnCopy        = NULL;
TGUIControl* BtnExecutar    = NULL; 

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
        ext_slot->tem_evento_teclado = 0; 
        return key;
    }
    
    // 2. Se não houver tecla virtual, fallback para o teclado físico do Kernel
    return get_key();
}

/* ============================================================================
 * FUNÇÃO: Carregar_Diretorio
 * ============================================================================ */
void Carregar_Diretorio(const char* path) {
    if (!FileList) return;

    GUI_ListView_Clear(FileList);

    if (sys_fat_chdir(path) != 0) {
        sys_fat_chdir("0:/");
        if (PathEdit) GUI_Edit_SetText(PathEdit, "0:/");
    }

    // Se não estiver na raiz, adiciona o retrocesso de diretório
    if (strcmp(path, "0:/") != 0 && strcmp(path, "0:") != 0) {
        GUI_ListView_AddItem(FileList, "..", 0, 0x10); 
    }

    char nome_arquivo[64];
    file_info_t metadados;
    int idx = 0;
    int resultado;

    while ((resultado = sys_fat_readdir(idx, nome_arquivo, &metadados)) == 1) {
        if (strcmp(nome_arquivo, ".") != 0 && strcmp(nome_arquivo, "..") != 0) {
            GUI_ListView_AddItem(FileList, nome_arquivo, metadados.size, metadados.attributes);
        }
        idx++;
        if (idx > 500) break; 
    }
}

/* ============================================================================
 * FUNÇÃO: Flush_Grafico_Janela
 * ============================================================================ */
void Flush_Grafico_Janela(void) {
    // 1. Desenha os componentes internos do aplicativo (RAD)
    gui_draw_form((TForm*)MyApp.MainWindow);
    gui_render_form((TForm*)MyApp.MainWindow);
        
    // 2. Transfere os buffers via IPC
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
void OnBtnGoClick(void* sender) {
    Carregar_Diretorio(GUI_Edit_GetText(PathEdit));
}

void OnBtnDelClick(void* sender) {
    char* action_text = GUI_Edit_GetText(ActionEdit);
    if (action_text && action_text[0] != '\0') {
        sys_fat_rm(action_text);
        Carregar_Diretorio(GUI_Edit_GetText(PathEdit)); 
        GUI_Edit_SetText(ActionEdit, ""); 
    }
}

void Executar_Binario_Seguro(void) {
    char* caminho_elf = GUI_Edit_GetText(ExecEdit);
    if (!caminho_elf || caminho_elf[0] == '\0') return;

    // Dispara a execução assíncrona para a task_d no Ring 0
    sys_exec(caminho_elf);

    // Limpa o campo de texto
    GUI_Edit_SetText(ExecEdit, "");
}

void OnBtnExecutarClick(void* sender) {
    Executar_Binario_Seguro();
}

void Tratar_Modificacao_Arquivo(bool is_rename) {
    char* action_text = GUI_Edit_GetText(ActionEdit);
    if (!action_text || action_text[0] == '\0') return;

    char temp_buffer[256];
    strncpy(temp_buffer, action_text, 255);
    temp_buffer[255] = '\0'; 
    
    char* space_ptr = NULL;
    int len = strlen(temp_buffer);
    for(int i = 0; i < len; i++) {
        if(temp_buffer[i] == ' ') { 
            space_ptr = &temp_buffer[i]; 
            break; 
        }
    }
    
    if (space_ptr) {
        *space_ptr = '\0'; 
        char* arg1 = temp_buffer;
        char* arg2 = space_ptr + 1;
        
        if (is_rename) sys_fat_rename(arg1, arg2);
        else           sys_fat_copy(arg1, arg2);
        
        Carregar_Diretorio(GUI_Edit_GetText(PathEdit)); 
        GUI_Edit_SetText(ActionEdit, ""); 
    }
}

void OnBtnRenameClick(void* sender) {
    Tratar_Modificacao_Arquivo(true);
}

void OnBtnCopyClick(void* sender) {
    Tratar_Modificacao_Arquivo(false);
}

void OnFileListChange(void* sender) {
    int idx = gui_get_prop(FileList, PROP_ITEM_INDEX);
    if (idx == -1) return;

    char nome_sel[64];
    uint32_t tam_arquivo = 0; 
    uint8_t attributes = 0;

    GUI_ListView_GetItem(FileList, idx, nome_sel, &tam_arquivo, &attributes);

    if (attributes & 0x10) { 
        if (strcmp(nome_sel, "..") == 0) {
            Carregar_Diretorio("0:/"); 
            if (PathEdit) GUI_Edit_SetText(PathEdit, "0:/");
        } else {
            char novo_caminho[256];
            strncpy(novo_caminho, GUI_Edit_GetText(PathEdit), 255);
            novo_caminho[255] = '\0';
            
            int path_len = strlen(novo_caminho);
            if (path_len > 0 && novo_caminho[path_len - 1] != '/') {
                strcat(novo_caminho, "/");
            }
            strcat(novo_caminho, nome_sel);
            
            GUI_Edit_SetText(PathEdit, novo_caminho);
            Carregar_Diretorio(novo_caminho);
        }
    } else {
        GUI_Edit_SetText(ActionEdit, nome_sel);
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
    void* ultimo_controle_focado = NULL;

    // Inicialização do Ambiente Gráfico e do Servidor WM
    graphics_init_app(winWidth, winHeight);
    wm_init();
    
    my_app_slot = OS_IPC_RegisterApp("Gerenciador de Arquivos", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "Gerenciador de Arquivos v0.0.4 14/07/26", winWidth, winHeight);

    // Ajusta o fundo do formulário principal para preto
    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x000000);
    }

    /* =========================================================================
     * DESIGN LAYOUT RAD ENCAPSULADO
     * ========================================================================= */
    GUI_CreateLabel(&MyApp, 10, 42, "Caminho:");
    PathEdit = GUI_CreateEdit(&MyApp, 70, 38, 330, 25, "0:/", NULL);
    GoButton = GUI_CreateButton(&MyApp, 410, 38, 80, 25, "Navegar", OnBtnGoClick);

    FileList = GUI_CreateListView(&MyApp, 10, 75, 480, 275, OnFileListChange);
    gui_set_prop(FileList, PROP_ITEM_INDEX, -1);

    GUI_CreateLabel(&MyApp, 10, 364, "Acao:");
    ActionEdit = GUI_CreateEdit(&MyApp, 70, 360, 420, 25, "", NULL);

    BtnRename = GUI_CreateButton(&MyApp, 10,  395, 150, 25, "Renomear", OnBtnRenameClick);
    BtnDel    = GUI_CreateButton(&MyApp, 175, 395, 150, 25, "Deletar", OnBtnDelClick);
    BtnCopy   = GUI_CreateButton(&MyApp, 340, 395, 150, 25, "Copiar", OnBtnCopyClick);

    GUI_CreateLabel(&MyApp, 10, 429, "Executar:");
    ExecEdit = GUI_CreateEdit(&MyApp, 70, 425, 420, 25, "", NULL); 
    BtnExecutar = GUI_CreateButton(&MyApp, 10, 455, 150, 25, "Executar", OnBtnExecutarClick);

    // Foco inicial padrão
    g_focused_control = (void*)PathEdit;
    ultimo_controle_focado = (void*)PathEdit;
    gui_set_prop(PathEdit, PROP_SET_FOCUS, 1);

    // REMOVIDO: Carregar_Diretorio("0:/"); 
    // O diretório agora só é lido ao clicar no botão "Navegar" (GoButton)

    // Primeiro desenho obrigatório
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

        // Gatilho do frame inicial
        if (primeiro_desenho) {
            primeiro_desenho = false;
            precisa_redesenhar = true;
        }

        // Verifica se o foco da janela mudou no S.O.
        bool euTenhoFoco = (IPC_CONTROL->active_focus_slot == my_app_slot);
        if (euTenhoFoco != ultimo_estado_foco) {
            ultimo_estado_foco = euTenhoFoco;
            if (MyApp.MainWindow) {
                ((TForm*)MyApp.MainWindow)->ActiveFocus = euTenhoFoco;
            }
            precisa_redesenhar = true;
        }

        // Manutenção persistente do controle focado (Cursor piscando)
        if (g_focused_control != NULL) {
            ultimo_controle_focado = g_focused_control;
        } else if (ultimo_controle_focado != NULL) {
            g_focused_control = ultimo_controle_focado;
            gui_set_prop((TGUIControl*)ultimo_controle_focado, PROP_SET_FOCUS, 1);
        }

        // --- Entrada de Teclado (Híbrida: Virtual + Física) ---
        if (euTenhoFoco) {
            char key = Obter_Tecla_Entrada();
            if (key != 0) {
                GUI_ProcessKeyboard(&MyApp, key); 
                precisa_redesenhar = true;
            }
        }

        // --- Entrada de Mouse (Clicks) ---
        if (IPC_WINDOW_LIST[my_app_slot].has_click_event == 1) {
            if (mouse_hold_timer == 0) {
                int rel_x = IPC_WINDOW_LIST[my_app_slot].local_click_x;
                int rel_y = IPC_WINDOW_LIST[my_app_slot].local_click_y;
                ultimo_x = rel_x; 
                ultimo_y = rel_y; 
                mouse_hold_timer = 2; 

                // Feedback visual de pressionado
                if (GoButton && rel_x >= GoButton->Left && rel_x < (GoButton->Left + GoButton->Width) && rel_y >= GoButton->Top && rel_y < (GoButton->Top + GoButton->Height)) {
                    gui_set_prop(GoButton, PROP_STATE, 2);
                }
                else if (BtnRename && rel_x >= BtnRename->Left && rel_x < (BtnRename->Left + BtnRename->Width) && rel_y >= BtnRename->Top && rel_y < (BtnRename->Top + BtnRename->Height)) {
                    gui_set_prop(BtnRename, PROP_STATE, 2);
                }
                else if (BtnDel && rel_x >= BtnDel->Left && rel_x < (BtnDel->Left + BtnDel->Width) && rel_y >= BtnDel->Top && rel_y < (BtnDel->Top + BtnDel->Height)) {
                    gui_set_prop(BtnDel, PROP_STATE, 2);
                }
                else if (BtnCopy && rel_x >= BtnCopy->Left && rel_x < (BtnCopy->Left + BtnCopy->Width) && rel_y >= BtnCopy->Top && rel_y < (BtnCopy->Top + BtnCopy->Height)) {
                    gui_set_prop(BtnCopy, PROP_STATE, 2);
                }
                else if (BtnExecutar && rel_x >= BtnExecutar->Left && rel_x < (BtnExecutar->Left + BtnExecutar->Width) && rel_y >= BtnExecutar->Top && rel_y < (BtnExecutar->Top + BtnExecutar->Height)) {
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

        // --- Release do Clique ---
        if (mouse_hold_timer > 0) {
            mouse_hold_timer--; 
            if (mouse_hold_timer == 0) {
                if (GoButton)    gui_set_prop(GoButton, PROP_STATE, 0);    
                if (BtnRename)   gui_set_prop(BtnRename, PROP_STATE, 0);
                if (BtnDel)      gui_set_prop(BtnDel, PROP_STATE, 0);
                if (BtnCopy)     gui_set_prop(BtnCopy, PROP_STATE, 0);
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
