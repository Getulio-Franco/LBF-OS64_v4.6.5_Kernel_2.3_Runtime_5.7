#include "libgui.h"

TGUIControl* GUI_CreateButton(TGUIEnvironment* app, int x, int y, int w, int h, const char* caption, TEventClick onClick) {
    if (!app) return NULL;

    TGUIControl* btn = (TGUIControl*)malloc(sizeof(TGUIControl));
    if (!btn) return NULL;
    memset(btn, 0, sizeof(TGUIControl)); 

    btn->Type = TYPE_BUTTON; 
    btn->Left = x;
    btn->Top = y;
    btn->Width = w;         
    btn->Height = h;        
    btn->IsSelected = false;
    btn->OnClick = onClick; // Vincula o callback de clique passado pelo App

    GUI_RegisterControl(app, btn, "Button");

    btn->KernelHandle = (uint64_t)gui_create_button((TWinControl*)app->MainWindow, btn->Name, (char*)caption);
    if (btn->KernelHandle == 0) {
        free(btn);
        return NULL;
    }

    gui_set_prop((void*)btn->KernelHandle, PROP_LEFT,   (uint64_t)btn->Left);
    gui_set_prop((void*)btn->KernelHandle, PROP_TOP,    (uint64_t)btn->Top);
    gui_set_prop((void*)btn->KernelHandle, PROP_WIDTH,  (uint64_t)btn->Width);
    gui_set_prop((void*)btn->KernelHandle, PROP_HEIGHT, (uint64_t)btn->Height);

    return btn;
}
