#ifndef JSHELL_REGISTER_BUILTINS_H
#define JSHELL_REGISTER_BUILTINS_H


void jshell_register_all_builtin_commands(void);


void jshell_register_jobs_command(void);
void jshell_register_ps_command(void);
void jshell_register_kill_command(void);
void jshell_register_wait_command(void);
void jshell_register_edit_replace_line_command(void);
void jshell_register_edit_insert_line_command(void);
void jshell_register_edit_delete_line_command(void);
void jshell_register_edit_replace_command(void);
void jshell_register_cd_command(void);
void jshell_register_pwd_command(void);
void jshell_register_env_command(void);
void jshell_register_export_command(void);
void jshell_register_unset_command(void);
void jshell_register_type_command(void);
void jshell_register_help_command(void);


#endif
