#include "util.h"

char * cedit_strdup(const char *text){
  size_t length;
  char *copy;

  if (text == NULL){
      return NULL;
    }

  length = strlen(text);
  copy = malloc(length + 1);
  if (copy == NULL){
      return NULL;
    }

  memcpy(copy, text, length + 1);
  return copy;
}

void cedit_trim(char *text){
  char *start;
  char *end;
  size_t length;

  if (text == NULL || *text == '\0'){
      return;
    }

  start = text;
  while (*start != '\0' && isspace((unsigned char) *start)){
      start++;
    }

  if (start != text){
      memmove(text, start, strlen(start) + 1);
    }

  length = strlen(text);
  if (length == 0){
      return;
    }

  end = text + length - 1;
  while (end >= text && isspace((unsigned char) *end)){
      *end = '\0';
      if (end == text){
          break;
        }
      end--;
    }
}

int cedit_file_exists(const char *path){
  struct stat info;

  return path != NULL && stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

int cedit_dir_exists(const char *path){
  struct stat info;

  return path != NULL && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

char * cedit_join_path(const char *dir, const char *name){
  size_t dir_length;
  size_t name_length;
  int need_slash;
  char *path;

  if (dir == NULL || name == NULL){
      return NULL;
    }

  dir_length = strlen(dir);
  name_length = strlen(name);
  need_slash = dir_length > 0 && dir[dir_length - 1] != '/';

  path = malloc(dir_length + name_length + (size_t) need_slash + 1);
  if (path == NULL){
      return NULL;
    }

  memcpy(path, dir, dir_length);
  if (need_slash){
      path[dir_length] = '/';
      memcpy(path + dir_length + 1, name, name_length + 1);
    }
  else{
      memcpy(path + dir_length, name, name_length + 1);
    }

  return path;
}

void cedit_parent_dir(char *path){
  size_t length;
  char *last_slash;

  if (path == NULL){
      return;
    }

  length = strlen(path);
  while (length > 1 && path[length - 1] == '/'){
      path[length - 1] = '\0';
      length--;
    }

  last_slash = strrchr(path, '/');
  if (last_slash == NULL){
      strcpy(path, ".");
    }
  else if (last_slash == path){
      path[1] = '\0';
    }
  else{
      *last_slash = '\0';
    }
}

const char * cedit_extension(const char *path){
  const char *name;
  const char *dot;

  if (path == NULL){
      return NULL;
    }

  name = strrchr(path, '/');
  name = name == NULL ? path : name + 1;
  dot = strrchr(name, '.');
  if (dot == NULL || dot == name){
      return NULL;
    }

  return dot;
}

int cedit_supports_path(const char *path){
  const char *extension;

  extension = cedit_extension(path);
  if (extension == NULL){
      return 0;
    }

  return strcmp(extension, ".c") == 0
         || strcmp(extension, ".cc") == 0
         || strcmp(extension, ".c++") == 0
         || strcmp(extension, ".cpp") == 0
         || strcmp(extension, ".h") == 0
         || strcmp(extension, ".hpp") == 0;
}

char * cedit_basename_no_ext(const char *path){
  const char *name;
  const char *dot;
  size_t length;
  char *base;

  if (path == NULL){
      return cedit_strdup("cedit-out");
    }

  name = strrchr(path, '/');
  name = name == NULL ? path : name + 1;
  dot = strrchr(name, '.');
  if (dot == NULL || dot == name){
      return cedit_strdup(name);
    }

  length = (size_t) (dot - name);
  base = malloc(length + 1);
  if (base == NULL){
      return NULL;
    }

  memcpy(base, name, length);
  base[length] = '\0';
  return base;
}

char * cedit_dirname_dup(const char *path){
  const char *slash;
  size_t length;
  char *dir;

  if (path == NULL || *path == '\0'){
      return cedit_strdup(".");
    }

  slash = strrchr(path, '/');
  if (slash == NULL){
      return cedit_strdup(".");
    }

  if (slash == path){
      return cedit_strdup("/");
    }

  length = (size_t) (slash - path);
  dir = malloc(length + 1);
  if (dir == NULL){
      return NULL;
    }

  memcpy(dir, path, length);
  dir[length] = '\0';
  return dir;
}
