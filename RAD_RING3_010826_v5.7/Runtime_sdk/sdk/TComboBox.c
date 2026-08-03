#include "libgui.h"

/* ============================================================================
 * HELPER INTERNO DE FORMATAÇÃO DE DISPLAY
 * ============================================================================ */
static void GUI_ComboBox_UpdateKernelText(TGUIControl* combo) {
    if (!combo || !combo->KernelHandle || combo->ItemCount == 0) return;

    char buffer_formatado[32];
    memset(buffer_formatado, 0, sizeof(buffer_formatado));

    // Copia o texto do item atual (Ex: "False")
    strcpy(buffer_formatado, combo->Items[combo->ItemIndex]);
    strcat(buffer_formatado, " "); 

    // Converte e anexa o índice atual (Baseado em 1)
    int idx_atual = combo->ItemIndex + 1;
    char tmp_idx[2] = { '0' + (idx_atual % 10), '\0' };
    strcat(buffer_formatado, tmp_idx);

    // Separador interpretado pelo driver gráfico do Kernel
    strcat(buffer_formatado, "-");

    // Converte e anexa o total de itens
    char tmp_total[2] = { '0' + (combo->ItemCount % 10), '\0' };
    strcat(buffer_formatado, tmp_total);

    // Envia a string final montada para a propriedade de Caption do Kernel
    gui_set_prop((void*)combo->KernelHandle, PROP_CAPTION, (uintptr_t)buffer_formatado);
}

/* ============================================================================
 * CRIAÇÃO E MÉTODOS PÚBLICOS
 * ============================================================================ */
TGUIControl* GUI_CreateComboBox(TGUIEnvironment* app, int x, int y, int w, int h, TEventChange onChange) {
    if (!app) return NULL;

    TGUIControl* combo = (TGUIControl*)malloc(sizeof(TGUIControl));
    if (!combo) return NULL;
    memset(combo, 0, sizeof(TGUIControl));

    combo->Type = TYPE_COMBOBOX; 
    combo->Left = x;
    combo->Top = y;
    combo->Width = w;
    combo->Height = h;
    combo->ItemCount = 0;
    combo->ItemIndex = 0;
    combo->DroppedDown = false;
    combo->OnChange = onChange; // Callback opcional para o app do usuário

    // Registro unificado na SDK
    GUI_RegisterControl(app, combo, "ComboBox");

    // Instanciação física no Subsistema de Janelas do Kernel
    combo->KernelHandle = (uint64_t)gui_create_combobox((TWinControl*)app->MainWindow, combo->Name);
    if (combo->KernelHandle == 0) {
        free(combo);
        return NULL;
    }

    // Sincronização inicial de dimensões
    gui_set_prop((void*)combo->KernelHandle, PROP_LEFT,   (uint64_t)combo->Left);
    gui_set_prop((void*)combo->KernelHandle, PROP_TOP,    (uint64_t)combo->Top);
    gui_set_prop((void*)combo->KernelHandle, PROP_WIDTH,  (uint64_t)combo->Width);
    gui_set_prop((void*)combo->KernelHandle, PROP_HEIGHT, (uint64_t)combo->Height);

    return combo;
}

void GUI_ComboBox_AddItem(TGUIControl* combo, const char* texto) {
    if (!combo || !texto || combo->ItemCount >= 8) return;

    strncpy(combo->Items[combo->ItemCount], texto, 15);
    combo->Items[combo->ItemCount][15] = '\0';

    combo->ItemCount++;

    if (combo->ItemCount == 1) {
        combo->ItemIndex = 0;
        GUI_ComboBox_UpdateKernelText(combo);
    }
}

void GUI_ComboBox_Rotate(TGUIControl* combo) {
    if (!combo || combo->ItemCount == 0) return;

    // Avança o carrossel circularmente
    combo->ItemIndex = (combo->ItemIndex + 1) % combo->ItemCount;

    // Atualiza a visualização gráfica no Ring 0
    GUI_ComboBox_UpdateKernelText(combo);

    // 🔥 Notifica o aplicativo de que a seleção do ComboBox mudou!
    if (combo->OnChange != NULL) {
        combo->OnChange(combo);
    }
}

char* GUI_ComboBox_GetText(TGUIControl* combo) {
    if (!combo || !combo->KernelHandle) return "";
    
    // Retorna a string local segura selecionada no Ring 3
    if (combo->ItemCount > 0 && combo->ItemIndex >= 0) {
        return combo->Items[combo->ItemIndex];
    }
    return "";
}
