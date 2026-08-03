#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include <stdbool.h>

// Identificadores de tipos de componentes
#define TYPE_CONTROL     0
#define TYPE_BUTTON      1
#define TYPE_EDIT        2
#define TYPE_PANEL       3
#define TYPE_CHECKBOX    4
#define TYPE_RADIOBUTTON 5
#define TYPE_MEMO        6
#define TYPE_LABEL       7
#define TYPE_COMBOBOX    8
#define TYPE_IMAGE       9
#define TYPE_FORM        10
#define TYPE_SCROLLBAR   11
#define TYPE_LISTVIEW    12
#define TYPE_TREEVIEW    13

// Enums para propriedades genéricas usadas pelo Designer da VCL
typedef enum {
    PROP_LEFT,         // 0
    PROP_TOP,          // 1
    PROP_WIDTH,        // 2
    PROP_HEIGHT,       // 3
    PROP_VISIBLE,      // 4
    PROP_CAPTION,      // 5
    PROP_COLOR,        // 6
    PROP_ADD_ITEM,     // 7
    PROP_SET_FOCUS,    // 8
    PROP_STATE,        // 9  <- Centralizado aqui o efeito de afundar/levantar botões!
    PROP_SCROLL_Y,      // 10 <- Substitui o número mágico 65, padronizando o TMemo!
    PROP_ITEM_INDEX    // 11
} TGUIProperty;

// Definições de Eventos do Sistema Operacional
typedef enum {
    EVENT_NONE,
    EVENT_FOCUS_GAINED,
    EVENT_FOCUS_LOST,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_UP,
    EVENT_KEY_PRESS
} TEventType;

typedef struct {
    TEventType type;
    uint64_t form_handle; // ID ou ponteiro da janela afetada
    int mouseX;
    int mouseY;
    char key;
} TEvent;

/* --- ENUMS E TIPOS DE SUPORTE --- */
typedef enum { bsNone, bsSingle, bsSizeable, bsDialog } TBorderStyle;
typedef enum { wsNormal, wsMinimized, wsMaximized } TWindowState;

typedef struct {
    uint32_t Color;
    int Size;
    char Name[32];
} TFont;

/* --- 1. BASE: TObject & TComponent --- */
typedef struct { uint32_t InstanceSize; } TObject;

typedef struct {
    TObject Object;        
    void* Owner;           
} TComponent;

/* --- 2. DNA VISUAL: TControl --- */
struct TWinControl;

typedef struct TControl {
    TComponent Component;  
    char Name[32];
    char Caption[64];
    int Type;
    int Left, Top, Width, Height;
    bool Enabled;
    bool Visible;
    int State;  // 0=Normal, 1=Hover, 2=Pressed
    TFont Font;
    uint32_t Color;
    struct TWinControl* Parent; 
    bool IsSelected;
    
    // Eventos (Ponteiros de Função)
    void (*OnPaint)(void* sender);
    void (*OnClick)(void* sender);
    void (*OnMouseMove)(void* sender, int x, int y);
} TControl;

typedef struct { TControl Control; } TGraphicControl;

typedef struct {
    TGraphicControl Graphic; 
    uint32_t FontColor;
    int FontSize;
    bool Transparent;
} TLabel;

typedef struct {
    TGraphicControl Graphic;
    void* BitmapData;      
    bool Stretch;          
} TImage;

/* --- 3. RAMIFICAÇÃO: TWinControl --- */
typedef struct TWinControl {
    TControl Control;      
    int TabOrder;          
    bool Focused;  
    bool Draggable;
    void (*OnEnter)(void* sender);
    void (*OnExit)(void* sender);
    void (*OnKeyPress)(void* sender, char key);
} TWinControl;

// --- COMPONENTES DERIVADOS ---

typedef struct {
    TWinControl Win;       
    TBorderStyle BorderStyle;
    TWindowState WindowState; 
    int Layer;                 
    void* Icon;            
    int OwnerPID;              
    bool IsDragging;           
    int DragOffsetX;           
    int DragOffsetY;           
    struct TControl* Controls[100]; 
    int ControlCount;
    bool ActiveFocus;
} TForm;

typedef struct {
    TWinControl Win;
    char* Caption;
} TButton;

typedef struct {
    TWinControl Win;
    char* Text;        
    int CursorPos;
    int MaxLength;
} TEdit;

typedef struct {
    TWinControl Win;
    char* TextPointer; // <-- ADICIONE ESSE PONTEIRO NA ESTRUTURA DO KERNEL!
    char** Lines;      
    int LineCount;     
    int MaxLength;
    int ScrollY;
} TMemo;

typedef struct { TWinControl Win; bool Checked; } TCheckBox;
typedef struct { TWinControl Win; bool Checked; int GroupIndex; } TRadioButton;

typedef struct {
    TWinControl Win;
    char Items[8][16];     // Matriz estática segura (8 itens de 16 caracteres)
    int ItemCount;         // Quantidade de itens adicionados (Ex: 6)
    int ItemIndex;         // Qual está selecionado (Ex: 0 para o primeiro)
    bool DroppedDown;      // Fica sempre 'false' na roleta, guardado para o futuro
} TComboBox;

typedef struct { TWinControl Win; int BevelWidth; } TPanel;

// --- ESTRUTURAS DEDICADAS ---

typedef enum { sbVertical, sbHorizontal } TScrollOrientation;

typedef struct {
    TWinControl Win;            // Herança da VCL
    int Min;
    int Max;
    int Position;
    int PageSize;
    TScrollOrientation Orientation;
} TScrollBar;

// Estrutura leve para o Kernel saber o que desenhar em cada linha
typedef struct {
    char Name[64];
    uint32_t Size;
    uint8_t Attributes;
    int IconIndex;
} TListViewItem; // Garanta que o nome e tamanho batam com o os_listview.h

typedef struct {
    TWinControl Win;            // Herança da VCL
    TListViewItem* Items;       // CORRIGIDO: Agora aponta exatamente para a struct de 64 bytes de nome!
    int ItemCount;              // Quantidade de itens
    int ItemIndex;              // Item selecionado atualmente
    int ScrollY;                // Scroll atual da lista
} TListView;

/* --- PROTÓTIPOS --- */
void gui_get_abs_pos(TControl* ctrl, int* x, int* y);
void gui_render_control(TControl* ctrl);
void draw_sunken_border(int x, int y, int w, int h);
void gui_draw_raised_border(int x, int y, int w, int h);
void draw_selection_handles(int x, int y, int w, int h);

// Ponte de propriedades dinâmica para a VCL do Designer usar
void gui_set_prop(void* control_ptr, TGUIProperty prop, uint64_t value);
void gui_set_prop_string(void* control_ptr, TGUIProperty prop, uint64_t str_ptr, int extra_val);
void gui_render(uint64_t form_ptr);

TForm* gui_create_form(char* name, char* caption, int pid);
TButton* gui_create_button(TWinControl* parent, char* name, char* caption);
TEdit* gui_create_edit(TWinControl* parent, char* name);
TCheckBox* gui_create_checkbox(TWinControl* parent, char* name, char* caption);
TRadioButton* gui_create_radiobutton(TWinControl* parent, char* name, char* caption);
TComboBox* gui_create_combobox(TWinControl* parent, char* name);
TLabel* gui_create_label(TWinControl* parent, char* name, char* caption);
TPanel* gui_create_panel(TWinControl* parent, char* name);
TMemo* gui_create_memo(TWinControl* parent, char* name);
TImage* gui_create_image(TWinControl* parent, char* name);

void gui_draw_form(TForm* form);
void gui_draw_button(TButton* btn);
void gui_draw_edit(TEdit* edit);
void gui_draw_memo(TMemo* memo);
void gui_draw_checkbox(TCheckBox* cb);
void gui_draw_radiobutton(TRadioButton* rb);
void gui_draw_combobox(TComboBox* combo);
void gui_draw_panel(TPanel* panel);
void gui_draw_label(TLabel* label);
void gui_draw_image(TImage* img);

void gui_set_border_style(TForm* form, TBorderStyle style);

uint64_t gui_pop_event(void);
void gui_push_event(uint64_t event_id);

extern void gui_process_hover(TForm* form, int x, int y);
extern void gui_process_release(TForm* form, int x, int y);
extern void gui_process_press(TForm* form, int x, int y);
extern void gui_process_key(TForm* form, char key);

void gui_add_to_parent(TWinControl* parent, TControl* child);
void gui_set_prop(void* control_ptr, TGUIProperty prop, uint64_t value);


TScrollBar* gui_create_scrollbar(TWinControl* parent, char* name, TScrollOrientation orientation);
void gui_draw_scrollbar(TScrollBar* sb);

TListView* gui_create_listview(TWinControl* parent, char* name);
void gui_draw_listview(TListView* lv);

int64_t gui_get_prop(void* control_ptr, TGUIProperty prop);

#endif // GUI_H
