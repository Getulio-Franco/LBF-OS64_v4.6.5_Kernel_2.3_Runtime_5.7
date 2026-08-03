#include "libgui.h"

/* ============================================================================
 * CRIAÇÃO DO COMPONENTE EDIT
 * ============================================================================ */
TGUIControl* GUI_CreateEdit(TGUIEnvironment* app, int x, int y, int w, int h, const char* initialText, TEventChange onChange) {
    if (!app) return NULL;

    TGUIControl* edit = (TGUIControl*)malloc(sizeof(TGUIControl));
    if (!edit) return NULL;
    memset(edit, 0, sizeof(TGUIControl)); 

    // Configuração de atributos padronizados
    edit->Type = TYPE_EDIT; 
    edit->Left = x;
    edit->Top = y;
    edit->Width = w;  
    edit->Height = h;  
    edit->MaxLength = 255; 
    edit->CursorPos = 0;
    edit->OnChange = onChange; // Associa o callback opcional do desenvolvedor

    if (initialText) {
        strncpy(edit->Text, initialText, sizeof(edit->Text) - 1);
        edit->CursorPos = strlen(edit->Text);
    } else {
        edit->Text[0] = '\0';
    }

    // Registra o componente no ecossistema do App
    GUI_RegisterControl(app, edit, "Edit");

    // Criação no Subsistema Gráfico do Kernel
    edit->KernelHandle = (uint64_t)gui_create_edit((TWinControl*)app->MainWindow, edit->Name);
    if (edit->KernelHandle == 0) {
        free(edit);
        return NULL;
    }

    // Sincronização via Syscalls de propriedades
    gui_set_prop((void*)edit->KernelHandle, PROP_LEFT,    (uint64_t)edit->Left);
    gui_set_prop((void*)edit->KernelHandle, PROP_TOP,     (uint64_t)edit->Top);
    gui_set_prop((void*)edit->KernelHandle, PROP_WIDTH,   (uint64_t)edit->Width);
    gui_set_prop((void*)edit->KernelHandle, PROP_HEIGHT,  (uint64_t)edit->Height);
    gui_set_prop((void*)edit->KernelHandle, PROP_COLOR,   0xFFFFFF);
    gui_set_prop((void*)edit->KernelHandle, PROP_CAPTION, (uintptr_t)edit->Text);

    return edit;
}

/* ============================================================================
 * MOTOR INTERNO DE DIGITAÇÃO E MÉTODOS DA SDK
 * ============================================================================ */
void GUI_Edit_AddChar(TGUIControl* edit, char key) {
    if (!edit || !edit->KernelHandle) return;

    int len = strlen(edit->Text);

    if (key == 8 || key == '\b') { // Backspace
        if (len > 0) {
            edit->Text[len - 1] = '\0';
            edit->CursorPos = len - 1;
        }
    } 
    else if (key >= 32 && key <= 126) { // Caracteres imprimíveis
        int limite = (edit->MaxLength > 0 && edit->MaxLength < 256) ? edit->MaxLength : 255;
        if (len < (limite - 1)) {
            edit->Text[len] = key;
            edit->Text[len + 1] = '\0';
            edit->CursorPos = len + 1;
        }
    }

    // Sincroniza o novo texto com a renderização no Ring 0
    gui_set_prop((void*)edit->KernelHandle, PROP_CAPTION, (uintptr_t)edit->Text);

    // 🔥 MÁGICA: Se o aplicativo registrou um evento de mudança de texto, avisa ele agora!
    if (edit->OnChange != NULL) {
        edit->OnChange(edit);
    }
}

char* GUI_Edit_GetText(TGUIControl* edit) {
    if (!edit || !edit->KernelHandle) return "";
    
    // Busca a string real que está associada ao Caption do objeto gráfico
    TWinControl* win = (TWinControl*)edit->KernelHandle;
    return (char*)win->Control.Caption;
}

void GUI_Edit_SetText(TGUIControl* edit, const char* text) {
    if (!edit || !edit->KernelHandle) return;
    
    if (text) {
        strncpy(edit->Text, text, sizeof(edit->Text) - 1);
        edit->CursorPos = strlen(edit->Text);
    } else {
        edit->Text[0] = '\0';
        edit->CursorPos = 0;
    }

    gui_set_prop((void*)edit->KernelHandle, PROP_CAPTION, (uintptr_t)edit->Text);
}

void GUI_Edit_SetFocus(TGUIControl* edit) {
    if (!edit || !edit->KernelHandle) return;
    gui_set_prop((void*)edit->KernelHandle, PROP_SET_FOCUS, 1);
}
