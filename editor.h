#ifndef EDITOR_H
#define EDITOR_H

#include "cedit.h"

void editor_init(EditorState *editor);
void editor_open(EditorState *editor, const char *path);
void editor_run(EditorState *editor);
void editor_shutdown(EditorState *editor);

#endif
