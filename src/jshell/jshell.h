#ifndef JSHELL_H
#define JSHELL_H

#include <stdio.h>


int jshell_main(int argc, char **argv);

int jshell_exec_string(const char *cmd_string);

void jshell_print_usage(FILE *out);

int jshell_get_last_exit_status(void);

void jshell_set_last_exit_status(int status);

#endif
