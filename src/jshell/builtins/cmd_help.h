#ifndef CMD_HELP_H
#define CMD_HELP_H

#include "jshell/jshell_cmd_registry.h"

extern const jshell_cmd_spec_t cmd_help_spec;

void jshell_register_help_command(void);

#endif
