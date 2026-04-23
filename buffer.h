#ifndef BUFFER_H
#define BUFFER_H

#include "cedit.h"

void buffer_init(TextBuffer *buffer);
void buffer_free(TextBuffer *buffer);
int buffer_clone(TextBuffer *dest, const TextBuffer *src);
int buffer_load(TextBuffer *buffer, const char *path);
int buffer_save(TextBuffer *buffer, const char *path);
void buffer_insert_char(TextBuffer *buffer, int row, int col, int ch);
void buffer_insert_newline(TextBuffer *buffer, int *row, int *col, int indent_spaces);
void buffer_backspace(TextBuffer *buffer, int *row, int *col);
void buffer_delete(TextBuffer *buffer, int row, int col);
void buffer_kill_to_end(TextBuffer *buffer, int row, int col);
void buffer_indent_line(TextBuffer *buffer, int row, int *col, int spaces);
void buffer_set_line_indent(TextBuffer *buffer, int row, int *col, int spaces);
int buffer_first_non_space(const char *line);
int buffer_line_length(const TextBuffer *buffer, int row);

#endif
