#ifndef CURSOR_ENGINE_H
#define CURSOR_ENGINE_H

void cursor_restore_bg(int mx, int my, int screen_w, int screen_h);
void cursor_save_bg(int mx, int my, int screen_w, int screen_h);
void cursor_draw(int mx, int my, int screen_w, int screen_h);

#endif
