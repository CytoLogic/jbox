#include "jshell_register_builtins.h"


void jshell_register_all_builtin_commands(void) {
  jshell_register_jobs_command();
  jshell_register_ps_command();
  jshell_register_kill_command();
  jshell_register_wait_command();
  jshell_register_edit_replace_line_command();
  jshell_register_edit_insert_line_command();
  jshell_register_edit_delete_line_command();
  jshell_register_edit_replace_command();
  jshell_register_cd_command();
  jshell_register_pwd_command();
  jshell_register_env_command();
  jshell_register_export_command();
  jshell_register_unset_command();
  jshell_register_type_command();
  jshell_register_help_command();
  jshell_register_history_command();
}
