#include "popup.h"

WINDOW *popup_create_shadow(int height, int width, int start_y, int start_x){
  WINDOW *shadow;

  if (!has_colors()){
    return NULL;
  }
  if (start_y + 1 + height > LINES || start_x + 2 + width > COLS){
    return NULL;
  }

  shadow = newwin(height, width, start_y + 1, start_x + 2);
  if (shadow == NULL){
    return NULL;
  }

  wbkgd(shadow, COLOR_PAIR(CEDIT_COLOR_SHADOW) | A_DIM);
  werase(shadow);
  wrefresh(shadow);
  return shadow;
}

WINDOW *popup_create_window(int height, int width, int start_y, int start_x){
  WINDOW *window;

  window = newwin(height, width, start_y, start_x);
  if (window != NULL && has_colors()){
    wbkgd(window, COLOR_PAIR(CEDIT_COLOR_POPUP));
  }
  return window;
}

void popup_draw_box(WINDOW *window){
  if (window == NULL){
    return;
  }
  if (has_colors()){
    wattron(window, COLOR_PAIR(CEDIT_COLOR_POPUP) | A_BOLD);
  }
  box(window, 0, 0);
  if (has_colors()){
    wattroff(window, COLOR_PAIR(CEDIT_COLOR_POPUP) | A_BOLD);
    wattron(window, COLOR_PAIR(CEDIT_COLOR_POPUP));
  }
}

void popup_draw_button(WINDOW *window, int row, int col,
                       const char *label, int focused){
  if (window == NULL){
    return;
  }
  if (focused){
    wattron(window, A_REVERSE | A_BOLD);
  }
  mvwaddstr(window, row, col, label);
  if (focused){
    wattroff(window, A_REVERSE | A_BOLD);
  }
}

void popup_destroy(WINDOW *window, WINDOW *shadow){
  if (window != NULL){
    delwin(window);
  }
  if (shadow != NULL){
    delwin(shadow);
  }
}
