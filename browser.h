#ifndef BROWSER_H
#define BROWSER_H

#include "cedit.h"

int browser_browse(const EditorState *editor, const char *initial_dir,
                   char *selected_path, size_t selected_size);
int browser_save_as(const EditorState *editor, const char *initial_dir,
                    char *selected_path, size_t selected_size);

#endif
