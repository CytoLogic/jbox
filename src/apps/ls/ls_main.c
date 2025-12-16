/**
 * @file ls_main.c
 * @brief Standalone binary entry point for the ls command.
 */

#include "cmd_ls.h"

/**
 * Main entry point for the standalone ls binary.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from the ls command.
 */
int main(int argc, char **argv) {
  return cmd_ls_spec.run(argc, argv);
}
