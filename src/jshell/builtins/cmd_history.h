#ifndef CMD_HISTORY_H
#define CMD_HISTORY_H

#include "jshell/jshell_cmd_registry.h"

extern const jshell_cmd_spec_t cmd_history_spec;

void jshell_register_history_command(void);

#endif
