#include "libgui.h"
#include "../gui/wm.h"

/* ============================================================================
 * CRIAÇÃO E DESTRUIÇÃO DE FORMULÁRIOS SECUNDÁRIOS (RING 3)
 * ============================================================================ */
TGUIControl* GUI_CreateForm(const char* title, int x, int y, int width, int height) {
    // 1. Aloca e limpa a estrutura de controle do Formulário
    TGUIControl* vform = (TGUIControl*)malloc(sizeof(TGUIControl));
    if (!vform) return NULL;
    memset(vform, 0, sizeof(TGUIControl));

    // 2. Configura as propriedades geométricas locais de Runtime
    vform->Type = TYPE_FORM; // Assumindo que TYPE_FORM está mapeado no seu gui.h/controls.h
    vform->Left = x;
    vform->Top = y;
    vform->Width = width;
    vform->Height = height;
    vform->Modal = false;
    vform->BorderStyle = bsSingle; // Padrão inicial

    // Fallback de string segura para o título da janela
    const char* window_title = (title && title[0] != '\0') ? title : "Form";

    // 3. Invoca a criação do formulário nativo direto no gerenciador do Kernel
    TForm* kernel_win = gui_create_form("SubForm", (char*)window_title, 1);
    if (!kernel_win) {
        free(vform);
        return NULL;
    }
    
    // Guarda o ponteiro físico de 64 bits de forma segura
    vform->KernelHandle = (uint64_t)kernel_win;

    // 4. Sincroniza a geometria inicial com a GUI do Kernel via Syscall gui_set_prop
    gui_set_prop((void*)kernel_win, PROP_LEFT,   (uint64_t)vform->Left);
    gui_set_prop((void*)kernel_win, PROP_TOP,    (uint64_t)vform->Top);
    gui_set_prop((void*)kernel_win, PROP_WIDTH,  (uint64_t)vform->Width);
    gui_set_prop((void*)kernel_win, PROP_HEIGHT, (uint64_t)vform->Height);

    // 5. Configura a propriedade física de arrasto da estrutura interna do Kernel
    kernel_win->Win.Draggable = true;
    
    // 6. Registra e renderiza a nova janela no gerenciador central (Window Manager)
    wm_add_window(kernel_win);

    return vform;
}

void GUI_DestroyForm(TGUIControl* form) {
    if (!form) return;
    
    // Se o seu Kernel possuir uma chamada de remoção (Ex: wm_remove_window), ela entraria aqui.
    // Como estamos gerenciando a memória alocada no Ring 3:
    free(form);
}

/*
// Evento de clique do botão na tela principal que abre a tela secundária
void OnAbreConfigClick(void* sender) {
    // Cria uma nova janela independente
    TGUIControl* frmConfig = GUI_CreateForm("Configurações do Sistema", 150, 150, 400, 300);
    
    if (frmConfig) {
        // Altera para estilo diálogo se desejado no futuro
        frmConfig->BorderStyle = bsDialog; 
        
        // Passando (TWinControl*)frmConfig->KernelHandle, o Kernel sabe 
        // que os novos componentes devem ser desenhados DENTRO desta sub-janela!
        TGUIControl* btnSalvar = GUI_CreateButton(app, 20, 40, 100, 30, "Salvar", OnSalvarClick);
    }
}
*/
