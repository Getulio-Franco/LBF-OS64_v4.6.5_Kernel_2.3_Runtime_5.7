#include "mouse_geometry.h"

void geometry_apply_limits(int *x, int *y, int screen_w, int screen_h) {
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (*x >= screen_w - 1) *x = screen_w - 1;
    if (*y >= screen_h - 1) *y = screen_h - 1;
}
