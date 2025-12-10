#include <jbox.h>
#include "jshell.h"


typedef int (*CmdFunc)(int, char *[]);

typedef struct {
    const char *name;
    CmdFunc cmd_func;
} CmdEntry;

// sort alpha for binary search
CmdEntry cmd_table[] = {
    {"cat", cat_main},
    {"cp", cp_main},
    {"echo", echo_main},
    {"ls", ls_main},
    {"mkdir", mkdir_main},
    {"mv", mv_main},
    {"pwd", pwd_main},
    {"rm", rm_main},
    {"stat", stat_main},
    {"touch", touch_main},
};



int cmd_cmp_func(const void *key, const void *cmd_entry) {
  return strcmp(key, ((CmdEntry *)cmd_entry)->name);
}

int main(int argc, char *argv[]) {
  char *cmd = argv[0];

  // get addr after slash
  char *after_slash = strrchr(cmd, '/') + 1;

  // move target substring forward
  memmove(cmd, after_slash, strlen(after_slash) + 1);

  if (strcmp(cmd, "jbox")==0) {
    printf("welcome to jbox!\n");
    jshell_main();
    exit(EXIT_SUCCESS);
  }

  // get cmd_func
  size_t cmd_entry_sz = sizeof(CmdEntry);
  size_t num_cmds = sizeof(cmd_table) / cmd_entry_sz;
  CmdEntry *cmd_entry = bsearch(cmd, cmd_table, num_cmds, cmd_entry_sz, cmd_cmp_func);

  if (cmd_entry == NULL) {
    printf("invalid cmd!\n");
    exit(EXIT_FAILURE);
  }

  cmd_entry->cmd_func(argc, argv);
}
