/** @file rmdir_main.c
 *  @brief Standalone entry point for the rmdir command.
 */

#include "cmd_rmdir.h"


/**
 * @brief Main entry point for standalone rmdir binary.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from rmdir_run.
 */
int main(int argc, char **argv) {
  return cmd_rmdir_spec.run(argc, argv);
}
