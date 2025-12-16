/**
 * @file jshell_register_externals.c
 * @brief Registration of external (non-builtin) shell commands
 */

#include "jshell_register_externals.h"

#include "apps/pkg/cmd_pkg.h"


/**
 * Register all external commands with the command registry.
 * Currently only registers the pkg command statically.
 * Other external commands (ls, cat, etc.) are registered dynamically
 * when their packages are installed via "pkg install".
 * This should be called during shell initialization, after builtins.
 */
void jshell_register_all_external_commands(void) {
  /* Only register pkg command statically. */
  /* Other external commands (ls, cat, etc.) are registered dynamically */
  /* when their packages are installed via "pkg install". */
  jshell_register_pkg_command();
}
