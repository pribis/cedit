#include "browser.h"
#include "popup.h"
#include "util.h"

static int browser_is_visible_entry(const EditorState *editor, const char *path, int is_dir);
static void browser_seed_input(const char *directory, char *input, size_t input_size);
static void browser_cleanup(BrowserState *browser, WINDOW *window, WINDOW *shadow);
static void browser_select_last_opened(const EditorState *editor, BrowserState *browser);
static void browser_notice(const EditorState *editor, const char *title, const char *message);

static void browser_free(BrowserState *browser){
  size_t index;

  for (index = 0; index < browser->count; index++){
    free(browser->entries[index].name);
    free(browser->entries[index].path);
  }

  free(browser->entries);
  memset(browser, 0, sizeof(*browser));
}

static int browser_append_entry(BrowserState *browser, size_t *capacity,
const char *name, const char *path, int is_dir){
  BrowserEntry *grown;
  BrowserEntry *entry;

  if (browser->count == *capacity){
    *capacity *= 2;
    grown = realloc(browser->entries, *capacity * sizeof(*grown));
    if (grown == NULL){
      return 0;
    }
    browser->entries = grown;
  }

  entry = &browser->entries[browser->count];
  memset(entry, 0, sizeof(*entry));
  entry->name = cedit_strdup(name);
  entry->path = cedit_strdup(path);
  if (entry->name == NULL || entry->path == NULL){
    free(entry->name);
    free(entry->path);
    memset(entry, 0, sizeof(*entry));
    return 0;
  }

  entry->is_dir = is_dir;
  browser->count++;
  return 1;
}

static int browser_is_visible_entry(const EditorState *editor, const char *path, int is_dir){
  (void) editor;
  (void) path;
  (void) is_dir;
  return 1;
}

static void browser_seed_input(const char *directory, char *input, size_t input_size){
  size_t length;

  strlcpy(input, directory, input_size);
  length = strlen(input);
  if (length > 0 && input[length - 1] != '/' && length + 1 < input_size){
    input[length] = '/';
    input[length + 1] = '\0';
  }
}

static void browser_cleanup(BrowserState *browser, WINDOW *window, WINDOW *shadow){
  set_escdelay(25);
  if (browser->cursor_state != ERR){
    curs_set(browser->cursor_state);
  }
  popup_destroy(window, shadow);
  browser_free(browser);
}

static void browser_notice(const EditorState *editor, const char *title, const char *message){
  WINDOW *window;
  WINDOW *shadow;
  int width;
  int height;
  int start_x;
  int start_y;

  width = (int) strlen(message) + 8;
  if (width < 28){
    width = 28;
  }
  if (width > editor->screen_cols - 2){
    width = editor->screen_cols - 2;
  }
  height = 6;
  start_x = (editor->screen_cols - width) / 2;
  start_y = (editor->screen_rows - height) / 2;

  shadow = popup_create_shadow(height, width, start_y, start_x);
  window = popup_create_window(height, width, start_y, start_x);
  if (window == NULL){
    popup_destroy(NULL, shadow);
    return;
  }
  keypad(window, TRUE);

  werase(window);
  popup_draw_box(window);
  mvwprintw(window, 0, 2, " %s ", title);
  mvwaddnstr(window, 2, 2, message, width - 4);
  mvwaddstr(window, 3, 2, "[Press any key]");
  wrefresh(window);
  wgetch(window);

  popup_destroy(window, shadow);
}

static void browser_select_last_opened(const EditorState *editor, BrowserState *browser){
  size_t index;

  if (editor->last_opened_path[0] == '\0'){
      browser->selected = browser->count > 1 ? 1 : 0;
      return;
    }

  for (index = 0; index < browser->count; index++){
      if (strcmp(browser->entries[index].path, editor->last_opened_path) == 0){
          browser->selected = index;
          return;
        }
    }

  browser->selected = browser->count > 1 ? 1 : 0;
}

static int browser_compare(const void *left, const void *right){
  const BrowserEntry *entry_left;
  const BrowserEntry *entry_right;

  entry_left = left;
  entry_right = right;
  if (strcmp(entry_left->name, "..") == 0){
    return -1;
  }
  if (strcmp(entry_right->name, "..") == 0){
    return 1;
  }
  if (entry_left->is_dir != entry_right->is_dir){
    return entry_right->is_dir - entry_left->is_dir;
  }

  return strcmp(entry_left->name, entry_right->name);
}

static int browser_load(const EditorState *editor, BrowserState *browser, const char *path){
  DIR *directory;
  struct dirent *entry;
  size_t capacity;
  char resolved[PATH_MAX];
  char parent[PATH_MAX];

  if (path == NULL || *path == '\0'){
    return 0;
  }

  if (realpath(path, resolved) == NULL){
    if (strlcpy(resolved, path, sizeof(resolved)) >= sizeof(resolved)){
      return 0;
    }
  }
  browser_free(browser);
  if (strlcpy(browser->cwd, resolved, sizeof(browser->cwd)) >= sizeof(browser->cwd)){
    return 0;
  }

  directory = opendir(browser->cwd);
  if (directory == NULL){
    return 0;
  }

  capacity = 32;
  browser->entries = calloc(capacity, sizeof(*browser->entries));
  if (browser->entries == NULL){
    closedir(directory);
    return 0;
  }

  if (strcmp(browser->cwd, "/") == 0){
    strlcpy(parent, "/", sizeof(parent));
  }
  else{
    strlcpy(parent, browser->cwd, sizeof(parent));
    cedit_parent_dir(parent);
  }
  if (!browser_append_entry(browser, &capacity, "..", parent, 1)){
    closedir(directory);
    return 0;
  }

  while ((entry = readdir(directory)) != NULL){
    BrowserEntry item;
    struct stat info;

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
      continue;
    }

    memset(&item, 0, sizeof(item));
    item.name = cedit_strdup(entry->d_name);
    item.path = cedit_join_path(browser->cwd, entry->d_name);
    if (item.name == NULL || item.path == NULL || stat(item.path, &info) != 0){
      free(item.name);
      free(item.path);
      continue;
    }

    item.is_dir = S_ISDIR(info.st_mode);
    if (!browser_is_visible_entry(editor, item.path, item.is_dir)){
      free(item.name);
      free(item.path);
      continue;
    }
    if (!browser_append_entry(browser, &capacity, item.name, item.path, item.is_dir)){
      free(item.name);
      free(item.path);
      closedir(directory);
      return 0;
    }
    free(item.name);
    free(item.path);
  }

  closedir(directory);
  qsort(browser->entries, browser->count, sizeof(*browser->entries), browser_compare);
  return 1;
}

static void browser_draw(WINDOW *window, const BrowserState *browser, int offset, int rows, int cols){
  int visible;
  int line;

  popup_draw_box(window);
  mvwprintw(window, 0, 2, " Open file ");
  mvwprintw(window, 1, 2, "Location: %s", browser->cwd);
  mvwprintw(window, rows + 2, 2, "Arrows=move Enter=open Esc=cancel");

  visible = rows;
  for (line = 0; line < visible; line++){
    int entry_index;
    const BrowserEntry *entry;
    char display[PATH_MAX + 8];
    char marker;

    entry_index = offset + line;
    if ((size_t) entry_index >= browser->count){
      break;
    }

    entry = &browser->entries[entry_index];
    marker = (size_t) entry_index == browser->selected ? '>' : ' ';
    snprintf(display, sizeof(display), "%c %s%s",
    marker, entry->name, entry->is_dir ? "/" : "");
    mvwhline(window, line + 2, 2, ' ', cols - 4);
    mvwaddnstr(window, line + 2, 2, display, cols - 4);
  }
  if (has_colors()){
    wattroff(window, COLOR_PAIR(CEDIT_COLOR_POPUP));
  }
}

int browser_browse(const EditorState *editor, const char *initial_dir,
char *selected_path, size_t selected_size){
  BrowserState browser;
  WINDOW *window;
  int width;
  int height;
  int start_x;
  int start_y;
  int view_rows;
  int offset;
  int key;
  WINDOW *shadow;

  memset(&browser, 0, sizeof(browser));
  if (!browser_load(editor, &browser, initial_dir)){
    browser_free(&browser);
    return 0;
  }
  browser_select_last_opened(editor, &browser);

  width = editor->screen_cols > 90 ? 90 : editor->screen_cols - 4;
  height = editor->screen_rows > 24 ? 24 : editor->screen_rows - 2;
  if (width < 30){
    width = 30;
  }
  if (height < 10){
    height = 10;
  }

  start_x = (editor->screen_cols - width) / 2;
  start_y = (editor->screen_rows - height) / 2;
  view_rows = height - 4;
  offset = 0;

  shadow = popup_create_shadow(height, width, start_y, start_x);
  window = popup_create_window(height, width, start_y, start_x);
  if (window == NULL){
    popup_destroy(NULL, shadow);
    browser_free(&browser);
    return 0;
  }
  keypad(window, TRUE);
  set_escdelay(75);
  browser.cursor_state = curs_set(0);

  while (1){
    if ((int) browser.selected < offset){
      offset = (int) browser.selected;
    }
    if ((int) browser.selected >= offset + view_rows){
      offset = (int) browser.selected - view_rows + 1;
    }

    werase(window);
    browser_draw(window, &browser, offset, view_rows, width);
    wmove(window, height - 1, width - 2);
    wrefresh(window);

    key = wgetch(window);
    if (key == 27){
      browser_cleanup(&browser, window, shadow);
      return 0;
    }

    if (key == KEY_UP){
      if (browser.selected > 0){
        browser.selected--;
      }
      continue;
    }

    if (key == KEY_DOWN){
      if (browser.selected + 1 < browser.count){
        browser.selected++;
      }
      continue;
    }

    if (key == '\n' || key == KEY_ENTER || key == '\r'){
      BrowserEntry *entry;

      if (browser.count == 0){
        continue;
      }

      entry = &browser.entries[browser.selected];
      if (entry->is_dir){
        if (strcmp(entry->path, browser.cwd) != 0){
          browser_load(editor, &browser, entry->path);
          browser.selected = browser.count > 1 ? 1 : 0;
        }
      }
      else{
        strlcpy(selected_path, entry->path, selected_size);
        browser_cleanup(&browser, window, shadow);
        return 1;
      }
    }
  }
}

int browser_save_as(const EditorState *editor, const char *initial_dir,
                char *selected_path, size_t selected_size){
  WINDOW *window;
  WINDOW *shadow;
  int width;
  int height;
  int start_x;
  int start_y;
  int focus;
  int key;
  int input_offset;
  int input_width;
  char directory[PATH_MAX];
  char input[PATH_MAX];
  size_t input_length;

  if (initial_dir == NULL || *initial_dir == '\0'){
    strlcpy(directory, ".", sizeof(directory));
  }
  else if (realpath(initial_dir, directory) == NULL){
    strlcpy(directory, initial_dir, sizeof(directory));
  }
  browser_seed_input(directory, input, sizeof(input));
  input_length = strlen(input);
  focus = 0;

  width = editor->screen_cols > 72 ? 72 : editor->screen_cols - 4;
  height = 9;
  if (width < 40){
    width = 40;
  }
  if (height > editor->screen_rows - 2){
    height = editor->screen_rows - 2;
  }

  start_x = (editor->screen_cols - width) / 2;
  start_y = (editor->screen_rows - height) / 2;

  shadow = popup_create_shadow(height, width, start_y, start_x);
  window = popup_create_window(height, width, start_y, start_x);
  if (window == NULL){
    popup_destroy(NULL, shadow);
    return 0;
  }
  keypad(window, TRUE);
  set_escdelay(75);
  curs_set(1);

  while (1){
    werase(window);
    popup_draw_box(window);
    mvwprintw(window, 0, 2, " Save file as ");
    mvwaddstr(window, 1, 2, "Save path:");
    input_width = width - 8;
    if (input_width < 1){
      input_width = 1;
    }
    mvwaddch(window, 2, 2, ACS_ULCORNER);
    mvwhline(window, 2, 3, ACS_HLINE, width - 6);
    mvwaddch(window, 2, width - 3, ACS_URCORNER);
    mvwaddch(window, 3, 2, ACS_VLINE);
    mvwhline(window, 3, 3, ' ', width - 6);
    mvwaddch(window, 3, width - 3, ACS_VLINE);
    mvwaddch(window, 4, 2, ACS_LLCORNER);
    mvwhline(window, 4, 3, ACS_HLINE, width - 6);
    mvwaddch(window, 4, width - 3, ACS_LRCORNER);
    input_offset = (int) input_length - input_width;
    if (input_offset < 0){
      input_offset = 0;
    }
    mvwaddnstr(window, 3, 3, input + input_offset, input_width);

    popup_draw_button(window, 6, 2, "Save", focus == 1);
    popup_draw_button(window, 6, 12, "Cancel", focus == 2);

    if (focus == 0){
      curs_set(1);
      if ((int) input_length - input_offset >= input_width){
        wmove(window, 3, 3 + input_width - 1);
      }
      else{
        wmove(window, 3, 3 + (int) input_length - input_offset);
      }
    }
    else{
      curs_set(1);
      wmove(window, 6, focus == 1 ? 2 : 12);
    }
    wrefresh(window);

    key = wgetch(window);
    if (key == 27){
      curs_set(1);
      popup_destroy(window, shadow);
      return 0;
    }

    if (key == KEY_BACKSPACE || key == 127 || key == '\b'){
      if (input_length > 0){
        input_length--;
        input[input_length] = '\0';
      }
      continue;
    }

    if (key == '\t'){
      focus = focus == 0 ? 1 : (focus == 1 ? 2 : 0);
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

    if ((key == '\n' || key == KEY_ENTER || key == '\r') && focus == 2){
      curs_set(1);
      popup_destroy(window, shadow);
      return 0;
    }

    if ((key == '\n' || key == KEY_ENTER || key == '\r') && focus != 2){
      const char *name;

      name = strrchr(input, '/');
      name = name == NULL ? input : name + 1;
      if (*name == '\0'){
        browser_notice(editor, "Save file as", "Enter a filename.");
        continue;
      }
      if (input[0] != '\0'){
        strlcpy(selected_path, input, selected_size);
        curs_set(1);
        popup_destroy(window, shadow);
        return 1;
      }
      continue;
    }

    if (isprint(key) && input_length + 1 < sizeof(input)){
      focus = 0;
      input[input_length++] = (char) key;
      input[input_length] = '\0';
    }
  }
}
