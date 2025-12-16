/** @file mv_main.c
 *  @brief Standalone entry point for the mv command.
 */

#include "cmd_mv.h"


/**
 * @brief Main entry point for standalone mv binary.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from mv_run.
 */
int main(int argc, char **argv) {
  return cmd_mv_spec.run(argc, argv);
}
