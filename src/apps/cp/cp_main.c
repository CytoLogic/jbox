/**
 * @file cp_main.c
 * @brief Standalone binary entry point for the cp command.
 */

#include "cmd_cp.h"

/**
 * Main entry point for the standalone cp binary.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from the cp command.
 */
int main(int argc, char **argv) {
  return cmd_cp_spec.run(argc, argv);
}
