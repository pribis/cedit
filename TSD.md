# Technical Specification Document

## Overview

`cedit` is a single-process terminal editor written in C99 and linked against `ncurses`. The system is organized as a small set of C modules around one in-memory text buffer and one editor state object. The runtime is event-driven: initialize terminal state, enter a key-processing loop, update editor state, redraw, and clean up on exit.

## Build and runtime dependencies

- C99 compiler
- `ncurses`
- POSIX-style terminal and filesystem APIs:
  - `termios`
  - `dirent`
  - `stat`
  - `unistd`

Build entry point:

```sh
make
```

## Top-level architecture

| Module | Responsibility |
| --- | --- |
| `main.c` | Program entry point and lifecycle orchestration |
| `editor.c` / `editor.h` | Main UI loop, key handling, rendering, editing workflows, popups, undo, search, selection, save/open flows |
| `buffer.c` / `buffer.h` | In-memory text buffer storage and primitive text operations |
| `browser.c` / `browser.h` | Popup file open browser and save-as path input flow |
| `syntax.c` / `syntax.h` | Color-pair setup and line-by-line syntax rendering |
| `popup.c` / `popup.h` | Shared popup/shadow window helpers |
| `util.c` / `util.h` | Filesystem and string helpers |
| `cedit.h` | Shared includes, constants, and core data structures |

## Core data model

### `TextBuffer`

Defined in `cedit.h`.

- `char **lines`: dynamic array of heap-allocated lines
- `int line_count`: number of active lines
- `int capacity`: allocated line pointer capacity
- `int dirty`: unsaved-change flag
- `char *path`: current file path or `NULL` for unnamed buffers

The buffer stores text as one string per logical line with newline characters omitted from storage and reintroduced during save.

### `EditorState`

Central mutable runtime state for the editor.

Key fields:

- `buffer`: active `TextBuffer`
- `undo_head`: linked-list stack of snapshots
- `browser_dir`: starting directory for open/save flows
- `last_opened_path`: used to bias browser selection
- `clipboard`: internal copy/cut buffer
- `search_query`, `search_active`, `search_row`, `search_col`, `search_len`
- `color_scheme_active`, `show_line_numbers`
- `selecting`, `select_x`, `select_y`
- `cursor_x`, `cursor_y`
- `row_offset`, `col_offset`
- `screen_rows`, `screen_cols`
- `should_quit`
- `saved_tty`, `tty_saved`
- `status`, `status_time`

### `UndoNode`

Each undo node stores:

- a cloned `TextBuffer`
- cursor position
- scroll offsets
- pointer to the next node

Undo is implemented as whole-buffer snapshots, which keeps behavior simple at the cost of memory proportional to edit history and file size.

### `BrowserState`

Used only during open/save popup flows.

- `entries`: list of `BrowserEntry`
- `count`
- `selected`
- `cursor_state`
- `cwd`

## Execution flow

1. `main()` creates `EditorState`.
2. `editor_init()` initializes state, buffer, curses mode, colors, key mappings, terminal settings, and initial status.
3. If a file path is provided, `editor_open()` loads it into the buffer.
4. `editor_run()` loops until `should_quit` is set.
5. Each loop iteration:
   - redraws the full screen
   - waits for a key with `getch()`
   - dispatches through `editor_handle_key()`
6. `editor_shutdown()` restores terminal state and frees memory.

## Rendering pipeline

Primary render path in `editor.c`:

1. `editor_draw()`
2. `editor_refresh_size()`
3. `editor_scroll()`
4. `editor_draw_frame()`
5. `editor_draw_rows()`
6. `editor_draw_status()`
7. place cursor in visible screen coordinates

### Main view layout

- Outer bordered frame
- Content area inside the frame
- Optional gutter for line numbers
- Status bar on the last terminal row

### Syntax rendering

`syntax_draw_line()` performs lightweight lexical coloring directly while drawing each line. It recognizes:

- preprocessor lines beginning with `#`
- `//` comments
- `/* ... */` comment spans within a line
- string and character literals with basic escape handling
- numeric literals
- identifiers classified as:
  - keywords
  - function names
  - uppercase constants
  - entity-like identifiers
  - general variables

Coloring is enabled only when:

1. the user-facing color scheme toggle is on, and
2. the active file path has a supported C/C++ extension, or the buffer is unnamed

## Buffer operations

`buffer.c` provides the primitive text operations used by `editor.c`.

### Storage behavior

- Buffer capacity grows by doubling.
- An empty buffer always contains at least one line.
- Loading strips trailing `\n` and `\r` from input lines.
- Saving writes lines joined by newline characters.

### Editing primitives

- `buffer_insert_char()`
- `buffer_insert_newline()`
- `buffer_backspace()`
- `buffer_delete()`
- `buffer_kill_to_end()`
- `buffer_indent_line()`
- `buffer_set_line_indent()`
- `buffer_line_length()`
- `buffer_first_non_space()`

These primitives intentionally do not manage undo, search state, or UI status; those concerns stay in `editor.c`.

## Input handling

`editor_handle_key()` is the central dispatcher. It handles:

- window resize
- cursor/navigation movement
- open/new/save/save-as/quit
- help, search, go-to-line
- line-number and color toggles
- reindent
- copy, cut, paste, delete, backspace
- selection start and cancel
- undo
- printable text insertion

Custom escape sequences are registered for:

- `Ctrl-Left`
- `Ctrl-Right`
- `Ctrl-PageUp`
- `Ctrl-PageDown`

## Editing behavior details

### Undo

- A full snapshot is captured before most mutating actions.
- Undo restore replaces the active buffer and cursor/scroll state with the latest snapshot.
- The undo stack is cleared when opening a different file or creating a new one.

### Selection model

- Selection starts with key code `0` (`Ctrl-Space` in the intended terminal mapping).
- The stored anchor is `(select_x, select_y)`, but current editing logic uses only line boundaries.
- Copy/cut/select-delete operate on whole lines between anchor line and cursor line.

### Clipboard behavior

- Clipboard is internal to the process.
- Copy and cut write text into `editor->clipboard`.
- Kill-to-end also writes the removed text into the clipboard.
- Paste inserts clipboard contents verbatim, including embedded newlines.

### Search behavior

- Search query is stored in `search_query`.
- Matching is literal and case-sensitive using `strncmp`.
- Search state tracks only the current match.
- Repeated search continues after the previous match and wraps to the top.
- Search reset is triggered by most edit operations.

### Indentation and formatting

- `CEDIT_TAB_WIDTH` is `2`.
- Auto-indent is active for unnamed buffers and supported C/C++ file paths.
- Newline indentation is based on leading whitespace plus brace scanning around the cursor.
- Whole-file reindent recomputes indentation depth by scanning earlier lines while ignoring strings, chars, and comments.
- Typing `}` at or before the first non-space column repositions the line indentation before inserting the brace.

## File browser design

### Open browser

`browser_browse()` displays a popup listing directory entries.

Behavior:

- reads entries from the filesystem
- includes hidden files
- prepends `..` as a parent-directory entry
- sorts parent first, then directories before files, then lexicographically
- opens directories in place
- returns the selected file path to the editor

### Save-as flow

`browser_save_as()` is not a navigable browser. It is a popup text input seeded with the current directory and trailing slash. The user types a full save path, and the editor optionally confirms overwrite if the target already exists.

## Popup system

`popup.c` provides shared helpers to create:

- popup shadow window
- popup body window
- boxed borders
- simple focused buttons

These helpers are reused by confirm dialogs, notices, help, search, go-to-line, open browser, and save-as.

## Terminal management

Terminal setup in `editor_init()` and related helpers:

- `initscr()`
- `raw()`
- `noecho()`
- `keypad(stdscr, TRUE)`
- `meta(stdscr, TRUE)`
- color initialization when available
- custom key sequence registration
- explicit `termios` updates to disable several control-character behaviors

On shutdown:

- `endwin()` is called
- saved terminal attributes are restored when available

## Error handling strategy

- Most operations return success/failure and surface feedback through the editor status bar.
- Popup creation failures usually abort that popup flow quietly and return control to the editor.
- File open/save errors use `strerror(errno)` where appropriate.
- Buffer allocation failures generally fail the current operation without terminating the process.

## Current limitations

- Single active buffer only
- No persistent configuration file support
- No mouse support
- No external clipboard integration
- No multi-byte or Unicode-aware text handling
- Search is literal and case-sensitive only
- Selection is effectively line-based despite storing both x and y anchors
- Syntax highlighting is heuristic, not parser-based
