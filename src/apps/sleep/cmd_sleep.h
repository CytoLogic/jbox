#ifndef CMD_SLEEP_H
#define CMD_SLEEP_H

#include "jshell/jshell_cmd_registry.h"

extern const jshell_cmd_spec_t cmd_sleep_spec;

void jshell_register_sleep_command(void);

#endif
