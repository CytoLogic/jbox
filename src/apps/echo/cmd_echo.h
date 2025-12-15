#ifndef CMD_ECHO_H
#define CMD_ECHO_H

#include "jshell/jshell_cmd_registry.h"

extern const jshell_cmd_spec_t cmd_echo_spec;

void jshell_register_echo_command(void);

#endif
