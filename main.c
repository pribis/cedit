#include "editor.h"

int main(int argc, char **argv){
  EditorState editor;

  editor_init(&editor);
  if (argc > 1){
      editor_open(&editor, argv[1]);
    }
  editor_run(&editor);
  editor_shutdown(&editor);
  return 0;
}
