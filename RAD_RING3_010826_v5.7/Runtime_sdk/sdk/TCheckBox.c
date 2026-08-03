#include "libgui.h"

/* ============================================================================
 * CRIAÇÃO E MÉTODOS PÚBLICOS DO TCHECKBOX
 * ============================================================================ */
TGUIControl* GUI_CreateCheckBox(TGUIEnvironment* app, int x, int y, const char* caption, TEventChange onChange) {
    if (!app) return NULL;

    TGUIControl* cb = (TGUIControl*)malloc(sizeof(TGUIControl));
    if (!cb) return NULL;
    memset(cb, 0, sizeof(TGUIControl));

    cb->Type = TYPE_CHECKBOX; 
    cb->Left = x;
    cb->Top = y;
    cb->Width = 140;          
    cb->Height = 20;
    cb->Checked = false;      
    cb->IsSelected = false;
    cb->OnChange = onChange;   // Callback para notificar a aplicação

    // Registro unificado no motor do App
    GUI_RegisterControl(app, cb, "CheckBox");

    // Acoplamento físico com o subsistema gráfico do Kernel
    cb->KernelHandle = (uint64_t)gui_create_checkbox((TWinControl*)app->MainWindow, cb->Name, (char*)caption);
    if (cb->KernelHandle == 0) {
        free(cb);
        return NULL;
    }

    // Sincronização inicial de propriedades no Ring 0
    gui_set_prop((void*)cb->KernelHandle, PROP_LEFT,   (uint64_t)cb->Left);
    gui_set_prop((void*)cb->KernelHandle, PROP_TOP,    (uint64_t)cb->Top);
    gui_set_prop((void*)cb->KernelHandle, PROP_WIDTH,  (uint64_t)cb->Width);
    gui_set_prop((void*)cb->KernelHandle, PROP_HEIGHT, (uint64_t)cb->Height);

    return cb;
}

void GUI_CheckBox_Toggle(TGUIControl* cb) {
    if (!cb || !cb->KernelHandle) return;

    // Inverte o estado booleano de 1 byte no Ring 3
    cb->Checked = !cb->Checked;

    // Sincroniza o estado com o Kernel. 
    // Usamos o PROP_STATE (ou mude para PROP_CHECKED se o seu gui.h for mapeado assim)
#ifdef PROP_STATE
    gui_set_prop((void*)cb->KernelHandle, PROP_STATE, (uint64_t)cb->Checked);
#endif

    // 🔥 Dispara o evento de mudança automática para a aplicação do usuário
    if (cb->OnChange != NULL) {
        cb->OnChange(cb);
    }
}
