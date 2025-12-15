#ifndef CMD_ENV_H
#define CMD_ENV_H

#include "jshell/jshell_cmd_registry.h"


extern const jshell_cmd_spec_t cmd_env_spec;

void jshell_register_env_command(void);


#endif
