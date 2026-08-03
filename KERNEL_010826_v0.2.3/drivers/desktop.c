#include "desktop.h"
//#include "../gui/wm.h" // Precisamos disso para chamar wm_handle_mouse

desktop_t ctx; 
volatile int mouse_needs_update = 0;
int refresh_screen = 0;

void desktop_init(int w, int h) {
    ctx.width = w;
    ctx.height = h;
    
    // Cores foram removidas, o Window Manager cuida do visual agora
    ctx.mouse.x = w / 2;
    ctx.mouse.y = h / 2;
    ctx.mouse.show = 1;
    ctx.mouse.buttons = 0;
    
    mouse_needs_update = 1;
}

void desktop_update_mouse(int delta_x, int delta_y, uint8_t buttons, int8_t scroll) {
    // 1. Atualiza fisicamente a coordenada usando a struct ctx
    ctx.mouse.x += delta_x;
    ctx.mouse.y += delta_y;

    // 2. Trava o mouse dentro da tela (Clamping)
    if (ctx.mouse.x < 0) ctx.mouse.x = 0;
    if (ctx.mouse.y < 0) ctx.mouse.y = 0;
    if (ctx.mouse.x >= ctx.width)  ctx.mouse.x = ctx.width - 1;
    if (ctx.mouse.y >= ctx.height) ctx.mouse.y = ctx.height - 1;

    // 3. Verifica se houve mudança nos botões para decidir o evento
    // Se o botão esquerdo (bit 0) acabou de ser pressionado
    if ((buttons & 1) && !(ctx.mouse.buttons & 1)) {
     //   wm_handle_mouse_event(ctx.mouse.x, ctx.mouse.y, EVENT_LEFT_DOWN);
    } 
    // Se o botão esquerdo foi solto
    else if (!(buttons & 1) && (ctx.mouse.buttons & 1)) {
       // wm_handle_mouse_event(ctx.mouse.x, ctx.mouse.y, EVENT_LEFT_UP);
    } 
    // Se foi apenas movimento
    else {
      //  wm_handle_mouse_event(ctx.mouse.x, ctx.mouse.y, EVENT_MOUSE_MOVE);
    }

    // 4. Salva o estado atual dos botões para a próxima comparação
    ctx.mouse.buttons = buttons;
    mouse_needs_update = 1; 
}
