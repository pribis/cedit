#include "syntax.h"

static const char *keywords[] = {
  "auto", "break", "case", "char", "const", "continue", "default", "do",
  "double", "else", "enum", "extern", "float", "for", "goto", "if",
  "inline", "int", "long", "register", "restrict", "return", "short",
  "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
  "unsigned", "void", "volatile", "while", "class", "namespace", "template",
  "typename", "public", "private", "protected", "virtual", "using", "constexpr",
  "nullptr", "operator", "friend", "this", "new", "delete", "try", "catch",
  "throw", "noexcept", "bool", "true", "false", NULL
};

static int syntax_is_keyword(const char *text, size_t length){
  int index;

  for (index = 0; keywords[index] != NULL; index++){
      if (strlen(keywords[index]) == length
          && strncmp(keywords[index], text, length) == 0){
          return 1;
        }
    }

  return 0;
}

static int syntax_is_function_name(const char *line, int start, int end){
  int index;

  if (syntax_is_keyword(line + start, (size_t) (end - start))){
      return 0;
    }

  index = end;
  while (line[index] != '\0' && isspace((unsigned char) line[index])){
      index++;
    }

  return line[index] == '(';
}

static int syntax_is_number_start(const char *line, int index){
  if (!isdigit((unsigned char) line[index])){
      return 0;
    }

  if (index == 0){
      return 1;
    }

  return !isalnum((unsigned char) line[index - 1]) && line[index - 1] != '_';
}

static int syntax_is_preprocessor_line(const char *line, int index){
  int scan;

  if (line[index] != '#'){
      return 0;
    }

  for (scan = 0; scan < index; scan++){
      if (!isspace((unsigned char) line[scan])){
          return 0;
        }
    }

  return 1;
}

static int syntax_prev_nonspace(const char *line, int index){
  index--;
  while (index >= 0 && isspace((unsigned char) line[index])){
      index--;
    }
  return index;
}

static int syntax_next_nonspace(const char *line, int index){
  while (line[index] != '\0' && isspace((unsigned char) line[index])){
      index++;
    }
  return index;
}

static int syntax_is_upper_identifier(const char *line, int start, int end){
  int index;
  int saw_alpha;

  saw_alpha = 0;
  for (index = start; index < end; index++){
      if (isalpha((unsigned char) line[index])){
          saw_alpha = 1;
          if (!isupper((unsigned char) line[index])){
              return 0;
            }
        }
      else if (!isdigit((unsigned char) line[index]) && line[index] != '_'){
          return 0;
        }
    }

  return saw_alpha;
}

static int syntax_is_entity_name(const char *line, int start, int end){
  int prev;
  int next;

  if (isupper((unsigned char) line[start])){
      return 1;
    }

  prev = syntax_prev_nonspace(line, start);
  if (prev >= 1 && line[prev] == ':' && line[prev - 1] == ':'){
      return 1;
    }

  next = syntax_next_nonspace(line, end);
  return line[next] == ':' && line[next + 1] == ':';
}

static void syntax_set_pair(short pair, short fg, short bg){
  init_pair(pair, fg, bg);
}

static void syntax_apply_theme(int syntax_theme_active, int popup_theme_active){
  if (!has_colors()){
      return;
    }

  if (!syntax_theme_active){
      syntax_set_pair(CEDIT_COLOR_MAIN, -1, -1);
      syntax_set_pair(CEDIT_COLOR_COMMENT, COLOR_BLUE, -1);
      syntax_set_pair(CEDIT_COLOR_BORDER, COLOR_WHITE, -1);
      syntax_set_pair(CEDIT_COLOR_KEYWORD, COLOR_CYAN, -1);
      syntax_set_pair(CEDIT_COLOR_STRING, COLOR_YELLOW, -1);
      syntax_set_pair(CEDIT_COLOR_NUMBER, COLOR_MAGENTA, -1);
      syntax_set_pair(CEDIT_COLOR_PREPROC, COLOR_GREEN, -1);
      syntax_set_pair(CEDIT_COLOR_FUNCTION, COLOR_WHITE, -1);
      syntax_set_pair(CEDIT_COLOR_ENTITY, COLOR_MAGENTA, -1);
      syntax_set_pair(CEDIT_COLOR_VARIABLE, COLOR_YELLOW, -1);
      syntax_set_pair(CEDIT_COLOR_CONSTANT, COLOR_CYAN, -1);
    }
  else if (COLORS >= 256){
      syntax_set_pair(CEDIT_COLOR_MAIN, 252, 233);
      syntax_set_pair(CEDIT_COLOR_COMMENT, 247, 233);
      syntax_set_pair(CEDIT_COLOR_BORDER, 247, 233);
      syntax_set_pair(CEDIT_COLOR_KEYWORD, 167, 233);
      syntax_set_pair(CEDIT_COLOR_STRING, 111, 233);
      syntax_set_pair(CEDIT_COLOR_NUMBER, 189, 233);
      syntax_set_pair(CEDIT_COLOR_PREPROC, 26, 233);
      syntax_set_pair(CEDIT_COLOR_FUNCTION, 141, 233);
      syntax_set_pair(CEDIT_COLOR_ENTITY, 141, 233);
      syntax_set_pair(CEDIT_COLOR_VARIABLE, 209, 233);
      syntax_set_pair(CEDIT_COLOR_CONSTANT, 189, 233);
    }
  else{
    syntax_set_pair(CEDIT_COLOR_MAIN, COLOR_WHITE, COLOR_BLACK);
    syntax_set_pair(CEDIT_COLOR_COMMENT, COLOR_CYAN, COLOR_BLACK);
    syntax_set_pair(CEDIT_COLOR_BORDER, COLOR_WHITE, COLOR_BLACK);
    syntax_set_pair(CEDIT_COLOR_KEYWORD, COLOR_RED, COLOR_BLACK);
    syntax_set_pair(CEDIT_COLOR_STRING, COLOR_BLUE, COLOR_BLACK);
    syntax_set_pair(CEDIT_COLOR_NUMBER, COLOR_CYAN, COLOR_BLACK);
    syntax_set_pair(CEDIT_COLOR_PREPROC, COLOR_BLUE, COLOR_BLACK);
    syntax_set_pair(CEDIT_COLOR_FUNCTION, COLOR_MAGENTA, COLOR_BLACK);
    syntax_set_pair(CEDIT_COLOR_ENTITY, COLOR_MAGENTA, COLOR_BLACK);
    syntax_set_pair(CEDIT_COLOR_VARIABLE, COLOR_YELLOW, COLOR_BLACK);
    syntax_set_pair(CEDIT_COLOR_CONSTANT, COLOR_CYAN, COLOR_BLACK);
  }

  if (!popup_theme_active){
    syntax_set_pair(CEDIT_COLOR_POPUP, COLOR_YELLOW, COLOR_BLUE);
    syntax_set_pair(CEDIT_COLOR_SHADOW, COLOR_BLACK, COLOR_BLACK);
  }
  else if (COLORS >= 256){
    syntax_set_pair(CEDIT_COLOR_POPUP, 252, 238);
    syntax_set_pair(CEDIT_COLOR_SHADOW, 232, 232);
  }
  else{
    syntax_set_pair(CEDIT_COLOR_POPUP, COLOR_WHITE, COLOR_BLUE);
    syntax_set_pair(CEDIT_COLOR_SHADOW, COLOR_BLACK, COLOR_BLACK);
  }
}

static void syntax_addch(WINDOW *window, int screen_row, int *screen_col, int col_offset,
             int max_cols, int ch, int attrs){
  int count;
  int spaces;

  if (ch == '\t'){
      spaces = CEDIT_TAB_WIDTH - (*screen_col % CEDIT_TAB_WIDTH);
      if (spaces == 0){
          spaces = CEDIT_TAB_WIDTH;
        }
      for (count = 0; count < spaces; count++){
          syntax_addch(window, screen_row, screen_col, col_offset, max_cols, ' ', attrs);
        }
      return;
    }

  if (*screen_col >= col_offset && *screen_col - col_offset < max_cols){
      mvwaddch(window, screen_row, *screen_col - col_offset, (chtype) ch | (chtype) attrs);
    }

  *screen_col += 1;
}

void syntax_init_colors(int syntax_theme_active, int popup_theme_active){
  if (has_colors()){
      start_color();
      use_default_colors();
      syntax_apply_theme(syntax_theme_active, popup_theme_active);
    }
}

void syntax_set_theme(int syntax_theme_active, int popup_theme_active){
  syntax_apply_theme(syntax_theme_active, popup_theme_active);
}

void syntax_draw_line(WINDOW *window, int row, const char *line, int col_offset,
                      int max_cols, int color_scheme_active){
  int index;
  int screen_col;

  if (line == NULL){
      return;
    }

  screen_col = 0;
  for (index = 0; line[index] != '\0';){
      if (!color_scheme_active){
          syntax_addch(window, row, &screen_col, col_offset, max_cols, line[index], A_NORMAL);
          index++;
          continue;
        }

      if (syntax_is_preprocessor_line(line, index)){
          while (line[index] != '\0'){
              syntax_addch(window, row, &screen_col, col_offset, max_cols,
                           line[index], COLOR_PAIR(CEDIT_COLOR_PREPROC));
              index++;
            }
          break;
        }

      if (line[index] == '/' && line[index + 1] == '/'){
          while (line[index] != '\0'){
              syntax_addch(window, row, &screen_col, col_offset, max_cols,
                           line[index], COLOR_PAIR(CEDIT_COLOR_COMMENT));
              index++;
            }
          break;
        }

      if (line[index] == '/' && line[index + 1] == '*'){
          while (line[index] != '\0'){
              int ch;

              ch = line[index];
              syntax_addch(window, row, &screen_col, col_offset, max_cols,
                           ch, COLOR_PAIR(CEDIT_COLOR_COMMENT));
              index++;
              if (ch == '*' && line[index] == '/'){
                  syntax_addch(window, row, &screen_col, col_offset, max_cols,
                               '/', COLOR_PAIR(CEDIT_COLOR_COMMENT));
                  index++;
                  break;
                }
            }
          continue;
        }

      if (line[index] == '"' || line[index] == '\''){
          int quote;

          quote = line[index];
          syntax_addch(window, row, &screen_col, col_offset, max_cols,
                       line[index], COLOR_PAIR(CEDIT_COLOR_STRING));
          index++;
          while (line[index] != '\0'){
              syntax_addch(window, row, &screen_col, col_offset, max_cols,
                           line[index], COLOR_PAIR(CEDIT_COLOR_STRING));
              if (line[index] == '\\' && line[index + 1] != '\0'){
                  index++;
                  syntax_addch(window, row, &screen_col, col_offset, max_cols,
                               line[index], COLOR_PAIR(CEDIT_COLOR_STRING));
                }
              else if (line[index] == quote){
                  index++;
                  break;
                }
              index++;
            }
          continue;
        }

      if (syntax_is_number_start(line, index)){
          while (isalnum((unsigned char) line[index]) || line[index] == '.'
                 || line[index] == 'x' || line[index] == 'X'
                 || line[index] == '+' || line[index] == '-'){
              syntax_addch(window, row, &screen_col, col_offset, max_cols,
                           line[index], COLOR_PAIR(CEDIT_COLOR_NUMBER));
              index++;
            }
          continue;
        }

      if (isalpha((unsigned char) line[index]) || line[index] == '_'){
          int start;
          int end;
          int attrs;

          start = index;
          while (isalnum((unsigned char) line[index]) || line[index] == '_'){
              index++;
            }
          end = index;
          attrs = COLOR_PAIR(CEDIT_COLOR_MAIN);
          if (syntax_is_keyword(line + start, (size_t) (end - start))){
              attrs = COLOR_PAIR(CEDIT_COLOR_KEYWORD);
            }
          else if (syntax_is_function_name(line, start, end)){
              attrs = COLOR_PAIR(CEDIT_COLOR_FUNCTION) | A_BOLD;
            }
          else if (syntax_is_upper_identifier(line, start, end)){
              attrs = COLOR_PAIR(CEDIT_COLOR_CONSTANT);
            }
          else if (syntax_is_entity_name(line, start, end)){
              attrs = COLOR_PAIR(CEDIT_COLOR_ENTITY);
            }
          else{
              attrs = COLOR_PAIR(CEDIT_COLOR_VARIABLE);
            }

          while (start < end){
              syntax_addch(window, row, &screen_col, col_offset, max_cols,
                           line[start], attrs);
              start++;
            }
          continue;
        }

      syntax_addch(window, row, &screen_col, col_offset, max_cols,
                   line[index], COLOR_PAIR(CEDIT_COLOR_MAIN));
      index++;
    }
}
