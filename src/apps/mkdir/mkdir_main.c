/** @file mkdir_main.c
 *  @brief Standalone entry point for the mkdir command.
 */

#include "cmd_mkdir.h"


/**
 * @brief Main entry point for standalone mkdir binary.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from mkdir_run.
 */
int main(int argc, char **argv) {
  return cmd_mkdir_spec.run(argc, argv);
}
