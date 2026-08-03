#include "libgui.h"

// Mapeamento externo para a chamada física do Kernel
extern void* gui_create_radio(void* parent, const char* name, const char* caption);

/* ============================================================================
 * CRIAÇÃO E MÉTODOS PÚBLICOS DO TRADIOBUTTON
 * ============================================================================ */
TGUIControl* GUI_CreateRadioButton(TGUIEnvironment* app, int x, int y, const char* caption, TEventChange onChange) {
    if (!app) return NULL;

    TGUIControl* rb = (TGUIControl*)malloc(sizeof(TGUIControl));
    if (!rb) return NULL;
    memset(rb, 0, sizeof(TGUIControl));

    rb->Type = TYPE_RADIOBUTTON; 
    rb->Left = x;
    rb->Top = y;
    rb->Width = 120;
    rb->Height = 20;
    rb->IsSelected = false;
    rb->Checked = false;
    rb->GroupIndex = 0;   // Grupo padrão inicial
    rb->OnChange = onChange;

    // Registro unificado no motor do App
    GUI_RegisterControl(app, rb, "RadioButton");

    // Instanciação física no Subsistema do Kernel
    rb->KernelHandle = (uint64_t)gui_create_radio((TWinControl*)app->MainWindow, rb->Name, (char*)caption);
    if (rb->KernelHandle == 0) {
        free(rb);
        return NULL;
    }

    // Sincronização inicial de propriedades no Ring 0
    gui_set_prop((void*)rb->KernelHandle, PROP_LEFT,   (uint64_t)rb->Left);
    gui_set_prop((void*)rb->KernelHandle, PROP_TOP,    (uint64_t)rb->Top);
    gui_set_prop((void*)rb->KernelHandle, PROP_WIDTH,  (uint64_t)rb->Width);
    gui_set_prop((void*)rb->KernelHandle, PROP_HEIGHT, (uint64_t)rb->Height);
    gui_set_prop((void*)rb->KernelHandle, PROP_COLOR,  0xC0C0C0);

    return rb;
}

void GUI_RadioButton_Select(TGUIEnvironment* app, TGUIControl* target_rb) {
    if (!app || !target_rb || !target_rb->KernelHandle) return;

    // Se já estiver marcado, ignora para evitar overhead
    if (target_rb->Checked) return;

    // Varre todos os controles registrados para desmarcar os irmãos do mesmo grupo
    for (int i = 0; i < app->ControlCount; i++) {
        TGUIControl* current_ctrl = app->Controls[i];

        if (current_ctrl && current_ctrl->Type == TYPE_RADIOBUTTON) {
            // Se pertencer ao mesmo grupo de exclusão mútua
            if (current_ctrl->GroupIndex == target_rb->GroupIndex) {
                current_ctrl->Checked = false;
                
#ifdef PROP_CHECKED
                gui_set_prop((void*)current_ctrl->KernelHandle, PROP_CHECKED, 0);
#endif
            }
        }
    }

    // Marca o botão alvo como o único ativo do grupo
    target_rb->Checked = true;

#ifdef PROP_CHECKED
    gui_set_prop((void*)target_rb->KernelHandle, PROP_CHECKED, 1);
#endif

    // Dispara o callback informando o App sobre a mudança
    if (target_rb->OnChange != NULL) {
        target_rb->OnChange(target_rb);
    }
}
