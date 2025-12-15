#include "jshell_register_externals.h"

#include "apps/ls/cmd_ls.h"
#include "apps/stat/cmd_stat.h"
#include "apps/cat/cmd_cat.h"


void jshell_register_all_external_commands(void) {
  jshell_register_ls_command();
  jshell_register_stat_command();
  jshell_register_cat_command();
}
