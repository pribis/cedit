#include "buffer.h"
#include "util.h"

static int buffer_reserve(TextBuffer *buffer, int needed){
  int next_capacity;
  char **grown;

  if (buffer->capacity >= needed){
    return 1;
  }

  next_capacity = buffer->capacity == 0 ? 16 : buffer->capacity;
  while (next_capacity < needed){
    next_capacity *= 2;
  }

  grown = realloc(buffer->lines, (size_t) next_capacity * sizeof(*grown));
  if (grown == NULL){
    return 0;
  }

  buffer->lines = grown;
  buffer->capacity = next_capacity;
  return 1;
}

static char * buffer_substring(const char *source, size_t start, size_t length){
  char *result;

  result = malloc(length + 1);
  if (result == NULL){
    return NULL;
  }

  memcpy(result, source + start, length);
  result[length] = '\0';
  return result;
}

static int buffer_insert_line(TextBuffer *buffer, int at, char *line){
  int index;

  if (!buffer_reserve(buffer, buffer->line_count + 1)){
    free(line);
    return 0;
  }

  for (index = buffer->line_count; index > at; index--){
    buffer->lines[index] = buffer->lines[index - 1];
  }

  buffer->lines[at] = line;
  buffer->line_count++;
  buffer->dirty = 1;
  return 1;
}

void buffer_init(TextBuffer *buffer){
  memset(buffer, 0, sizeof(*buffer));
  buffer_reserve(buffer, 1);
  buffer->lines[0] = cedit_strdup("");
  buffer->line_count = 1;
}

void buffer_free(TextBuffer *buffer){
  int index;

  for (index = 0; index < buffer->line_count; index++){
    free(buffer->lines[index]);
  }

  free(buffer->lines);
  free(buffer->path);
  memset(buffer, 0, sizeof(*buffer));
}

int buffer_clone(TextBuffer *dest, const TextBuffer *src){
  int index;

  memset(dest, 0, sizeof(*dest));
  if (!buffer_reserve(dest, src->line_count > 0 ? src->line_count : 1)){
    return 0;
  }

  for (index = 0; index < src->line_count; index++){
    dest->lines[index] = cedit_strdup(src->lines[index]);
    if (dest->lines[index] == NULL){
      buffer_free(dest);
      return 0;
    }
  }

  dest->line_count = src->line_count;
  dest->dirty = src->dirty;
  dest->path = cedit_strdup(src->path);
  if (src->path != NULL && dest->path == NULL){
    buffer_free(dest);
    return 0;
  }

  return 1;
}

int buffer_load(TextBuffer *buffer, const char *path){
  FILE *input;
  char line[4096];
  TextBuffer replacement;

  input = fopen(path, "r");
  if (input == NULL){
    return 0;
  }

  memset(&replacement, 0, sizeof(replacement));
  if (!buffer_reserve(&replacement, 1)){
    fclose(input);
    return 0;
  }

  replacement.line_count = 0;
  while (fgets(line, sizeof(line), input) != NULL){
    size_t length;
    char *copy;

    length = strlen(line);
    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')){
      line[length - 1] = '\0';
      length--;
    }

    copy = cedit_strdup(line);
    if (copy == NULL || !buffer_insert_line(&replacement, replacement.line_count, copy)){
      fclose(input);
      buffer_free(&replacement);
      return 0;
    }
  }

  fclose(input);

  if (replacement.line_count == 0){
    if (!buffer_insert_line(&replacement, 0, cedit_strdup(""))){
      buffer_free(&replacement);
      return 0;
    }
  }

  replacement.dirty = 0;
  replacement.path = cedit_strdup(path);
  buffer_free(buffer);
  *buffer = replacement;
  return 1;
}

int buffer_save(TextBuffer *buffer, const char *path){
  FILE *output;
  int index;
  char *new_path;

  output = fopen(path, "w");
  if (output == NULL){
    return 0;
  }

  for (index = 0; index < buffer->line_count; index++){
    if (fputs(buffer->lines[index], output) == EOF){
      fclose(output);
      return 0;
    }

    if (index + 1 < buffer->line_count && fputc('\n', output) == EOF){
      fclose(output);
      return 0;
    }
  }

  if (fclose(output) != 0){
    return 0;
  }

  new_path = cedit_strdup(path);
  if (new_path == NULL){
    return 0;
  }

  free(buffer->path);
  buffer->path = new_path;
  buffer->dirty = 0;
  return 1;
}

void buffer_insert_char(TextBuffer *buffer, int row, int col, int ch){
  char *line;
  size_t length;
  char *grown;

  if (row < 0 || row >= buffer->line_count){
    return;
  }

  line = buffer->lines[row];
  length = strlen(line);
  if (col < 0){
    col = 0;
  }
  if ((size_t) col > length){
    col = (int) length;
  }

  grown = realloc(line, length + 2);
  if (grown == NULL){
    return;
  }

  memmove(grown + col + 1, grown + col, length - (size_t) col + 1);
  grown[col] = (char) ch;
  buffer->lines[row] = grown;
  buffer->dirty = 1;
}

void buffer_insert_newline(TextBuffer *buffer, int *row, int *col, int indent_spaces){
  char *line;
  size_t length;
  char *left;
  char *right;
  char *new_line;

  if (*row < 0 || *row >= buffer->line_count){
    return;
  }

  line = buffer->lines[*row];
  length = strlen(line);
  if ((size_t) *col > length){
    *col = (int) length;
  }

  left = buffer_substring(line, 0, (size_t) *col);
  right = buffer_substring(line, (size_t) *col, length - (size_t) *col);
  new_line = malloc((size_t) indent_spaces + strlen(right) + 1);
  if (left == NULL || right == NULL || new_line == NULL){
    free(left);
    free(right);
    free(new_line);
    return;
  }

  memset(new_line, ' ', (size_t) indent_spaces);
  strcpy(new_line + indent_spaces, right);

  free(buffer->lines[*row]);
  buffer->lines[*row] = left;
  if (!buffer_insert_line(buffer, *row + 1, new_line)){
    return;
  }

  free(right);
  *row += 1;
  *col = indent_spaces;
}

void buffer_backspace(TextBuffer *buffer, int *row, int *col){
  char *line;
  size_t length;
  char *previous;
  char *merged;
  size_t previous_length;
  int index;

  if (*row < 0 || *row >= buffer->line_count){
    return;
  }

  if (*col > 0){
    line = buffer->lines[*row];
    length = strlen(line);
    memmove(line + *col - 1, line + *col, length - (size_t) *col + 1);
    *col -= 1;
    buffer->dirty = 1;
    return;
  }

  if (*row == 0){
    return;
  }

  previous = buffer->lines[*row - 1];
  line = buffer->lines[*row];
  previous_length = strlen(previous);
  merged = realloc(previous, previous_length + strlen(line) + 1);
  if (merged == NULL){
    return;
  }

  strcpy(merged + previous_length, line);
  buffer->lines[*row - 1] = merged;
  free(line);

  for (index = *row; index < buffer->line_count - 1; index++){
    buffer->lines[index] = buffer->lines[index + 1];
  }

  buffer->line_count--;
  *row -= 1;
  *col = (int) previous_length;
  buffer->dirty = 1;
}

void buffer_delete(TextBuffer *buffer, int row, int col){
  char *line;
  size_t length;

  if (row < 0 || row >= buffer->line_count){
    return;
  }

  line = buffer->lines[row];
  length = strlen(line);

  if ((size_t) col < length){
    memmove(line + col, line + col + 1, length - (size_t) col);
    buffer->dirty = 1;
    return;
  }

  if (row + 1 < buffer->line_count){
    int next_row;
    int next_col;

    next_row = row + 1;
    next_col = 0;
    buffer_backspace(buffer, &next_row, &next_col);
  }
}

void buffer_kill_to_end(TextBuffer *buffer, int row, int col){
  char *line;
  size_t length;

  if (row < 0 || row >= buffer->line_count){
    return;
  }

  line = buffer->lines[row];
  length = strlen(line);
  if (col < 0){
    col = 0;
  }
  if ((size_t) col > length){
    col = (int) length;
  }

  if (row + 1 < buffer->line_count){
    char *next_line;
    char *merged;
    int index;

    next_line = buffer->lines[row + 1];
    merged = realloc(line, (size_t) col + strlen(next_line) + 1);
    if (merged == NULL){
      return;
    }

    strcpy(merged + col, next_line);
    buffer->lines[row] = merged;
    free(next_line);
    for (index = row + 1; index < buffer->line_count - 1; index++){
      buffer->lines[index] = buffer->lines[index + 1];
    }
    buffer->line_count--;
  }
  else{
    line[col] = '\0';
  }

  buffer->dirty = 1;
}

void buffer_indent_line(TextBuffer *buffer, int row, int *col, int spaces){
  int count;

  if (row < 0 || row >= buffer->line_count || spaces <= 0){
    return;
  }

  for (count = 0; count < spaces; count++){
    buffer_insert_char(buffer, row, *col, ' ');
    *col += 1;
  }
}

void buffer_set_line_indent(TextBuffer *buffer, int row, int *col, int spaces){
  char *line;
  int first_non_space;
  size_t suffix_length;
  char *updated;

  if (row < 0 || row >= buffer->line_count || spaces < 0){
    return;
  }

  line = buffer->lines[row];
  first_non_space = buffer_first_non_space(line);
  suffix_length = strlen(line + first_non_space);
  updated = malloc((size_t) spaces + suffix_length + 1);
  if (updated == NULL){
    return;
  }

  memset(updated, ' ', (size_t) spaces);
  memcpy(updated + spaces, line + first_non_space, suffix_length + 1);
  free(buffer->lines[row]);
  buffer->lines[row] = updated;
  buffer->dirty = 1;

  if (*col <= first_non_space){
    *col = spaces;
  }
  else{
    *col += spaces - first_non_space;
    if (*col < spaces){
      *col = spaces;
    }
  }
}

int buffer_first_non_space(const char *line){
  int index;

  if (line == NULL){
    return 0;
  }

  for (index = 0; line[index] != '\0'; index++){
    if (!isspace((unsigned char) line[index])){
      return index;
    }
  }

  return index;
}

int buffer_line_length(const TextBuffer *buffer, int row){
  if (row < 0 || row >= buffer->line_count){
    return 0;
  }

  return (int) strlen(buffer->lines[row]);
}