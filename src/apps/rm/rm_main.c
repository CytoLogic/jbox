/** @file rm_main.c
 *  @brief Standalone entry point for the rm command.
 */

#include "cmd_rm.h"


/**
 * @brief Main entry point for standalone rm binary.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from rm_run.
 */
int main(int argc, char **argv) {
  return cmd_rm_spec.run(argc, argv);
}
