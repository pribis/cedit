#ifndef UTIL_H
#define UTIL_H

#include "cedit.h"

char *cedit_strdup(const char *text);
void cedit_trim(char *text);
int cedit_file_exists(const char *path);
int cedit_dir_exists(const char *path);
char *cedit_join_path(const char *dir, const char *name);
void cedit_parent_dir(char *path);
const char *cedit_extension(const char *path);
int cedit_supports_path(const char *path);
char *cedit_basename_no_ext(const char *path);
char *cedit_dirname_dup(const char *path);

#endif
