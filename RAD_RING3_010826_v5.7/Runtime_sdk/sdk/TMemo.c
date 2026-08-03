#include "libgui.h"

/* ============================================================================
 * CRIAÇÃO DO COMPONENTE MEMO (ATUALIZADO E ENCAPSULADO)
 * ============================================================================ */
TGUIControl* GUI_CreateMemo(TGUIEnvironment* app, int x, int y, int w, int h) {
    if (!app) return NULL;

    TGUIControl* memo = (TGUIControl*)malloc(sizeof(TGUIControl));
    if (!memo) return NULL;
    memset(memo, 0, sizeof(TGUIControl)); 

    memo->Type = TYPE_MEMO; 
    memo->Left = x;
    memo->Top = y;
    memo->Width = w;  
    memo->Height = h;  
    memo->IsSelected = false;
    
    // CORREÇÃO: Inicialização do Buffer Dinâmico Expandido para Logs Estáveis (Heap do Ring 3)
    memo->AllocatedSize = 2048; // Aumentado de 512 para 2048 para evitar buffer overflow precoce
    memo->Buffer = (char*)malloc(memo->AllocatedSize);
    if (!memo->Buffer) {
        free(memo);
        return NULL;
    }
    
    // Garante que o buffer comece limpo com o terminador nulo
    memo->Buffer[0] = '\0';
    memo->TextLength = 0;
    memo->ScrollY = 0;

    // Registro no ambiente do app
    GUI_RegisterControl(app, memo, "Memo");

    // Acoplamento com a engine gráfica do Kernel
    memo->KernelHandle = (uint64_t)gui_create_memo((TWinControl*)app->MainWindow, memo->Name);
    if (memo->KernelHandle == 0) {
        free(memo->Buffer);
        free(memo);
        return NULL;
    }

    // Configuração de propriedades iniciais
    gui_set_prop((void*)memo->KernelHandle, PROP_LEFT,    (uint64_t)memo->Left);
    gui_set_prop((void*)memo->KernelHandle, PROP_TOP,     (uint64_t)memo->Top);
    gui_set_prop((void*)memo->KernelHandle, PROP_WIDTH,   (uint64_t)memo->Width);
    gui_set_prop((void*)memo->KernelHandle, PROP_HEIGHT,  (uint64_t)memo->Height);
    gui_set_prop((void*)memo->KernelHandle, PROP_COLOR,   0xFFFFFF);
    
    // CORREÇÃO: Garante que a estrutura nativa no Kernel inicialize com o texto limpo sincronizado
    gui_set_prop((void*)memo->KernelHandle, PROP_CAPTION, (uintptr_t)memo->Buffer);

    return memo;
}

/* ============================================================================
 * OPERAÇÕES E MANIPULAÇÃO DE DADOS DO MEMO
 * ============================================================================ */

void GUI_Memo_AddChar(TGUIControl* memo, char key) {
    if (!memo || !memo->KernelHandle || !memo->Buffer) return;

    // 1. Tratamento de Backspace
    if (key == 8 || key == '\b') {
        if (memo->TextLength > 0) {
            memo->TextLength--;
            memo->Buffer[memo->TextLength] = '\0';
        }
    } 
    // 2. Tratamento de Caracteres Normais e Quebras de Linha
    else if ((key >= 32 && key <= 126) || key == '\n' || key == '\r') {
        char char_to_insert = (key == '\r') ? '\n' : key;

        // Redimensionamento dinâmico automático (Dobrar tamanho se estourar)
        if (memo->TextLength + 2 >= memo->AllocatedSize) {
            int new_size = memo->AllocatedSize * 2;
            char* new_buffer = (char*)realloc(memo->Buffer, new_size);
            
            if (new_buffer) {
                memo->Buffer = new_buffer;
                memo->AllocatedSize = new_size;
            } else {
                return; // Falha OOM (Out Of Memory) preventiva
            }
        }

        memo->Buffer[memo->TextLength] = char_to_insert;
        memo->TextLength++;
        memo->Buffer[memo->TextLength] = '\0';
    }

    // Atualiza a renderização gráfica no Kernel enviando o ponteiro de dados
    gui_set_prop((void*)memo->KernelHandle, PROP_CAPTION, (uintptr_t)memo->Buffer);
}

void GUI_Memo_AddStr(TGUIControl* memo, const char* str) {
    if (!memo || !str) return;

    while (*str != '\0') {
        GUI_Memo_AddChar(memo, *str);
        str++;
    }
}

void GUI_Memo_SetFocus(TGUIControl* memo) {
    if (!memo || !memo->KernelHandle) return;
    gui_set_prop((void*)memo->KernelHandle, PROP_SET_FOCUS, 1);
}

void GUI_Memo_SetScroll(TGUIControl* memo, int value) {
    if (!memo || !memo->KernelHandle) return;
    
    // Trava inferior do scroll
    if (value < 0) value = 0;

    memo->ScrollY = value;
    
    // CORREÇÃO: Substituído o '65' pela macro oficial PROP_SCROLL_Y
    // Isso garante que o gui.c no Kernel intercepte corretamente a propriedade.
    gui_set_prop((void*)memo->KernelHandle, PROP_SCROLL_Y, (uint64_t)memo->ScrollY);
}

void GUI_Memo_Clear(TGUIControl* memo) {
    if (!memo) return;

    if (memo->Buffer) {
        memo->Buffer[0] = '\0';
    }
    memo->TextLength = 0;
    memo->ScrollY = 0;

    if (memo->KernelHandle) {
        gui_set_prop((void*)memo->KernelHandle, PROP_CAPTION, (uintptr_t)"");
        gui_set_prop((void*)memo->KernelHandle, PROP_SCROLL_Y, 0);
    }
}
