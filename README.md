# cedit

`cedit` is small, fast terminal editor for **C and C++ source editing only**. It is built with C99 and ncurses, stays lightweight, and focuses on quick editing instead of project management features.

## Scope

- Supports only C/C++ files: `.c`, `.cc`, `.c++`, `.cpp`, `.h`, `.hpp`
- Single-buffer editor only
- No buffer list, tabs, or split views
- No support for opening or editing non-C/C++ files

If you want multiple working files at same time, use **tmux** and run one `cedit` session per pane or window. tmux is recommended way to handle "buffers" with this editor.

## Expected workflow

`cedit` is meant to be used with other terminal tools to fill gaps it intentionally does not cover.

Example:

`cedit` lacks buffers. In other words, you cannot open multiple files at once. To compensate for this, use **tmux** and open `cedit` in multiple tmux panes or windows, then switch between them as needed.

## Build

```sh
make
```

## Run

Open supported file directly:

```sh
./cedit main.c
```

Or start editor and use file browser:

```sh
./cedit
```

## Main features

- Fast ncurses text UI
- C/C++ syntax coloring enabled by default
- Auto indentation and whole-file reindent
- Single-file open, save, and save-as
- File browser limited to supported source files
- Search with repeat (`F3`)
- Line number toggle (`F4`)
- Go to line (`Ctrl-G`)
- Syntax color scheme toggle (`F5`), enabled by default
- Undo (`Ctrl-U`)
- Copy, paste, selection, and line kill/delete commands

## Keys

- `F1` - help
- `F3` - search / next result
- `F4` - toggle line numbers
- `F5` - toggle color scheme
- `F6` - reindent whole file
- `Ctrl-F` - open file browser
- `Ctrl-G` - go to line
- `Ctrl-S` - save
- `Ctrl-W` - save as
- `Ctrl-Q` - quit
- `Ctrl-U` - undo
- `Ctrl-C` - copy selection
- `Ctrl-V` - paste
- `Ctrl-X` - delete from cursor to end of line
- `Ctrl-D` - delete character under cursor
- `Ctrl-Space` - start selection

## Notes

`cedit` is intentionally narrow in scope. It is for editing C/C++ source quickly in terminal, not for general-purpose text editing.
