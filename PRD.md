# Product Requirements Document

## Product

**cedit** is a lightweight terminal text editor implemented in C99 on top of `ncurses`. The current product is optimized for quick editing of a single file at a time, with extra formatting and syntax-color support for C and C++ source files.

This PRD reflects the behavior implemented in the codebase today.

## Problem

Developers working in a terminal sometimes need a small editor that starts quickly, works without a GUI, and supports common source-editing tasks without the weight of a full IDE. The product solves that by providing a focused TUI editor with file open/save flows, search, navigation, selection, clipboard-style editing, and C/C++-aware formatting.

## Target user

- Developers editing code directly in a terminal session
- Users who prefer a small single-file editor over a multi-buffer environment
- C and C++ users who want syntax coloring and indentation support without leaving the shell

## Product goals

1. Start quickly and stay lightweight.
2. Support an efficient single-file editing workflow in a terminal.
3. Provide C/C++-specific syntax coloring and indentation while still allowing any file to be opened.
4. Keep the UI simple, keyboard-driven, and popup-based.

## Non-goals

- Multi-buffer, tabbed, or split-pane editing
- Project management, builds, or terminal multiplexing features
- Language-server integration, autocomplete, diagnostics, or refactoring tools
- General-purpose syntax support beyond the built-in C/C++ handling
- Configuration files, themes, or plugin systems

## Core workflow

1. Launch `cedit` with or without a file path.
2. Edit text directly in the main window.
3. Open another file through the file browser or create a new buffer.
4. Save in place or use save-as.
5. Search, jump to a line, reindent the file, or toggle line numbers/coloring as needed.

## Functional requirements

### File handling

- Open a file passed on the command line at startup.
- Open a file later through a popup file browser.
- Create a new empty buffer.
- Save to the current path when the buffer already has one.
- Save as to a user-provided path when the buffer is new or when explicitly requested.
- Warn before overwriting an existing file during save-as.
- Warn before quitting or creating a new file when unsaved changes exist.

### Editing

- Insert printable characters at the cursor.
- Insert newlines.
- Support backspace and forward delete behavior across line boundaries.
- Support deleting from cursor to end of line with line-join behavior.
- Support undo of prior edits through an undo stack.
- Support paste from the editor clipboard.

### Selection and clipboard

- Allow users to start a selection with `Ctrl-Space`.
- Treat selection as a line-based range between the anchor line and current cursor line.
- Copy the selected lines.
- Cut the selected lines.
- Replace the selected lines when pasting or typing over them.

### Navigation

- Move with arrow keys.
- Move to line start/end.
- Move by page.
- Jump to top or bottom of file.
- Jump directly to a requested line number.
- Keep the cursor and viewport synchronized while scrolling horizontally and vertically.

### Search

- Open a popup search dialog with `F3`.
- Search for a literal text string.
- Continue searching for the next hit with repeated `F3` or Enter in the search popup.
- Wrap to the top when the search reaches the end of the file.
- Move the cursor to the current match and highlight that match.

### Presentation

- Render the editor in a curses-based full-screen terminal UI.
- Show a status bar with line/column position, mode, filename, dirty marker, and transient status text.
- Support toggling line numbers.
- Support popup windows for help, confirmation, notices, search, go-to-line, open, and save-as interactions.

### C/C++-specific behavior

- Enable syntax coloring only for supported C/C++ file extensions:
  - `.c`
  - `.cc`
  - `.c++`
  - `.cpp`
  - `.h`
  - `.hpp`
- Enable auto-indentation for supported C/C++ files and new unnamed buffers.
- Reindent the whole file on demand.
- Adjust indentation of closing braces entered at the start of a line.

## Keyboard requirements

| Key | Behavior |
| --- | --- |
| `F1` | Show help |
| `F3` | Search / next hit |
| `F4` | Toggle line numbers |
| `F5` | Toggle color scheme |
| `F6` | Reindent whole file |
| `Ctrl-F` | Open file browser |
| `Ctrl-G` | Go to line |
| `Ctrl-N` | New file |
| `Ctrl-S` | Save |
| `Ctrl-W` | Save as |
| `Ctrl-Q` | Quit |
| `Ctrl-U` | Undo |
| `Ctrl-C` | Copy selected lines |
| `Ctrl-V` | Paste |
| `Ctrl-X` | Cut selected lines or kill to end of line |
| `Ctrl-D` | Delete character under cursor |
| `Ctrl-Space` | Start selection |
| `Esc` | Cancel selection or close popup |

## UX requirements

- The product must be fully keyboard-driven.
- The default startup state must enable color mode.
- The default startup state must hide line numbers.
- Status messages should explain the result of important actions such as open, save, copy, canceled flows, and search misses.
- Popups should be centered or intentionally positioned and should be dismissible without mouse input.

## Quality attributes

- Lightweight native executable built with `make`
- C99 source
- Terminal-first operation via `ncurses`
- Graceful behavior when color support is unavailable
- Preserve terminal settings on shutdown

## Known scope constraints from the current implementation

- Only one buffer is active at a time.
- Selection is line-oriented, not character-range-oriented.
- Search is case-sensitive and literal.
- Save-as uses a typed path popup rather than a navigable browser.
- No external clipboard integration is implemented.
