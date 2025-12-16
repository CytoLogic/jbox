#include "jshell_register_externals.h"

#include "apps/pkg/cmd_pkg.h"


void jshell_register_all_external_commands(void) {
  // Only register pkg command statically.
  // Other external commands (ls, cat, etc.) are registered dynamically
  // when their packages are installed via "pkg install".
  jshell_register_pkg_command();
}
