#ifndef CEDIT_H
#define CEDIT_H

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CEDIT_TAB_WIDTH 2
#define CEDIT_STATUS_SIZE 256
#define CEDIT_CTRL_KEY(k) ((k) & 0x1f)
#define CEDIT_KEY_CTRL_LEFT 2001
#define CEDIT_KEY_CTRL_RIGHT 2002
#define CEDIT_KEY_CTRL_PGUP 2003
#define CEDIT_KEY_CTRL_PGDOWN 2004
#define CEDIT_COLOR_MAIN 1
#define CEDIT_COLOR_COMMENT 2
#define CEDIT_COLOR_BORDER 3
#define CEDIT_COLOR_POPUP 4
#define CEDIT_COLOR_SHADOW 5
#define CEDIT_COLOR_KEYWORD 6
#define CEDIT_COLOR_STRING 7
#define CEDIT_COLOR_NUMBER 8
#define CEDIT_COLOR_PREPROC 9
#define CEDIT_COLOR_FUNCTION 10
#define CEDIT_COLOR_ENTITY 11
#define CEDIT_COLOR_VARIABLE 12
#define CEDIT_COLOR_CONSTANT 13

typedef struct {
  char **lines;
  int line_count;
  int capacity;
  int dirty;
  char *path;
} TextBuffer;

typedef struct {
  char *name;
  char *path;
  int is_dir;
} BrowserEntry;

typedef struct {
  BrowserEntry *entries;
  size_t count;
  size_t selected;
  int cursor_state;
  char cwd[PATH_MAX];
} BrowserState;

typedef struct UndoNode {
  TextBuffer buffer;
  int cursor_x;
  int cursor_y;
  int row_offset;
  int col_offset;
  struct UndoNode *next;
} UndoNode;

typedef struct {
  TextBuffer buffer;
  UndoNode *undo_head;
  char browser_dir[PATH_MAX];
  char last_opened_path[PATH_MAX];
  char *clipboard;
  char search_query[256];
  int color_scheme_active;
  int show_line_numbers;
  int selecting;
  int select_x;
  int select_y;
  int search_active;
  int search_row;
  int search_col;
  int search_len;
  int cursor_x;
  int cursor_y;
  int row_offset;
  int col_offset;
  int screen_rows;
  int screen_cols;
  int should_quit;
  int tty_saved;
  struct termios saved_tty;
  char status[CEDIT_STATUS_SIZE];
  time_t status_time;
} EditorState;

#endif
