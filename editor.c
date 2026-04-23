#include "editor.h"
#include "browser.h"
#include "buffer.h"
#include "popup.h"
#include "syntax.h"
#include "util.h"

static void editor_set_status(EditorState *editor, const char *format, ...);
static void editor_draw(EditorState *editor);
static void editor_handle_key(EditorState *editor, int key);
static void editor_scroll(EditorState *editor);
static void editor_move_cursor(EditorState *editor, int key);
static int editor_color_scheme_enabled(const EditorState *editor);
static int editor_content_cols(const EditorState *editor);
static int editor_text_cols(const EditorState *editor);
static int editor_line_number_width(const EditorState *editor);
static void editor_draw_frame(EditorState *editor);
static void editor_clear_undo(EditorState *editor);
static void editor_capture_undo(EditorState *editor);
static void editor_restore_undo(EditorState *editor);
static void editor_clear_selection(EditorState *editor);
static int editor_has_selection(const EditorState *editor);
static void editor_set_clipboard(EditorState *editor, const char *text);
static char *editor_collect_lines(const EditorState *editor, int start_y, int end_y);
static char *editor_selection_text(const EditorState *editor);
static char *editor_kill_text(const EditorState *editor);
static void editor_insert_text(EditorState *editor, const char *text);
static int editor_find_in_line(const char *line, const char *needle, int start_col, int end_col);
static int editor_search_next(EditorState *editor);
static void editor_search_dialog(EditorState *editor);
static void editor_goto_line(EditorState *editor);
static void editor_delete_selection(EditorState *editor);
static int editor_confirm(EditorState *editor, const char *title, const char *message);
static void editor_popup_text(EditorState *editor, const char *title, const char *text);
static void editor_popup_text_flags(EditorState *editor, const char *title, const char *text, int show_footer);
static int editor_save(EditorState *editor, int save_as);
static void editor_show_help(EditorState *editor);
static void editor_new_file(EditorState *editor);
static int editor_indent_for_newline(EditorState *editor);
static int editor_should_auto_indent(const EditorState *editor);
static int editor_desired_indent_for_row(const EditorState *editor, int row);
static void editor_set_browser_dir(EditorState *editor, const char *path);
static void editor_insert_closing_brace(EditorState *editor);
static void editor_insert_tab(EditorState *editor);
static void editor_indent_file(EditorState *editor);
static void editor_kill_line(EditorState *editor);
static void editor_toggle_color_scheme(EditorState *editor);
static void editor_save_terminal(EditorState *editor);
static void editor_configure_terminal(EditorState *editor);

static void editor_save_terminal(EditorState *editor){
  if (!editor->tty_saved && tcgetattr(STDIN_FILENO, &editor->saved_tty) == 0){
    editor->tty_saved = 1;
  }
}

static void editor_configure_terminal(EditorState *editor){
  struct termios tty;
  #ifdef _POSIX_VDISABLE
  cc_t disabled;
  #endif

  (void) editor;
  if (tcgetattr(STDIN_FILENO, &tty) != 0){
    return;
  }

  tty.c_iflag &= (tcflag_t) ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR);
  tty.c_lflag &= (tcflag_t) ~(ISIG | IEXTEN);
  #ifdef _POSIX_VDISABLE
  disabled = _POSIX_VDISABLE;
  #ifdef VINTR
  tty.c_cc[VINTR] = disabled;
  #endif
  #ifdef VQUIT
  tty.c_cc[VQUIT] = disabled;
  #endif
  #ifdef VSUSP
  tty.c_cc[VSUSP] = disabled;
  #endif
  #ifdef VSTART
  tty.c_cc[VSTART] = disabled;
  #endif
  #ifdef VSTOP
  tty.c_cc[VSTOP] = disabled;
  #endif
  #ifdef VLNEXT
  tty.c_cc[VLNEXT] = disabled;
  #endif
  #ifdef VDISCARD
  tty.c_cc[VDISCARD] = disabled;
  #endif
  #ifdef VWERASE
  tty.c_cc[VWERASE] = disabled;
  #endif
  #ifdef VREPRINT
  tty.c_cc[VREPRINT] = disabled;
  #endif
  #ifdef VSTATUS
  tty.c_cc[VSTATUS] = disabled;
  #endif
  #ifdef VDSUSP
  tty.c_cc[VDSUSP] = disabled;
  #endif
  #endif
  tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

static int editor_color_scheme_enabled(const EditorState *editor){
  return editor->color_scheme_active
         && (editor->buffer.path == NULL || cedit_supports_path(editor->buffer.path));
}

static int editor_text_rows(const EditorState *editor){
  int rows;

  rows = editor->screen_rows - 3;
  return rows > 1 ? rows : 1;
}

static int editor_content_cols(const EditorState *editor){
  int cols;

  cols = editor->screen_cols - 2;
  return cols > 1 ? cols : 1;
}

static int editor_line_number_width(const EditorState *editor){
  int width;
  int lines;

  if (!editor->show_line_numbers){
    return 0;
  }

  width = 2;
  lines = editor->buffer.line_count;
  while (lines >= 10){
    width++;
    lines /= 10;
  }
  return width + 1;
}

static int editor_text_cols(const EditorState *editor){
  int cols;

  cols = editor_content_cols(editor) - editor_line_number_width(editor);
  return cols > 1 ? cols : 1;
}

static int editor_current_line_length(const EditorState *editor){
  return buffer_line_length(&editor->buffer, editor->cursor_y);
}

static void editor_refresh_size(EditorState *editor){
  getmaxyx(stdscr, editor->screen_rows, editor->screen_cols);
}

static void editor_centered_window(EditorState *editor, int width, int height,
int *start_x, int *start_y){
  *start_x = (editor->screen_cols - width) / 2;
  *start_y = (editor->screen_rows - height) / 2;
}

static void editor_draw_frame(EditorState *editor){
  WINDOW *frame;

  frame = derwin(stdscr, editor->screen_rows - 1, editor->screen_cols, 0, 0);
  if (frame == NULL){
    return;
  }

  wbkgd(frame, COLOR_PAIR(CEDIT_COLOR_MAIN));
  werase(frame);
  wattron(frame, COLOR_PAIR(CEDIT_COLOR_BORDER) | A_DIM);
  box(frame, 0, 0);
  wattroff(frame, COLOR_PAIR(CEDIT_COLOR_BORDER) | A_DIM);
  wrefresh(frame);
  delwin(frame);
}

static void editor_clear_undo(EditorState *editor){
  UndoNode *node;

  node = editor->undo_head;
  while (node != NULL){
    UndoNode *next;

    next = node->next;
    buffer_free(&node->buffer);
    free(node);
    node = next;
  }
  editor->undo_head = NULL;
}

static void editor_set_status(EditorState *editor, const char *format, ...){
  va_list args;

  va_start(args, format);
  vsnprintf(editor->status, sizeof(editor->status), format, args);
  va_end(args);
  editor->status_time = time(NULL);
}

static void editor_capture_undo(EditorState *editor){
  UndoNode *node;
  TextBuffer snapshot;

  if (!buffer_clone(&snapshot, &editor->buffer)){
    editor_set_status(editor, "undo snapshot failed");
    return;
  }

  node = malloc(sizeof(*node));
  if (node == NULL){
    buffer_free(&snapshot);
    editor_set_status(editor, "undo snapshot failed");
    return;
  }

  node->buffer = snapshot;
  node->cursor_x = editor->cursor_x;
  node->cursor_y = editor->cursor_y;
  node->row_offset = editor->row_offset;
  node->col_offset = editor->col_offset;
  node->next = editor->undo_head;
  editor->undo_head = node;
}

static void editor_restore_undo(EditorState *editor){
  UndoNode *node;

  if (editor->undo_head == NULL){
    return;
  }

  node = editor->undo_head;
  editor->undo_head = node->next;

  buffer_free(&editor->buffer);
  editor->buffer = node->buffer;
  editor->cursor_x = node->cursor_x;
  editor->cursor_y = node->cursor_y;
  editor->row_offset = node->row_offset;
  editor->col_offset = node->col_offset;
  free(node);
  editor_clear_selection(editor);
}

static void editor_clear_selection(EditorState *editor){
  editor->selecting = 0;
}

static int editor_has_selection(const EditorState *editor){
  return editor->selecting;
}

static void editor_set_clipboard(EditorState *editor, const char *text){
  char *copy;

  copy = cedit_strdup(text == NULL ? "" : text);
  if (copy == NULL){
    editor_set_status(editor, "copy failed");
    return;
  }

  free(editor->clipboard);
  editor->clipboard = copy;
}

static char * editor_collect_lines(const EditorState *editor, int start_y, int end_y){
  size_t total;
  char *copy;
  char *cursor;
  int row;

  if (start_y < 0 || end_y >= editor->buffer.line_count || start_y > end_y){
    return cedit_strdup("");
  }

  total = 1;
  for (row = start_y; row <= end_y; row++){
    total += strlen(editor->buffer.lines[row]);
    if (row < end_y){
      total++;
    }
  }

  copy = malloc(total);
  if (copy == NULL){
    return NULL;
  }

  cursor = copy;
  for (row = start_y; row <= end_y; row++){
    size_t length;

    length = strlen(editor->buffer.lines[row]);
    memcpy(cursor, editor->buffer.lines[row], length);
    cursor += length;
    if (row < end_y){
      *cursor++ = '\n';
    }
  }
  *cursor = '\0';
  return copy;
}

static char * editor_selection_text(const EditorState *editor){
  int start_y;
  int end_y;
  char *copy;
  char *grown;
  size_t length;

  if (!editor_has_selection(editor)){
    return cedit_strdup("");
  }

  start_y = editor->select_y;
  end_y = editor->cursor_y;
  if (start_y > end_y){
    int temp_y;

    temp_y = start_y;
    start_y = end_y;
    end_y = temp_y;
  }
  copy = editor_collect_lines(editor, start_y, end_y);
  if (copy == NULL){
    return NULL;
  }
  length = strlen(copy);
  grown = realloc(copy, length + 2);
  if (grown == NULL){
    free(copy);
    return NULL;
  }
  copy = grown;
  copy[length] = '\n';
  copy[length + 1] = '\0';
  return copy;
}

static char * editor_kill_text(const EditorState *editor){
  const char *line;
  size_t length;
  size_t total;
  char *copy;

  line = editor->buffer.lines[editor->cursor_y];
  length = strlen(line);
  if ((size_t) editor->cursor_x > length){
    length = 0;
  }
  else{
    length -= (size_t) editor->cursor_x;
  }

  total = length + 1;
  if (editor->cursor_y + 1 < editor->buffer.line_count){
    total++;
  }

  copy = malloc(total);
  if (copy == NULL){
    return NULL;
  }

  if (length > 0){
    memcpy(copy, line + editor->cursor_x, length);
  }
  if (editor->cursor_y + 1 < editor->buffer.line_count){
    copy[length++] = '\n';
  }
  copy[length] = '\0';
  return copy;
}

static void editor_insert_text(EditorState *editor, const char *text){
  size_t index;

  if (text == NULL){
    return;
  }

  for (index = 0; text[index] != '\0'; index++){
    if (text[index] == '\n'){
      buffer_insert_newline(&editor->buffer, &editor->cursor_y, &editor->cursor_x, 0);
    }
    else{
      buffer_insert_char(&editor->buffer, editor->cursor_y, editor->cursor_x, text[index]);
      editor->cursor_x++;
    }
  }
}

static void editor_search_reset(EditorState *editor){
  editor->search_active = 0;
  editor->search_row = -1;
  editor->search_col = -1;
  editor->search_len = 0;
}

static int editor_find_in_line(const char *line, const char *needle, int start_col, int end_col){
  size_t line_length;
  size_t needle_length;
  int index;

  if (line == NULL || needle == NULL || *needle == '\0'){
    return -1;
  }

  line_length = strlen(line);
  needle_length = strlen(needle);
  if (needle_length > line_length){
    return -1;
  }
  if (start_col < 0){
    start_col = 0;
  }
  if ((size_t) start_col > line_length){
    return -1;
  }
  if (end_col < 0 || (size_t) end_col > line_length){
    end_col = (int) line_length;
  }
  if ((size_t) end_col < needle_length){
    return -1;
  }

  for (index = start_col; index + (int) needle_length <= end_col; index++){
    if (strncmp(line + index, needle, needle_length) == 0){
      return index;
    }
  }
  return -1;
}

static int editor_search_next(EditorState *editor){
  const char *needle;
  int start_row;
  int start_col;
  int row;
  int found_col;
  int wrapped;

  needle = editor->search_query;
  if (needle[0] == '\0'){
    editor_set_status(editor, "search text empty");
    return 0;
  }

  wrapped = 0;
  start_row = editor->cursor_y;
  start_col = editor->cursor_x;
  if (editor->search_active && editor->search_len > 0
  && editor->search_row >= 0 && editor->search_row < editor->buffer.line_count){
    start_row = editor->search_row;
    start_col = editor->search_col + editor->search_len;
  }

  for (row = start_row; row < editor->buffer.line_count; row++){
    found_col = editor_find_in_line(editor->buffer.lines[row], needle,
    row == start_row ? start_col : 0, -1);
    if (found_col >= 0){
      editor->search_active = 1;
      editor->search_row = row;
      editor->search_col = found_col;
      editor->search_len = (int) strlen(needle);
      editor->cursor_y = row;
      editor->cursor_x = found_col;
      return 1;
    }
  }

  for (row = 0; row <= start_row; row++){
    int end_col;

    wrapped = 1;
    end_col = row == start_row ? start_col : -1;
    found_col = editor_find_in_line(editor->buffer.lines[row], needle, 0, end_col);
    if (found_col >= 0){
      editor->search_active = 1;
      editor->search_row = row;
      editor->search_col = found_col;
      editor->search_len = (int) strlen(needle);
      editor->cursor_y = row;
      editor->cursor_x = found_col;
      editor_set_status(editor, "End of search results. Wraping");
      return 1;
    }
  }

  editor_search_reset(editor);
  if (wrapped){
    editor_set_status(editor, "search text not found");
  }
  else{
    editor_set_status(editor, "search text not found");
  }
  return 0;
}

static void editor_search_dialog(EditorState *editor){
  WINDOW *window;
  WINDOW *shadow;
  int width;
  int height;
  int start_x;
  int start_y;
  int focus;
  size_t input_length;
  int key;

  width = 38;
  if (width > editor->screen_cols - 2){
    width = editor->screen_cols - 2;
  }
  if (width < 28){
    width = 28;
  }
  height = 6;
  start_x = editor->screen_cols - width - 1;
  if (start_x < 0){
    start_x = 0;
  }
  start_y = 1;

  shadow = popup_create_shadow(height, width, start_y, start_x);
  window = popup_create_window(height, width, start_y, start_x);
  if (window == NULL){
    popup_destroy(NULL, shadow);
    return;
  }
  keypad(window, TRUE);
  focus = 0;
  input_length = strlen(editor->search_query);

  while (1){
    const char *primary_label;

    editor_draw(editor);
    werase(window);
    popup_draw_box(window);
    mvwprintw(window, 0, 2, " Search ");
    mvwaddstr(window, 1, 2, "Find:");
    mvwhline(window, 2, 2, ' ', width - 4);
    mvwaddnstr(window, 2, 2, editor->search_query, width - 5);
    mvwchgat(window, 2, 2, width - 4, A_REVERSE, 0, NULL);
    primary_label = (editor->search_active && editor->search_query[0] != '\0') ? "Next" : "Search";
    popup_draw_button(window, 4, 2, primary_label, focus == 1);
    popup_draw_button(window, 4, 12, "Cancel", focus == 2);
    if (focus == 0){
      wmove(window, 2, 2 + (int) input_length);
      curs_set(1);
    }
    else{
      curs_set(0);
      wmove(window, height - 1, width - 2);
    }
    wrefresh(window);

    key = wgetch(window);
    if (key == 27){
      break;
    }
    if (key == KEY_F(3)){
      editor_search_next(editor);
      continue;
    }
    if (key == '\t'){
      focus = focus == 0 ? 1 : 0;
      continue;
    }
    if (key == KEY_LEFT || key == KEY_RIGHT){
      if (focus == 0){
        focus = 1;
      }
      else if (focus == 1){
        focus = key == KEY_LEFT ? 0 : 2;
      }
      else{
        focus = key == KEY_RIGHT ? 0 : 1;
      }
      continue;
    }
    if (key == KEY_BACKSPACE || key == 127 || key == '\b'){
      if (input_length > 0){
        input_length--;
        editor->search_query[input_length] = '\0';
        editor_search_reset(editor);
      }
      continue;
    }
    if (key == '\n' || key == '\r' || key == KEY_ENTER){
      if (focus == 2){
        break;
      }
      editor_search_next(editor);
      continue;
    }
    if (isprint(key) && input_length + 1 < sizeof(editor->search_query)){
      focus = 0;
      editor->search_query[input_length++] = (char) key;
      editor->search_query[input_length] = '\0';
      editor_search_reset(editor);
    }
  }

  curs_set(1);
  popup_destroy(window, shadow);
}

static void editor_goto_line(EditorState *editor){
  WINDOW *window;
  WINDOW *shadow;
  int width;
  int height;
  int start_x;
  int start_y;
  int focus;
  char input[32];
  size_t input_length;
  int key;

  width = 28;
  if (width > editor->screen_cols - 2){
    width = editor->screen_cols - 2;
  }
  if (width < 24){
    width = 24;
  }
  height = 8;
  editor_centered_window(editor, width, height, &start_x, &start_y);
  shadow = popup_create_shadow(height, width, start_y, start_x);
  window = popup_create_window(height, width, start_y, start_x);
  if (window == NULL){
    popup_destroy(NULL, shadow);
    return;
  }
  keypad(window, TRUE);
  focus = 0;
  input[0] = '\0';
  input_length = 0;

  while (1){
    editor_draw(editor);
    werase(window);
    popup_draw_box(window);
    mvwprintw(window, 0, 2, " Go to line ");
    mvwaddstr(window, 1, 2, "Line:");
    mvwhline(window, 2, 2, ' ', width - 4);
    mvwaddnstr(window, 2, 2, input, width - 5);
    if (focus == 0){
      mvwchgat(window, 2, 2, width - 4, A_REVERSE, 0, NULL);
    }
    popup_draw_button(window, 4, 2, "[G]o", focus == 1);
    popup_draw_button(window, 4, 10, "[C]ancel", focus == 2);
    mvwaddstr(window, 6, 2, "Enter/G=go C/Esc=cancel");
    if (focus == 0){
      wmove(window, 2, 2 + (int) input_length);
      curs_set(1);
    }
    else{
      wmove(window, height - 1, width - 2);
      curs_set(0);
    }
    wrefresh(window);

    key = wgetch(window);
    if (key == 27 || key == 'c' || key == 'C'){
      break;
    }
    if (focus == 0){
      if (key == '\t' || key == KEY_DOWN || key == KEY_RIGHT){
        focus = 1;
        continue;
      }
      if (key == KEY_BACKSPACE || key == 127 || key == '\b'){
        if (input_length > 0){
          input[--input_length] = '\0';
        }
        continue;
      }
      if (key == '\n' || key == '\r' || key == KEY_ENTER){
        focus = 1;
        key = '\n';
      }
      else if (isdigit(key) && input_length + 1 < sizeof(input)){
        input[input_length++] = (char) key;
        input[input_length] = '\0';
        continue;
      }
    }

    if (key == '\t'){
      focus = focus == 1 ? 2 : 0;
      continue;
    }
    if (key == KEY_LEFT || key == KEY_RIGHT){
      focus = focus == 1 ? 2 : 1;
      continue;
    }
    if (key == KEY_UP){
      focus = 0;
      continue;
    }
    if (key == 'g' || key == 'G'){
      focus = 1;
      key = '\n';
    }
    if (key == '\n' || key == '\r' || key == KEY_ENTER){
      if (focus == 2){
        break;
      }
      if (input[0] == '\0'){
        editor_set_status(editor, "line number required");
        break;
      }
      editor->cursor_y = atoi(input) - 1;
      if (editor->cursor_y < 0){
        editor->cursor_y = 0;
      }
      if (editor->cursor_y >= editor->buffer.line_count){
        editor->cursor_y = editor->buffer.line_count - 1;
      }
      editor->cursor_x = 0;
      break;
    }
  }

  curs_set(1);
  popup_destroy(window, shadow);
}

static void editor_delete_selection(EditorState *editor){
  int start_y;
  int end_y;
  int row;
  int removed;

  if (!editor_has_selection(editor)){
    editor_clear_selection(editor);
    return;
  }

  start_y = editor->select_y;
  end_y = editor->cursor_y;
  if (start_y > end_y){
    int temp_y;

    temp_y = start_y;
    start_y = end_y;
    end_y = temp_y;
  }

  for (row = start_y; row <= end_y; row++){
    free(editor->buffer.lines[row]);
  }
  removed = end_y - start_y + 1;
  for (row = start_y; row + removed < editor->buffer.line_count; row++){
    editor->buffer.lines[row] = editor->buffer.lines[row + removed];
  }
  editor->buffer.line_count -= removed;
  if (editor->buffer.line_count == 0){
    editor->buffer.lines[0] = cedit_strdup("");
    editor->buffer.line_count = 1;
  }
  editor->buffer.dirty = 1;

  editor->cursor_x = 0;
  editor->cursor_y = start_y;
  if (editor->cursor_y >= editor->buffer.line_count){
    editor->cursor_y = editor->buffer.line_count - 1;
  }
  editor_clear_selection(editor);
}

static void editor_set_browser_dir(EditorState *editor, const char *path){
  char *dir;

  if (path == NULL || *path == '\0'){
    return;
  }

  if (cedit_dir_exists(path)){
    strlcpy(editor->browser_dir, path, sizeof(editor->browser_dir));
    return;
  }

  dir = cedit_dirname_dup(path);
  if (dir == NULL){
    return;
  }

  strlcpy(editor->browser_dir, dir, sizeof(editor->browser_dir));
  free(dir);
}

static void editor_init_key_sequences(void){
  define_key("\033[1;5D", CEDIT_KEY_CTRL_LEFT);
  define_key("\033[1;5C", CEDIT_KEY_CTRL_RIGHT);
  define_key("\033[5;5~", CEDIT_KEY_CTRL_PGUP);
  define_key("\033[6;5~", CEDIT_KEY_CTRL_PGDOWN);
}

static void editor_draw_rows(EditorState *editor){
  WINDOW *content;
  WINDOW *text;
  int gutter_width;
  int content_cols;
  int row;
  int text_rows;
  int text_cols;

  text_rows = editor_text_rows(editor);
  text_cols = editor_text_cols(editor);
  gutter_width = editor_line_number_width(editor);
  content_cols = editor_content_cols(editor);
  content = derwin(stdscr, text_rows, content_cols, 1, 1);
  if (content == NULL){
    return;
  }
  text = derwin(content, text_rows, text_cols, 0, gutter_width);
  if (text == NULL){
    delwin(content);
    return;
  }

  wbkgd(content, COLOR_PAIR(CEDIT_COLOR_MAIN));
  werase(content);
  wbkgd(text, COLOR_PAIR(CEDIT_COLOR_MAIN));
  werase(text);
  for (row = 0; row < text_rows; row++){
    int buffer_row;

    buffer_row = editor->row_offset + row;
    if (buffer_row >= editor->buffer.line_count){
      continue;
    }

    if (gutter_width > 0){
      mvwhline(content, row, 0, ' ', gutter_width);
      mvwprintw(content, row, 0, "%*d ", gutter_width - 1, buffer_row + 1);
      }
      syntax_draw_line(text, row, editor->buffer.lines[buffer_row],
      editor->col_offset, text_cols, editor_color_scheme_enabled(editor));

    if (editor_has_selection(editor)){
      int start_y;
      int end_y;

      start_y = editor->select_y;
      end_y = editor->cursor_y;
      if (start_y > end_y){
        int temp_y;

        temp_y = start_y;
        start_y = end_y;
        end_y = temp_y;
      }
      if (buffer_row >= start_y && buffer_row <= end_y){
        mvwchgat(content, row, 0, content_cols, A_REVERSE, 0, NULL);
      }
    }
    if (editor->search_active && editor->search_len > 0
    && buffer_row == editor->search_row){
      int start_col;
      int end_col;

      start_col = editor->search_col - editor->col_offset + gutter_width;
      end_col = editor->search_col + editor->search_len - editor->col_offset + gutter_width;
      if (start_col < gutter_width){
        start_col = gutter_width;
      }
      if (end_col > content_cols){
        end_col = content_cols;
      }
      if (end_col > start_col){
        mvwchgat(content, row, start_col, end_col - start_col,
        A_REVERSE | (editor_color_scheme_enabled(editor) ? A_BOLD : A_NORMAL),
        0, NULL);
      }
    }
  }
  wrefresh(text);
  wrefresh(content);
  delwin(text);
  delwin(content);
}

static void editor_draw_status(EditorState *editor){
  const char *file_name_only;
  const char *mode_text;
  char file_name[PATH_MAX + 32];

  if (editor->buffer.path == NULL){
    file_name_only = "[No Name]";
  }
  else{
    file_name_only = strrchr(editor->buffer.path, '/');
    file_name_only = file_name_only == NULL ? editor->buffer.path : file_name_only + 1;
  }

  move(editor->screen_rows - 1, 0);
  clrtoeol();
  attron(A_REVERSE | COLOR_PAIR(CEDIT_COLOR_MAIN));
  mode_text = editor_color_scheme_enabled(editor) ? "syntax color on" : "plain text";
  snprintf(file_name, sizeof(file_name), " Ln %d, Col %d | %s | %s%s ",
           editor->cursor_y + 1, editor->cursor_x + 1,
           mode_text,
           file_name_only, editor->buffer.dirty ? " *" : "");
  mvaddnstr(editor->screen_rows - 1, 0, file_name, editor->screen_cols - 1);

  if (editor->status[0] != '\0'){
    int status_col;

    status_col = editor->screen_cols - (int) strlen(editor->status) - 2;
    if (status_col < 0){
      status_col = 0;
    }
    mvaddnstr(editor->screen_rows - 1, status_col, editor->status,
    editor->screen_cols - status_col - 1);
  }
  attroff(A_REVERSE | COLOR_PAIR(CEDIT_COLOR_MAIN));
}

static void editor_draw(EditorState *editor){
  int gutter_width;
  int screen_x;
  int screen_y;

  editor_refresh_size(editor);
  editor_scroll(editor);
  erase();
  bkgd(COLOR_PAIR(CEDIT_COLOR_MAIN));
  editor_draw_frame(editor);
  editor_draw_rows(editor);
  editor_draw_status(editor);

  gutter_width = editor_line_number_width(editor);
  screen_y = editor->cursor_y - editor->row_offset + 1;
  screen_x = editor->cursor_x - editor->col_offset + gutter_width + 1;
  if (screen_y > 0 && screen_y <= editor_text_rows(editor)
  && screen_x > 0 && screen_x < editor->screen_cols - 1){
    move(screen_y, screen_x);
  }
  refresh();
}

static int editor_confirm(EditorState *editor, const char *title, const char *message){
  WINDOW *window;
  int width;
  int height;
  int start_x;
  int start_y;
  int key;
  WINDOW *shadow;

  width = (int) strlen(message) + 8;
  if (width < 32){
    width = 32;
  }
  if (width > editor->screen_cols - 2){
    width = editor->screen_cols - 2;
  }
  height = 6;

  editor_centered_window(editor, width, height, &start_x, &start_y);
  shadow = popup_create_shadow(height, width, start_y, start_x);
  window = popup_create_window(height, width, start_y, start_x);
  if (window == NULL){
    popup_destroy(NULL, shadow);
    return 0;
  }
  keypad(window, TRUE);

  while (1){
    werase(window);
    popup_draw_box(window);
    mvwprintw(window, 0, 2, " %s ", title);
    mvwaddnstr(window, 2, 2, message, width - 4);
    mvwaddstr(window, 3, 2, "[Y]es  [N]o");
    wrefresh(window);
    key = wgetch(window);
    if (key == 'y' || key == 'Y'){
      popup_destroy(window, shadow);
      return 1;
    }
    if (key == 'n' || key == 'N' || key == 27){
      popup_destroy(window, shadow);
      return 0;
    }
  }
}

static void editor_popup_text(EditorState *editor, const char *title, const char *text){
  editor_popup_text_flags(editor, title, text, 1);
}

static void editor_popup_text_flags(EditorState *editor, const char *title,
const char *text, int show_footer){
  WINDOW *window;
  WINDOW *shadow;
  char *copy;
  char *lines[256];
  int line_count;
  int offset;
  int height;
  int width;
  int start_x;
  int start_y;
  int key;
  int row;

  copy = cedit_strdup(text);
  if (copy == NULL){
    return;
  }

  line_count = 0;
  lines[line_count++] = copy;
  for (row = 0; copy[row] != '\0'; row++){
    if (copy[row] == '\n'){
      copy[row] = '\0';
      if (line_count < (int) (sizeof(lines) / sizeof(lines[0]))){
        lines[line_count++] = copy + row + 1;
      }
    }
  }

  width = editor->screen_cols > 92 ? 92 : editor->screen_cols - 2;
  if (width < 32){
    width = 32;
  }
  height = editor->screen_rows > 24 ? 24 : editor->screen_rows - 2;
  if (height < 8){
    height = 8;
  }

  editor_centered_window(editor, width, height, &start_x, &start_y);
  shadow = popup_create_shadow(height, width, start_y, start_x);
  window = popup_create_window(height, width, start_y, start_x);
  if (window == NULL){
    popup_destroy(NULL, shadow);
    free(copy);
    return;
  }
  keypad(window, TRUE);
  offset = 0;

  while (1){
    int max_rows;

    max_rows = height - 4;
    werase(window);
    popup_draw_box(window);
    mvwprintw(window, 0, 2, " %s ", title);
    for (row = 0; row < max_rows; row++){
      int index;

      index = offset + row;
      if (index >= line_count){
        break;
      }
      mvwaddnstr(window, row + 2, 2, lines[index], width - 4);
    }
    if (show_footer){
      mvwaddstr(window, height - 2, 2, "Esc=close Up/Down=scroll");
    }
    wrefresh(window);

    key = wgetch(window);
    if (key == 27 || key == '\n' || key == '\r' || key == KEY_ENTER){
      break;
    }
    if (key == KEY_UP && offset > 0){
      offset--;
    }
    if (key == KEY_DOWN && offset + max_rows < line_count){
      offset++;
    }
  }

  popup_destroy(window, shadow);
  free(copy);
}

static void editor_scroll(EditorState *editor){
  int visible_rows;

  visible_rows = editor_text_rows(editor);
  if (editor->cursor_y < editor->row_offset){
    editor->row_offset = editor->cursor_y;
  }
  if (editor->cursor_y >= editor->row_offset + visible_rows){
    editor->row_offset = editor->cursor_y - visible_rows + 1;
  }
  if (editor->cursor_x < editor->col_offset){
    editor->col_offset = editor->cursor_x;
  }
  if (editor->cursor_x >= editor->col_offset + editor_text_cols(editor)){
    editor->col_offset = editor->cursor_x - editor_text_cols(editor) + 1;
  }
}

static void editor_move_cursor(EditorState *editor, int key){
  switch (key){
    case KEY_LEFT:
    if (editor->cursor_x > 0){
      editor->cursor_x--;
    }
    else if (editor->cursor_y > 0){
      editor->cursor_y--;
      editor->cursor_x = buffer_line_length(&editor->buffer, editor->cursor_y);
    }
    break;

    case KEY_RIGHT:
    if (editor->cursor_x < editor_current_line_length(editor)){
      editor->cursor_x++;
    }
    else if (editor->cursor_y + 1 < editor->buffer.line_count){
      editor->cursor_y++;
      editor->cursor_x = 0;
    }
    break;

    case KEY_UP:
    if (editor->cursor_y > 0){
      editor->cursor_y--;
    }
    break;

    case KEY_DOWN:
    if (editor->cursor_y + 1 < editor->buffer.line_count){
      editor->cursor_y++;
    }
    break;

    case CEDIT_KEY_CTRL_LEFT:
    case KEY_HOME:
    editor->cursor_x = 0;
    break;

    case CEDIT_KEY_CTRL_RIGHT:
    case KEY_END:
    editor->cursor_x = editor_current_line_length(editor);
    break;

    case CEDIT_KEY_CTRL_PGUP:
    editor->cursor_y = 0;
    break;

    case CEDIT_KEY_CTRL_PGDOWN:
    editor->cursor_y = editor->buffer.line_count - 1;
    break;

    case KEY_PPAGE:
    editor->cursor_y -= editor_text_rows(editor);
    if (editor->cursor_y < 0){
      editor->cursor_y = 0;
    }
    break;

    case KEY_NPAGE:
    editor->cursor_y += editor_text_rows(editor);
    if (editor->cursor_y >= editor->buffer.line_count){
      editor->cursor_y = editor->buffer.line_count - 1;
    }
    break;
  }

  if (editor->cursor_x > editor_current_line_length(editor)){
    editor->cursor_x = editor_current_line_length(editor);
  }
}

static int editor_indent_for_newline(EditorState *editor){
  const char *line;
  int indent;
  int scan;

  line = editor->buffer.lines[editor->cursor_y];
  indent = buffer_first_non_space(line);

  for (scan = 0; scan < editor->cursor_x && line[scan] != '\0'; scan++){
    if (line[scan] == '{'){
      indent += CEDIT_TAB_WIDTH;
    }
    if (line[scan] == '}' && indent >= CEDIT_TAB_WIDTH){
      indent -= CEDIT_TAB_WIDTH;
    }
  }

  scan = editor->cursor_x;
  while (line[scan] != '\0' && isspace((unsigned char) line[scan])){
    scan++;
  }
  if (line[scan] == '}' && indent >= CEDIT_TAB_WIDTH){
    indent -= CEDIT_TAB_WIDTH;
  }

  if (indent < 0){
    indent = 0;
  }

  return indent;
}

static int editor_should_auto_indent(const EditorState *editor){
  return editor->buffer.path == NULL || cedit_supports_path(editor->buffer.path);
}

static int editor_desired_indent_for_row(const EditorState *editor, int row){
  int current_row;
  int depth;
  int in_block_comment;
  const char *line;
  int scan;

  depth = 0;
  in_block_comment = 0;
  for (current_row = 0; current_row < row; current_row++){
    int in_string;
    int in_char;
    int escaped;

    line = editor->buffer.lines[current_row];
    in_string = 0;
    in_char = 0;
    escaped = 0;

    for (scan = 0; line[scan] != '\0'; scan++){
      if (in_block_comment){
        if (line[scan] == '*' && line[scan + 1] == '/'){
          in_block_comment = 0;
          scan++;
        }
        continue;
      }

      if (in_string){
        if (!escaped && line[scan] == '"'){
          in_string = 0;
        }
        escaped = !escaped && line[scan] == '\\';
        continue;
      }

      if (in_char){
        if (!escaped && line[scan] == '\''){
          in_char = 0;
        }
        escaped = !escaped && line[scan] == '\\';
        continue;
      }

      if (line[scan] == '/' && line[scan + 1] == '/'){
        break;
      }
      if (line[scan] == '/' && line[scan + 1] == '*'){
        in_block_comment = 1;
        scan++;
        continue;
      }
      if (line[scan] == '"'){
        in_string = 1;
        escaped = 0;
        continue;
      }
      if (line[scan] == '\''){
        in_char = 1;
        escaped = 0;
        continue;
      }
      if (line[scan] == '{'){
        depth++;
      }
      else if (line[scan] == '}' && depth > 0){
        depth--;
      }
    }
  }

  if (row >= 0 && row < editor->buffer.line_count){
    line = editor->buffer.lines[row];
    scan = buffer_first_non_space(line);
    if (line[scan] == '}' && depth > 0){
      depth--;
    }
  }

  if (depth < 0){
    depth = 0;
  }
  return depth * CEDIT_TAB_WIDTH;
}

static void editor_insert_closing_brace(EditorState *editor){
  const char *line;
  int first_non_space;

  line = editor->buffer.lines[editor->cursor_y];
  first_non_space = buffer_first_non_space(line);

  if (editor_should_auto_indent(editor) && editor->cursor_x <= first_non_space){
    int desired_indent;

    desired_indent = editor_desired_indent_for_row(editor, editor->cursor_y);
    if (desired_indent >= CEDIT_TAB_WIDTH){
      desired_indent -= CEDIT_TAB_WIDTH;
    }
    else{
      desired_indent = 0;
    }

    buffer_set_line_indent(&editor->buffer, editor->cursor_y,
    &editor->cursor_x, desired_indent);
  }

  buffer_insert_char(&editor->buffer, editor->cursor_y, editor->cursor_x, '}');
  editor->cursor_x++;
}

static void editor_kill_line(EditorState *editor){
  char *killed;

  killed = editor_kill_text(editor);
  if (killed != NULL){
    editor_set_clipboard(editor, killed);
    free(killed);
  }
  editor_capture_undo(editor);
  editor_search_reset(editor);
  buffer_kill_to_end(&editor->buffer, editor->cursor_y, editor->cursor_x);
}

static void editor_insert_tab(EditorState *editor){
  const char *line;
  int first_non_space;

  line = editor->buffer.lines[editor->cursor_y];
  first_non_space = buffer_first_non_space(line);

  if (editor->cursor_x <= first_non_space){
    if (editor_should_auto_indent(editor)){
      int desired_indent;

      desired_indent = editor_desired_indent_for_row(editor, editor->cursor_y);
      buffer_set_line_indent(&editor->buffer, editor->cursor_y,
      &editor->cursor_x, desired_indent);
    }
    else{
      int spaces;

      spaces = CEDIT_TAB_WIDTH - (editor->cursor_x % CEDIT_TAB_WIDTH);
      if (spaces == 0){
        spaces = CEDIT_TAB_WIDTH;
      }
      buffer_indent_line(&editor->buffer, editor->cursor_y, &editor->cursor_x, spaces);
    }
  }
  else{
    int spaces;

    spaces = CEDIT_TAB_WIDTH - (editor->cursor_x % CEDIT_TAB_WIDTH);
    if (spaces == 0){
      spaces = CEDIT_TAB_WIDTH;
    }
    buffer_indent_line(&editor->buffer, editor->cursor_y, &editor->cursor_x, spaces);
  }
}

static void editor_indent_file(EditorState *editor){
  int row;
  int cursor_x;

  if (!editor_should_auto_indent(editor)){
    editor_set_status(editor, "formatting only for C/C++ files");
    return;
  }

  for (row = 0; row < editor->buffer.line_count; row++){
    const char *line;
    int desired_indent;
    int line_cursor;

    line = editor->buffer.lines[row];
    line_cursor = row == editor->cursor_y ? editor->cursor_x : 0;
    if (line[buffer_first_non_space(line)] == '\0'){
      desired_indent = 0;
    }
    else{
      desired_indent = editor_desired_indent_for_row(editor, row);
    }
    buffer_set_line_indent(&editor->buffer, row, &line_cursor, desired_indent);
    if (row == editor->cursor_y){
      cursor_x = line_cursor;
    }
  }

  if (editor->cursor_y >= editor->buffer.line_count){
    editor->cursor_y = editor->buffer.line_count - 1;
  }
  editor->cursor_x = cursor_x;
  if (editor->cursor_x > editor_current_line_length(editor)){
    editor->cursor_x = editor_current_line_length(editor);
  }
  editor_set_status(editor, "file indented");
}

static int editor_save(EditorState *editor, int save_as){
  char path[PATH_MAX];

  if (!save_as && !editor->buffer.dirty){
    editor_set_status(editor, "nothing to save");
    return 1;
  }

  if (!save_as && editor->buffer.path != NULL){
    if (buffer_save(&editor->buffer, editor->buffer.path)){
      editor_set_browser_dir(editor, editor->buffer.path);
      editor_set_status(editor, "saved");
      return 1;
    }
    editor_set_status(editor, "save failed: %s", strerror(errno));
    return 0;
  }

  if (!browser_save_as(editor, editor->browser_dir, path, sizeof(path))){
    editor_set_status(editor, "save canceled");
    return 0;
  }

  if (cedit_file_exists(path)
  && (editor->buffer.path == NULL || strcmp(editor->buffer.path, path) != 0)
  && !editor_confirm(editor, "Overwrite file", "File exists. Overwrite?")){
    editor_set_status(editor, "save canceled");
    return 0;
  }

  if (buffer_save(&editor->buffer, path)){
    editor_set_browser_dir(editor, path);
    editor_set_status(editor, "saved %s", path);
    return 1;
  }

  editor_set_status(editor, "save failed: %s", strerror(errno));
  return 0;
}

static void editor_new_file(EditorState *editor){
  if (editor->buffer.dirty
      && !editor_confirm(editor, "New file", "Unsaved changes. Create new file?")){
    editor_set_status(editor, "new file canceled");
    return;
  }

  editor_clear_undo(editor);
  editor_clear_selection(editor);
  editor_search_reset(editor);
  buffer_free(&editor->buffer);
  buffer_init(&editor->buffer);
  editor->cursor_x = 0;
  editor->cursor_y = 0;
  editor->row_offset = 0;
  editor->col_offset = 0;
  editor->last_opened_path[0] = '\0';
  syntax_set_theme(editor_color_scheme_enabled(editor), editor->color_scheme_active);
  editor_set_status(editor, editor_color_scheme_enabled(editor) ? "syntax color on" : "plain text");
}

static void editor_show_help(EditorState *editor){
  editor_popup_text(
  editor,
  "Help",
  "Command keys\n"
  "F1            Show help\n"
  "F3            Search / next hit\n"
  "F4            Toggle line numbers\n"
  "F5            Toggle color scheme\n"
  "Ctrl-G        Go to line\n"
  "F6            Reindent whole file\n"
  "Ctrl-F        Open file browser\n"
  "Ctrl-N        New file\n"
  "Ctrl-S        Save file\n"
  "Ctrl-W        Save file as\n"
  "Ctrl-Q        Quit editor\n"
  "Ctrl-U        Undo last edit\n"
  "Ctrl-C        Copy selected text\n"
  "Ctrl-V        Paste copied or killed text\n"
  "Ctrl-X        Delete from cursor to end of line\n"
  "Ctrl-D        Delete char under cursor\n"
  "Ctrl-Space    Start selection\n"
  "Esc           Cancel selection or close popup\n"
  "Arrows        Move cursor\n"
  "Ctrl-Left     Move to line start\n"
  "Ctrl-Right    Move to line end\n"
  "Ctrl-PageUp   Move to top of file\n"
  "Ctrl-PageDown Move to bottom of file\n"
  "\n"
  "Browser keys\n"
  "Arrows        Move browser selection\n"
  "Enter         Open folder or file\n"
  "..            Move to parent folder\n"
  "Esc           Close browser");
}

static void editor_toggle_color_scheme(EditorState *editor){
  editor->color_scheme_active = !editor->color_scheme_active;
  syntax_set_theme(editor_color_scheme_enabled(editor), editor->color_scheme_active);
  if (editor->color_scheme_active && !editor_color_scheme_enabled(editor)){
    editor_set_status(editor, "plain text for this file type");
  }
  else{
    editor_set_status(editor, editor->color_scheme_active ? "color scheme on" : "plain text");
  }
}

static void editor_open_browser(EditorState *editor){
  char path[PATH_MAX];

  if (!browser_browse(editor, editor->browser_dir, path, sizeof(path))){
    editor_set_status(editor, "open canceled");
    return;
  }

  editor_open(editor, path);
}

static void editor_handle_key(EditorState *editor, int key){
  switch (key){
    case KEY_RESIZE:
    editor_refresh_size(editor);
    return;

    case 27:
    if (editor->selecting){
      editor_clear_selection(editor);
    }
    return;

    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_UP:
    case KEY_DOWN:
    case KEY_HOME:
    case KEY_END:
    case KEY_PPAGE:
    case KEY_NPAGE:
    case CEDIT_KEY_CTRL_LEFT:
    case CEDIT_KEY_CTRL_RIGHT:
    case CEDIT_KEY_CTRL_PGUP:
    case CEDIT_KEY_CTRL_PGDOWN:
    editor_move_cursor(editor, key);
    return;

    case CEDIT_CTRL_KEY('f'):
    editor_open_browser(editor);
    return;

    case CEDIT_CTRL_KEY('g'):
    editor_goto_line(editor);
    return;

    case KEY_F(4):
    editor->show_line_numbers = !editor->show_line_numbers;
    return;

    case CEDIT_CTRL_KEY('n'):
    editor_new_file(editor);
    return;

    case CEDIT_CTRL_KEY('s'):
    editor_save(editor, 0);
    return;

    case CEDIT_CTRL_KEY('w'):
    editor_save(editor, 1);
    return;

    case KEY_F(3):
    if (editor->search_query[0] != '\0' && editor->search_active){
      editor_search_next(editor);
    }
    else{
      editor_search_dialog(editor);
    }
    return;

    case CEDIT_CTRL_KEY('q'):
    if (!editor->buffer.dirty
    || editor_confirm(editor, "Quit", "Unsaved changes. Quit anyway?")){
      editor->should_quit = 1;
    }
    return;

    case CEDIT_CTRL_KEY('c'):
    if (editor_has_selection(editor)){
      char *selected;

      selected = editor_selection_text(editor);
      if (selected == NULL){
        editor_set_status(editor, "copy failed");
      }
      else{
        editor_set_clipboard(editor, selected);
        free(selected);
        editor_set_status(editor, "copied");
      }
      editor_clear_selection(editor);
    }
    else{
      editor_set_status(editor, "nothing selected");
    }
    return;

    case CEDIT_CTRL_KEY('v'):
    if (editor->clipboard == NULL || editor->clipboard[0] == '\0'){
      editor_set_status(editor, "clipboard empty");
      return;
    }
    editor_capture_undo(editor);
    editor_search_reset(editor);
    if (editor_has_selection(editor)){
      editor_delete_selection(editor);
    }
    editor_insert_text(editor, editor->clipboard);
    return;

    case 0:
    editor->selecting = 1;
    editor->select_x = editor->cursor_x;
    editor->select_y = editor->cursor_y;
    return;

    case CEDIT_CTRL_KEY('d'):
    editor_capture_undo(editor);
    editor_search_reset(editor);
    if (editor_has_selection(editor)){
      editor_delete_selection(editor);
    }
    else{
      buffer_delete(&editor->buffer, editor->cursor_y, editor->cursor_x);
    }
    return;

    case CEDIT_CTRL_KEY('x'):
    if (editor_has_selection(editor)){
      char *selected;

      selected = editor_selection_text(editor);
      if (selected != NULL){
        editor_set_clipboard(editor, selected);
        free(selected);
      }
      editor_capture_undo(editor);
      editor_search_reset(editor);
      editor_delete_selection(editor);
    }
    else{
      editor_kill_line(editor);
    }
    return;

    case CEDIT_CTRL_KEY('u'):
    editor_restore_undo(editor);
    return;

    case KEY_F(1):
    editor_show_help(editor);
    return;

    case KEY_F(5):
    editor_toggle_color_scheme(editor);
    return;

    case KEY_F(6):
    editor_capture_undo(editor);
    editor_search_reset(editor);
    editor_indent_file(editor);
    return;

    case KEY_BACKSPACE:
    case 127:
    case '\b':
    editor_capture_undo(editor);
    editor_search_reset(editor);
    buffer_backspace(&editor->buffer, &editor->cursor_y, &editor->cursor_x);
    return;

    case KEY_DC:
    editor_capture_undo(editor);
    editor_search_reset(editor);
    if (editor_has_selection(editor)){
      editor_delete_selection(editor);
    }
    else{
      buffer_delete(&editor->buffer, editor->cursor_y, editor->cursor_x);
    }
    return;

    case '\n':
    case '\r':
    case KEY_ENTER:
    editor_capture_undo(editor);
    editor_search_reset(editor);
    buffer_insert_newline(&editor->buffer, &editor->cursor_y, &editor->cursor_x,
    editor_should_auto_indent(editor)
    ? editor_indent_for_newline(editor)
    : 0);
    return;

    case '\t':
    editor_capture_undo(editor);
    editor_search_reset(editor);
    editor_insert_tab(editor);
    return;
  }

  if (isprint(key)){
    int replaced_selection;

    replaced_selection = 0;
    if (editor_has_selection(editor)){
      editor_capture_undo(editor);
      editor_search_reset(editor);
      editor_delete_selection(editor);
      replaced_selection = 1;
    }

    if (key == '}'){
      if (!replaced_selection){
        editor_capture_undo(editor);
        editor_search_reset(editor);
      }
      editor_insert_closing_brace(editor);
    }
    else{
      if (!replaced_selection){
        editor_capture_undo(editor);
        editor_search_reset(editor);
      }
      buffer_insert_char(&editor->buffer, editor->cursor_y, editor->cursor_x, key);
      editor->cursor_x++;
    }
  }
}

void editor_init(EditorState *editor){
  memset(editor, 0, sizeof(*editor));
  editor->color_scheme_active = 1;
  buffer_init(&editor->buffer);
  if (getcwd(editor->browser_dir, sizeof(editor->browser_dir)) == NULL){
    strlcpy(editor->browser_dir, ".", sizeof(editor->browser_dir));
  }

  editor_save_terminal(editor);
  initscr();
  raw();
  noecho();
  keypad(stdscr, TRUE);
  meta(stdscr, TRUE);
  syntax_init_colors(editor_color_scheme_enabled(editor), editor->color_scheme_active);
  editor_init_key_sequences();
  editor_configure_terminal(editor);
  curs_set(1);
  set_escdelay(25);
  editor_refresh_size(editor);
  editor_set_status(editor, "F1 help");
}

void editor_open(EditorState *editor, const char *path){
  editor_clear_undo(editor);
  editor_clear_selection(editor);
  editor_search_reset(editor);
  if (!buffer_load(&editor->buffer, path)){
    editor_set_status(editor, "open failed: %s", strerror(errno));
    return;
  }

  editor->cursor_x = 0;
  editor->cursor_y = 0;
  editor->row_offset = 0;
  editor->col_offset = 0;
  strlcpy(editor->last_opened_path, path, sizeof(editor->last_opened_path));
  editor_set_browser_dir(editor, path);
  syntax_set_theme(editor_color_scheme_enabled(editor), editor->color_scheme_active);
  editor_set_status(editor, "opened %s", path);
}

void editor_run(EditorState *editor){
  int key;

  while (!editor->should_quit){
    editor_draw(editor);
    key = getch();
    editor_handle_key(editor, key);
  }
}

void editor_shutdown(EditorState *editor){
  endwin();
  if (editor->tty_saved){
    tcsetattr(STDIN_FILENO, TCSANOW, &editor->saved_tty);
  }
  free(editor->clipboard);
  buffer_free(&editor->buffer);
  editor_clear_undo(editor);
}
