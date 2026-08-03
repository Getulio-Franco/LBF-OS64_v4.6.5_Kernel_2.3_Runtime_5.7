#include "libgui.h"

/* ============================================================================
 * CRIAÇÃO E RENDERIZAÇÃO DO COMPONENTE TIMAGE
 * ============================================================================ */
TGUIControl* GUI_CreateImage(TGUIEnvironment* app, int x, int y, int w, int h, const char* path) {
    if (!app) return NULL;

    // 1. Alocação e limpeza do nó unificado da SDK
    TGUIControl* img = (TGUIControl*)malloc(sizeof(TGUIControl));
    if (!img) return NULL;
    memset(img, 0, sizeof(TGUIControl));

    // 2. Configuração inicial da geometria
    img->Type = TYPE_IMAGE; 
    img->Left = x;
    img->Top = y;
    img->Width = w;
    img->Height = h;
    img->IsSelected = false;

    // 3. Registro centralizado na SDK
    GUI_RegisterControl(app, img, "Image");

    // 4. Instanciação física no Subsistema de Janelas do Kernel
    img->KernelHandle = (uint64_t)gui_create_image((TWinControl*)app->MainWindow, img->Name);
    if (img->KernelHandle == 0) {
        free(img);
        return NULL;
    }

    // 5. Injeta o caminho do arquivo (Path) via PROP_CAPTION para o decodificador do Ring 0
    if (path && path[0] != '\0') {
        gui_set_prop((void*)img->KernelHandle, PROP_CAPTION, (uintptr_t)path);
    }

    // 6. Sincronização final de dimensões com o Kernel
    gui_set_prop((void*)img->KernelHandle, PROP_LEFT,   (uint64_t)img->Left);
    gui_set_prop((void*)img->KernelHandle, PROP_TOP,    (uint64_t)img->Top);
    gui_set_prop((void*)img->KernelHandle, PROP_WIDTH,  (uint64_t)img->Width);
    gui_set_prop((void*)img->KernelHandle, PROP_HEIGHT, (uint64_t)img->Height);

    return img;
}
