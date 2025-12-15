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


#endif
