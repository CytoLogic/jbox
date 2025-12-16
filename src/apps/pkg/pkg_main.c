/** @file pkg_main.c
 *  @brief Main entry point for standalone pkg binary.
 */

#include "cmd_pkg.h"

/** Main entry point for standalone pkg binary.
 *  @param argc Argument count
 *  @param argv Argument vector
 *  @return Exit status code from pkg_run
 */
int main(int argc, char **argv) {
  return cmd_pkg_spec.run(argc, argv);
}
