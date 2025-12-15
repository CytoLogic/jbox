#ifndef JSHELL_CMD_REGISTRY_H
#define JSHELL_CMD_REGISTRY_H

#include <stdio.h>

typedef enum {
  CMD_BUILTIN,    // Run in shell process (call run function)
  CMD_EXTERNAL,   // Compiled into shell, call run function
  CMD_PACKAGE     // Fork/exec binary from ~/.jshell/bin/
} jshell_cmd_type_t;

typedef struct jshell_cmd_spec {
  const char *name;
  const char *summary;
  const char *long_help;
  jshell_cmd_type_t type;
  int (*run)(int argc, char **argv);   // NULL for CMD_PACKAGE
  void (*print_usage)(FILE *out);      // NULL for CMD_PACKAGE
  const char *bin_path;                // Path to binary for CMD_PACKAGE
} jshell_cmd_spec_t;

// Register a static command spec (builtins and externals)
void jshell_register_command(const jshell_cmd_spec_t *spec);

// Register a package command dynamically (allocates memory)
// Returns 0 on success, -1 on error
int jshell_register_package_command(const char *name, const char *summary,
                                    const char *bin_path);

// Unregister a command by name
// Returns 0 on success, -1 if not found
int jshell_unregister_command(const char *name);

// Unregister all package commands (CMD_PACKAGE type)
void jshell_unregister_all_package_commands(void);

// Find a command by name
const jshell_cmd_spec_t *jshell_find_command(const char *name);

// Iterate over all commands
void jshell_for_each_command(
  void (*callback)(const jshell_cmd_spec_t *spec, void *userdata),
  void *userdata);

// Get count of registered commands
int jshell_get_command_count(void);

#endif
