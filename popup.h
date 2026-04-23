#ifndef POPUP_H
#define POPUP_H

#include "cedit.h"

WINDOW *popup_create_shadow(int height, int width, int start_y, int start_x);
WINDOW *popup_create_window(int height, int width, int start_y, int start_x);
void popup_draw_box(WINDOW *window);
void popup_draw_button(WINDOW *window, int row, int col,
                       const char *label, int focused);
void popup_destroy(WINDOW *window, WINDOW *shadow);

#endif
