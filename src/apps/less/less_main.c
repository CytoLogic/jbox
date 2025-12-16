/**
 * @file less_main.c
 * @brief Standalone binary entry point for the less command.
 */

#include "cmd_less.h"

/**
 * Main entry point for the standalone less binary.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from the less command.
 */
int main(int argc, char **argv) {
  return cmd_less_spec.run(argc, argv);
}
