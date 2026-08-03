#include "libgui.h"

/* ============================================================================
 * EVENTO INTERNO: Sincronização do Scrollbar com o componente no Kernel
 * ============================================================================ */
static void OnVScrollChange(void* sender, int new_position) {
    TGUIControl* sb = (TGUIControl*)sender;
    if (!sb || !sb->Owner) return;
    
    TGUIControl* lv = (TGUIControl*)sb->Owner;
    gui_set_prop((void*)lv->KernelHandle, PROP_SCROLL_Y, (uint64_t)new_position);
}

/* ============================================================================
 * FUNÇÃO: GUI_CreateListView
 * Inicializa o controle, aloca o array dinâmico e vincula o callback de evento
 * ============================================================================ */
TGUIControl* GUI_CreateListView(TGUIEnvironment* app, int x, int y, int w, int h, TNotifyEvent onChange) {
    if (!app) return NULL;

    TGUIControl* lv = (TGUIControl*)malloc(sizeof(TGUIControl));
    if (!lv) return NULL;
    memset(lv, 0, sizeof(TGUIControl));

    lv->Type = TYPE_LISTVIEW;
    lv->Left = x;
    lv->Top = y;
    lv->Width = w;
    lv->Height = h;
    lv->LVItemIndex = -1;       
    lv->LVAllocatedItems = 32;  
    lv->LVItemCount = 0;        
    
    // Vincula o callback RAD para mudanças de seleção/cliques
    lv->OnChange = onChange; 

    lv->LVItems = (TListViewItem*)malloc(lv->LVAllocatedItems * sizeof(TListViewItem)); 
    if (!lv->LVItems) {
        free(lv);
        return NULL;
    }

    GUI_RegisterControl(app, lv, "ListView");

    lv->KernelHandle = (uint64_t)gui_create_listview((TWinControl*)app->MainWindow, lv->Name);
    if (lv->KernelHandle == 0) {
        free(lv->LVItems);
        free(lv);
        return NULL;
    }

    int sb_width = 16;
    lv->VScrollBar = GUI_CreateScrollBar(app, x + w - sb_width - 2, y + 2, sb_width, h - 4, 1);
    if (lv->VScrollBar) {
        lv->VScrollBar->Owner = lv;
        lv->VScrollBar->OnScroll = OnVScrollChange;
        GUI_ScrollBar_SetRange(lv->VScrollBar, 0, 0, (h - 4) / 18);
    }

    gui_set_prop((void*)lv->KernelHandle, PROP_LEFT,   (uint64_t)lv->Left);
    gui_set_prop((void*)lv->KernelHandle, PROP_TOP,    (uint64_t)lv->Top);
    gui_set_prop((void*)lv->KernelHandle, PROP_WIDTH,  (uint64_t)lv->Width - sb_width);
    gui_set_prop((void*)lv->KernelHandle, PROP_HEIGHT, (uint64_t)lv->Height);
    
    gui_set_prop((void*)lv->KernelHandle, PROP_CAPTION, (uintptr_t)lv->LVItems); 

    return lv;
}

/* ============================================================================
 * FUNÇÃO: GUI_ListView_AddItem
 * ============================================================================ */
void GUI_ListView_AddItem(TGUIControl* lv, const char* name, uint32_t size, uint8_t attributes) {
    if (!lv || !name) return;

    if (lv->LVItemCount >= lv->LVAllocatedItems) { 
        lv->LVAllocatedItems *= 2;                  
        TListViewItem* new_array = (TListViewItem*)realloc(lv->LVItems, lv->LVAllocatedItems * sizeof(TListViewItem)); 
        if (!new_array) return;
        
        lv->LVItems = new_array;
        gui_set_prop((void*)lv->KernelHandle, PROP_CAPTION, (uintptr_t)lv->LVItems); 
    }

    TListViewItem* item = &lv->LVItems[lv->LVItemCount]; 
    strncpy(item->Name, name, 63);
    item->Name[63] = '\0';
    item->Size = size;
    item->Attributes = attributes;
    item->IconIndex = (attributes & 0x10) ? 0 : 1; 

    lv->LVItemCount++; 

    gui_set_prop((void*)lv->KernelHandle, PROP_STATE, (uint64_t)lv->LVItemCount); 

    int max_visible_lines = (lv->Height - 4) / 18;
    if (lv->VScrollBar) {
        int max_scroll_val = (lv->LVItemCount > max_visible_lines) ? (lv->LVItemCount - max_visible_lines) : 0; 
        GUI_ScrollBar_SetRange(lv->VScrollBar, 0, max_scroll_val, max_visible_lines);
    }
}

/* ============================================================================
 * FUNÇÃO: GUI_ListView_Clear
 * ============================================================================ */
void GUI_ListView_Clear(TGUIControl* lv) {
    if (!lv) return;
    
    lv->LVItemCount = 0;   
    lv->LVItemIndex = -1;  
    
    gui_set_prop((void*)lv->KernelHandle, PROP_STATE, 0);      
    gui_set_prop((void*)lv->KernelHandle, PROP_ITEM_INDEX, -1); 
    
    if (lv->VScrollBar) {
        GUI_ScrollBar_SetPosition(lv->VScrollBar, 0);
        GUI_ScrollBar_SetRange(lv->VScrollBar, 0, 0, (lv->Height - 4) / 18);
    }
}

/* ============================================================================
 * FUNÇÃO: GUI_ListView_GetItem
 * Retorna os metadados de forma segura baseando-se no índice solicitado
 * ============================================================================ */
void GUI_ListView_GetItem(TGUIControl* lv, int index, char* out_name, uint32_t* out_size, uint8_t* out_attr) {
    if (!lv || !lv->LVItems || index < 0 || index >= lv->LVItemCount) return;

    TListViewItem* item = &lv->LVItems[index];
    if (out_name) {
        strcpy(out_name, item->Name);
    }
    if (out_size) {
        *out_size = item->Size;
    }
    if (out_attr) {
        *out_attr = item->Attributes;
    }
}
