#include "libgui.h"

TGUIControl* GUI_CreateLabel(TGUIEnvironment* app, int x, int y, const char* caption) {
    if (!app) return NULL;

    TGUIControl* lbl = (TGUIControl*)malloc(sizeof(TGUIControl));
    if (!lbl) return NULL;
    memset(lbl, 0, sizeof(TGUIControl));

    lbl->Type = TYPE_LABEL; 
    lbl->Left = x;
    lbl->Top = y;
    lbl->Width = 80;        // Largura padrão da SDK
    lbl->Height = 16;       // Altura padrão da SDK para fontes do Kernel
    lbl->IsSelected = false;
    lbl->OnClick = NULL;    // Labels nativamente não possuem evento de clique inicial

    GUI_RegisterControl(app, lbl, "Label");

    lbl->KernelHandle = (uint64_t)gui_create_label((TWinControl*)app->MainWindow, lbl->Name, (char*)caption);
    if (lbl->KernelHandle == 0) {
        free(lbl);
        return NULL;
    }

    gui_set_prop((void*)lbl->KernelHandle, PROP_LEFT,   (uint64_t)lbl->Left);
    gui_set_prop((void*)lbl->KernelHandle, PROP_TOP,    (uint64_t)lbl->Top);
    gui_set_prop((void*)lbl->KernelHandle, PROP_WIDTH,  (uint64_t)lbl->Width);
    gui_set_prop((void*)lbl->KernelHandle, PROP_HEIGHT, (uint64_t)lbl->Height);

    return lbl;
}
