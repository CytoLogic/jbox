/**
 * @file jshell_cmd_registry.c
 * @brief Command registry for tracking and managing shell commands
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "jshell_cmd_registry.h"

#define MAX_COMMANDS 128

/** Registry entry that tracks whether the spec was dynamically allocated */
typedef struct {
  const jshell_cmd_spec_t *spec;
  int is_dynamic;  /* 1 if spec was allocated by register_package_command */
} RegistryEntry;

/** Array of registered commands */
static RegistryEntry command_registry[MAX_COMMANDS];

/** Number of currently registered commands */
static size_t command_count;


/**
 * Register a command specification in the registry.
 * The spec should be statically allocated and have lifetime for the entire
 * program execution.
 * @param spec Pointer to command specification (must not be NULL)
 */
void jshell_register_command(const jshell_cmd_spec_t *spec) {
  if (spec == NULL) {
    return;
  }
  if (command_count < MAX_COMMANDS) {
    command_registry[command_count].spec = spec;
    command_registry[command_count].is_dynamic = 0;
    command_count++;
  }
}


/**
 * Register a command from an installed package.
 * Creates a dynamically allocated command spec that will be freed when
 * the command is unregistered. Fails if the command name already exists.
 * @param name Name of the command (must not be NULL)
 * @param summary One-line description of the command (can be NULL)
 * @param bin_path Full path to the executable binary (must not be NULL)
 * @return 0 on success, -1 on failure (NULL params, registry full, or
 *         command already exists)
 */
int jshell_register_package_command(const char *name, const char *summary,
                                    const char *bin_path) {
  if (name == NULL || bin_path == NULL) {
    return -1;
  }

  if (command_count >= MAX_COMMANDS) {
    return -1;
  }

  // Check if command already exists
  for (size_t i = 0; i < command_count; i++) {
    if (command_registry[i].spec != NULL &&
        command_registry[i].spec->name != NULL &&
        strcmp(command_registry[i].spec->name, name) == 0) {
      return -1;  // Already registered
    }
  }

  // Allocate new spec
  jshell_cmd_spec_t *spec = calloc(1, sizeof(jshell_cmd_spec_t));
  if (spec == NULL) {
    return -1;
  }

  spec->name = strdup(name);
  spec->summary = summary ? strdup(summary) : NULL;
  spec->long_help = NULL;
  spec->type = CMD_PACKAGE;
  spec->run = NULL;
  spec->print_usage = NULL;
  spec->bin_path = strdup(bin_path);

  if (spec->name == NULL || spec->bin_path == NULL) {
    free((void *)spec->name);
    free((void *)spec->summary);
    free((void *)spec->bin_path);
    free(spec);
    return -1;
  }

  command_registry[command_count].spec = spec;
  command_registry[command_count].is_dynamic = 1;
  command_count++;

  return 0;
}


/**
 * Free a dynamically allocated command specification.
 * Frees all string fields and the spec structure itself.
 * @param spec Pointer to command spec to free (can be NULL)
 */
static void free_dynamic_spec(jshell_cmd_spec_t *spec) {
  if (spec == NULL) {
    return;
  }
  free((void *)spec->name);
  free((void *)spec->summary);
  free((void *)spec->long_help);
  free((void *)spec->bin_path);
  free(spec);
}


/**
 * Unregister a command by name.
 * If the command was dynamically allocated (package command), frees the spec.
 * Compacts the registry by shifting remaining entries.
 * @param name Name of the command to unregister (must not be NULL)
 * @return 0 on success, -1 if command not found or name is NULL
 */
int jshell_unregister_command(const char *name) {
  if (name == NULL) {
    return -1;
  }

  for (size_t i = 0; i < command_count; i++) {
    if (command_registry[i].spec != NULL &&
        command_registry[i].spec->name != NULL &&
        strcmp(command_registry[i].spec->name, name) == 0) {

      // Free if dynamically allocated
      if (command_registry[i].is_dynamic) {
        free_dynamic_spec((jshell_cmd_spec_t *)command_registry[i].spec);
      }

      // Shift remaining entries
      for (size_t j = i; j < command_count - 1; j++) {
        command_registry[j] = command_registry[j + 1];
      }
      command_count--;

      return 0;
    }
  }

  return -1;
}


/**
 * Unregister all package commands from the registry.
 * Frees dynamically allocated specs for package commands and compacts
 * the registry, preserving builtin and external commands.
 */
void jshell_unregister_all_package_commands(void) {
  size_t write_idx = 0;

  for (size_t read_idx = 0; read_idx < command_count; read_idx++) {
    if (command_registry[read_idx].spec != NULL &&
        command_registry[read_idx].spec->type == CMD_PACKAGE) {
      // Free and skip this entry
      if (command_registry[read_idx].is_dynamic) {
        free_dynamic_spec((jshell_cmd_spec_t *)command_registry[read_idx].spec);
      }
    } else {
      // Keep this entry
      if (write_idx != read_idx) {
        command_registry[write_idx] = command_registry[read_idx];
      }
      write_idx++;
    }
  }

  command_count = write_idx;
}


/**
 * Find a command specification by name.
 * Performs linear search through the registry.
 * @param name Name of the command to find (must not be NULL)
 * @return Pointer to command spec if found, NULL otherwise
 */
const jshell_cmd_spec_t *jshell_find_command(const char *name) {
  if (name == NULL) {
    return NULL;
  }
  for (size_t index = 0; index < command_count; ++index) {
    const jshell_cmd_spec_t *spec = command_registry[index].spec;
    if (spec != NULL && spec->name != NULL &&
        strcmp(spec->name, name) == 0) {
      return spec;
    }
  }
  return NULL;
}


/**
 * Iterate over all registered commands and invoke callback for each.
 * Commands are visited in registration order.
 * @param callback Function to call for each command (must not be NULL)
 * @param userdata User-defined data to pass to the callback
 */
void jshell_for_each_command(
    void (*callback)(const jshell_cmd_spec_t *spec, void *userdata),
    void *userdata) {
  if (callback == NULL) {
    return;
  }
  for (size_t index = 0; index < command_count; ++index) {
    const jshell_cmd_spec_t *spec = command_registry[index].spec;
    if (spec != NULL) {
      callback(spec, userdata);
    }
  }
}


/**
 * Get the total number of registered commands.
 * @return Count of commands in the registry
 */
int jshell_get_command_count(void) {
  return (int)command_count;
}
