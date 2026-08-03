#include "libgui.h"

TGUIControl* GUI_CreatePanel(TGUIEnvironment* app, int x, int y, int w, int h) {
    if (!app) return NULL;

    TGUIControl* pnl = (TGUIControl*)malloc(sizeof(TGUIControl));
    if (!pnl) return NULL;
    memset(pnl, 0, sizeof(TGUIControl));

    pnl->Type = TYPE_PANEL; // Valor 3 vindo do gui.h
    pnl->Left = x;
    pnl->Top = y;
    pnl->Width = w;
    pnl->Height = h;
    pnl->IsSelected = false;
    pnl->BevelWidth = 2;
    pnl->Parent = NULL; // Inicialmente acoplado à MainWindow

    // Registro centralizado no motor da SDK
    GUI_RegisterControl(app, pnl, "Panel");

    // Instanciação física no Kernel
    pnl->KernelHandle = (uint64_t)gui_create_panel((TWinControl*)app->MainWindow, pnl->Name);
    if (pnl->KernelHandle == 0) {
        free(pnl);
        return NULL;
    }

    // Sincronização inicial de propriedades no Ring 0
    gui_set_prop((void*)pnl->KernelHandle, PROP_LEFT,   (uint64_t)pnl->Left);
    gui_set_prop((void*)pnl->KernelHandle, PROP_TOP,    (uint64_t)pnl->Top);
    gui_set_prop((void*)pnl->KernelHandle, PROP_WIDTH,  (uint64_t)pnl->Width);
    gui_set_prop((void*)pnl->KernelHandle, PROP_HEIGHT, (uint64_t)pnl->Height);
    gui_set_prop((void*)pnl->KernelHandle, PROP_COLOR,  0xC0C0C0);

#ifdef PROP_BEVEL
    gui_set_prop((void*)pnl->KernelHandle, PROP_BEVEL,  (uint64_t)pnl->BevelWidth);
#endif

    return pnl;
}

void GUI_SetParent(TGUIControl* control, TGUIControl* new_parent) {
    if (!control || !control->KernelHandle || !new_parent || !new_parent->KernelHandle) return;

    // 1. Atualiza o mapeamento lógico no Ring 3
    control->Parent = new_parent;

    // 2. Altera a árvore de renderização/reparentabilidade física direto no Kernel
    gui_add_to_parent((void*)new_parent->KernelHandle, (void*)control->KernelHandle);
}
