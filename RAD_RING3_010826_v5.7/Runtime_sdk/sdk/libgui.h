#ifndef LIBGUI_H
#define LIBGUI_H

#include <stdint.h>
#include <stdbool.h>
#include "../system/liblib.h"
#include "../system/malloc.h"
#include "../system/string.h"
#include "../gui/gui.h" // Definições do Kernel (Já traz TYPE_LISTVIEW, TYPE_SCROLLBAR e a struct TListViewItem!)

#define MAX_APP_CONTROLS 64

/* ============================================================================
 * ESTRUTURAS DE DADOS DA UI (User Space SDK)
 * ============================================================================ */

typedef void (*TEventClick)(void* sender);
typedef void (*TEventHover)(void* sender);
typedef void (*TEventChange)(void* sender); 
typedef void (*TEventScroll)(void* sender, int new_position);
typedef void (*TEventItemClick)(void* sender, int item_index);
typedef void (*TNotifyEvent)(void* sender);

// Estrutura Base unificada para qualquer componente/janela na SDK
typedef struct TGUIControl {
    int Type;
    int Left;
    int Top;
    int Width;
    int Height;
    uint64_t KernelHandle; 
    bool IsSelected;       
    char Name[32];
    struct TGUIControl* Parent;
    void* Owner;           // Vínculo de controle para componentes compostos
    
    // Extensões de texto/Memo
    char* Buffer;          
    char Text[256];        
    int CursorPos;
    int MaxLength;
    int AllocatedSize;     
    int TextLength;        
    int ScrollY;           
    
    // O COMBOBOX (Igual ao seu TComboBox.c original):
    char Items[8][16];     
    int ItemCount;         
    int ItemIndex;         
    bool DroppedDown;      
    
    // Extensões para o TCheckBox / TRadioButton
    bool Checked;          
    int GroupIndex;        
    
    // Extensões para Sub-Formulários (TForm) / Painéis
    bool Modal;            
    int BorderStyle;       
    int BevelWidth;        

    // Extensões específicas para TScrollBar
    int Min;
    int Max;
    int Position;
    int PageSize;
    int Orientation;       
    TEventScroll OnScroll; 

    // LISTVIEW 
    TListViewItem* LVItems;     // Antes era Items
    int LVItemCount;            // Antes era ItemCount
    int LVAllocatedItems;       // Antes era AllocatedItems
    int LVItemIndex;            // Antes era ItemIndex
    struct TGUIControl* VScrollBar; 
    
    // Lista de Eventos nativos
    TEventClick     OnClick; 
    TEventHover     OnEnter;     
    TEventHover     OnLeave;     
    TEventChange    OnChange;   
    TEventItemClick OnItemClick; 
} TGUIControl;

typedef struct {
    int SlotID;
    TForm* MainWindow;
    TGUIControl* Controls[MAX_APP_CONTROLS];
    int ControlCount;
    TGUIControl* ActiveFocus; 
} TGUIEnvironment;

/* ============================================================================
 * GERENCIAMENTO DO MOTOR CENTRAL
 * ============================================================================ */
void GUI_InitApplication(TGUIEnvironment* app, int slot_id, const char* title, int w, int h);
void GUI_RegisterControl(TGUIEnvironment* app, TGUIControl* ctrl, const char* prefix);
bool GUI_ProcessMouseClick(TGUIEnvironment* app, int mouse_x, int mouse_y);
void GUI_ProcessKeyboard(TGUIEnvironment* app, char key);

// Componentes: ScrollBar
TGUIControl* GUI_CreateScrollBar(TGUIEnvironment* app, int x, int y, int w, int h, int orientation);
void         GUI_ScrollBar_SetPosition(TGUIControl* sb, int position);
void         GUI_ScrollBar_SetRange(TGUIControl* sb, int min, int max, int pagesize);

// Componentes: ListView
//TGUIControl* GUI_CreateListView(TGUIEnvironment* app, int x, int y, int w, int h);
//void         GUI_ListView_AddItem(TGUIControl* lv, const char* name, uint32_t size, uint8_t attributes);
//void         GUI_ListView_Clear(TGUIControl* lv);

TGUIControl* GUI_CreateListView(TGUIEnvironment* app, int x, int y, int w, int h, TNotifyEvent onChange);
void GUI_ListView_AddItem(TGUIControl* lv, const char* name, uint32_t size, uint8_t attributes);
void GUI_ListView_Clear(TGUIControl* lv);
void GUI_ListView_GetItem(TGUIControl* lv, int index, char* out_name, uint32_t* out_size, uint8_t* out_attr);

/* ============================================================================
 * COMPONENTES DISPONÍVEIS NA SDK
 * ============================================================================ */
TGUIControl* GUI_CreateButton(TGUIEnvironment* app, int x, int y, int w, int h, const char* caption, TEventClick onClick);
TGUIControl* GUI_CreateLabel(TGUIEnvironment* app, int x, int y, const char* caption);
TGUIControl* GUI_CreateEdit(TGUIEnvironment* app, int x, int y, int w, int h, const char* initialText, TEventChange onChange);
TGUIControl* GUI_CreateMemo(TGUIEnvironment* app, int x, int y, int w, int h);
TGUIControl* GUI_CreateComboBox(TGUIEnvironment* app, int x, int y, int w, int h, TEventChange onChange); 
TGUIControl* GUI_CreateCheckBox(TGUIEnvironment* app, int x, int y, const char* caption, TEventChange onChange);
TGUIControl* GUI_CreateRadioButton(TGUIEnvironment* app, int x, int y, const char* caption, TEventChange onChange);
TGUIControl* GUI_CreateImage(TGUIEnvironment* app, int x, int y, int w, int h, const char* path);
TGUIControl* GUI_CreateForm(const char* title, int x, int y, int width, int height);  
TGUIControl* GUI_CreatePanel(TGUIEnvironment* app, int x, int y, int w, int h);
                                                 
/* ============================================================================
 * MÉTODOS AUXILIARES (GETTERS / SETTERS / UTILS)
 * ============================================================================ */
char* GUI_Edit_GetText(TGUIControl* edit);
void  GUI_Edit_SetText(TGUIControl* edit, const char* text);
void  GUI_Edit_SetFocus(TGUIControl* edit);
void  GUI_Edit_AddChar(TGUIControl* edit, char key);

void  GUI_Memo_AddChar(TGUIControl* memo, char key);
void  GUI_Memo_AddStr(TGUIControl* memo, const char* str);
void  GUI_Memo_SetFocus(TGUIControl* memo);
void  GUI_Memo_SetScroll(TGUIControl* memo, int value);
void  GUI_Memo_Clear(TGUIControl* memo);

// Métodos utilitários do TComboBox
void  GUI_ComboBox_AddItem(TGUIControl* combo, const char* texto);
void  GUI_ComboBox_Rotate(TGUIControl* combo);
char* GUI_ComboBox_GetText(TGUIControl* combo);

// Métodos utilitários do TCheckBox
void GUI_CheckBox_Toggle(TGUIControl* cb);

// Métodos utilitários do TRadioButton
void GUI_RadioButton_Select(TGUIEnvironment* app, TGUIControl* target_rb);

void GUI_DestroyForm(TGUIControl* form);    

void GUI_SetParent(TGUIControl* control, TGUIControl* new_parent);

#endif // LIBGUI_H
