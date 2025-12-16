#ifndef JSHELL_ENV_LOADER_H
#define JSHELL_ENV_LOADER_H


// Load environment variables from ~/.jshell/env
// File format: VAR_NAME="value" (one per line)
// Lines starting with # are comments
// Empty lines are ignored
void jshell_load_env_file(void);


#endif
