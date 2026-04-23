#include "browser.h"
#include "util.h"

static WINDOW *browser_create_shadow(int height, int width, int start_y, int start_x);
static WINDOW *browser_create_popup(int height, int width, int start_y, int start_x);
static int browser_read_key(WINDOW *window);
static int browser_is_visible_entry(const EditorState *editor, const char *path, int is_dir);
static void browser_seed_input(const char *directory, char *input, size_t input_size);
static void browser_cleanup(BrowserState *browser, WINDOW *window, WINDOW *shadow);
static void browser_select_last_opened(const EditorState *editor, BrowserState *browser);

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

static WINDOW * browser_create_shadow(int height, int width, int start_y, int start_x){
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

static WINDOW * browser_create_popup(int height, int width, int start_y, int start_x){
  WINDOW *window;

  window = newwin(height, width, start_y, start_x);
  if (window != NULL && has_colors()){
    wbkgd(window, COLOR_PAIR(CEDIT_COLOR_POPUP));
  }
  return window;
}

static int browser_read_key(WINDOW *window){
  int key;

  (void) window;
  key = getch();
  if (key == KEY_UP || key == KEY_DOWN){
    return key;
  }
  if (key == 'k'){
    return KEY_UP;
  }
  if (key == 'j'){
    return KEY_DOWN;
  }
  return key;
}

static int browser_is_visible_entry(const EditorState *editor, const char *path, int is_dir){
  (void) editor;
  if (is_dir){
    return 1;
  }

  return cedit_supports_path(path);
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
  if (window != NULL){
    delwin(window);
  }
  if (shadow != NULL){
    delwin(shadow);
  }
  browser_free(browser);
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

  if (has_colors()){
    wattron(window, COLOR_PAIR(CEDIT_COLOR_POPUP) | A_BOLD);
  }
  box(window, 0, 0);
  if (has_colors()){
    wattroff(window, COLOR_PAIR(CEDIT_COLOR_POPUP) | A_BOLD);
    wattron(window, COLOR_PAIR(CEDIT_COLOR_POPUP));
  }
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

  shadow = browser_create_shadow(height, width, start_y, start_x);
  window = browser_create_popup(height, width, start_y, start_x);
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

    key = browser_read_key(window);
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
  BrowserState browser;
  WINDOW *window;
  WINDOW *shadow;
  int width;
  int height;
  int start_x;
  int start_y;
  int view_rows;
  int offset;
  int key;
  char input[PATH_MAX];
  size_t input_length;

  memset(&browser, 0, sizeof(browser));
  if (!browser_load(editor, &browser, initial_dir)){
    browser_free(&browser);
    return 0;
  }
  if (browser.count > 1){
    browser.selected = 1;
  }

  browser_seed_input(browser.cwd, input, sizeof(input));
  input_length = strlen(input);

  width = editor->screen_cols > 96 ? 96 : editor->screen_cols - 4;
  height = editor->screen_rows > 26 ? 26 : editor->screen_rows - 2;
  if (width < 40){
    width = 40;
  }
  if (height < 12){
    height = 12;
  }

  start_x = (editor->screen_cols - width) / 2;
  start_y = (editor->screen_rows - height) / 2;
  view_rows = height - 7;
  offset = 0;

  shadow = browser_create_shadow(height, width, start_y, start_x);
  window = browser_create_popup(height, width, start_y, start_x);
  keypad(window, TRUE);
  set_escdelay(75);
  browser.cursor_state = curs_set(0);

  while (1){
    int row;

    if ((int) browser.selected < offset){
      offset = (int) browser.selected;
    }
    if ((int) browser.selected >= offset + view_rows){
      offset = (int) browser.selected - view_rows + 1;
    }

    werase(window);
    if (has_colors()){
      wattron(window, COLOR_PAIR(CEDIT_COLOR_POPUP) | A_BOLD);
    }
    box(window, 0, 0);
    if (has_colors()){
      wattroff(window, COLOR_PAIR(CEDIT_COLOR_POPUP) | A_BOLD);
      wattron(window, COLOR_PAIR(CEDIT_COLOR_POPUP));
    }
    mvwprintw(window, 0, 2, " Save file as ");
    mvwprintw(window, 1, 2, "Location: %s", browser.cwd);
    for (row = 0; row < view_rows; row++){
      int entry_index;
      const BrowserEntry *entry;
      char display[PATH_MAX + 8];
      char marker;

      entry_index = offset + row;
      if ((size_t) entry_index >= browser.count){
        break;
      }

      entry = &browser.entries[entry_index];
      marker = (size_t) entry_index == browser.selected ? '>' : ' ';
      snprintf(display, sizeof(display), "%c %s%s",
      marker, entry->name, entry->is_dir ? "/" : "");
      mvwaddnstr(window, row + 2, 2, display, width - 4);
    }
    mvwaddstr(window, height - 4, 2, "Save path:");
    mvwaddnstr(window, height - 3, 2, input, width - 4);
    mvwaddstr(window, height - 2, 2, "Type name. Enter=open dir/save Esc=cancel");
    wmove(window, height - 1, width - 2);
    wrefresh(window);

    key = browser_read_key(window);
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

    if (key == KEY_BACKSPACE || key == 127 || key == '\b'){
      if (input_length > 0){
        input_length--;
        input[input_length] = '\0';
      }
      continue;
    }

    if (key == '\n' || key == KEY_ENTER || key == '\r'){
      BrowserEntry *entry;
      char default_input[PATH_MAX];

      browser_seed_input(browser.cwd, default_input, sizeof(default_input));
      if (input[0] != '\0' && strcmp(input, default_input) != 0
      && strcmp(input, browser.cwd) != 0){
        if (cedit_dir_exists(input)){
          browser_load(editor, &browser, input);
          browser.selected = browser.count > 1 ? 1 : 0;
          browser_seed_input(browser.cwd, input, sizeof(input));
          input_length = strlen(input);
          continue;
        }

        strlcpy(selected_path, input, selected_size);
        browser_cleanup(&browser, window, shadow);
        return 1;
      }

      if (browser.count == 0){
        continue;
      }

      entry = &browser.entries[browser.selected];
      if (entry->is_dir){
        if (strcmp(entry->path, browser.cwd) != 0){
          browser_load(editor, &browser, entry->path);
          browser.selected = browser.count > 1 ? 1 : 0;
          browser_seed_input(browser.cwd, input, sizeof(input));
          input_length = strlen(input);
        }
      }
      else{
        strlcpy(selected_path, entry->path, selected_size);
        browser_cleanup(&browser, window, shadow);
        return 1;
      }
      continue;
    }

    if (isprint(key) && input_length + 1 < sizeof(input)){
      input[input_length++] = (char) key;
      input[input_length] = '\0';
    }
  }
}
