#ifndef GUI_INTERACTION_H
#define GUI_INTERACTION_H

#include "gui.h"

TForm* gui_find_form_at(int mx, int my);
bool gui_is_inside_control(int lx, int ly, TControl* ctrl);
void gui_process_hover(TForm* f, int mx, int my);
void gui_process_click(TForm* f, int mx, int my);
void gui_process_release(TForm* f, int mx, int my);
void gui_process_press(TForm* f, int mx, int my);
void gui_process_key(TForm* f, char key);
void gui_process_key(TForm* f, char key);

#endif
