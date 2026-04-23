#ifndef SYNTAX_H
#define SYNTAX_H

#include "cedit.h"

void syntax_init_colors(int syntax_theme_active, int popup_theme_active);
void syntax_set_theme(int syntax_theme_active, int popup_theme_active);
void syntax_draw_line(WINDOW *window, int row, const char *line, int col_offset,
                      int max_cols, int color_scheme_active);

#endif
