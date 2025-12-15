#include "jshell_register_builtins.h"


void jshell_register_all_builtin_commands(void) {
  jshell_register_jobs_command();
  jshell_register_ls_command();
}
