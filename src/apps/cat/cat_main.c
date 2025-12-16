/**
 * @file cat_main.c
 * @brief Standalone binary entry point for the cat command.
 */

#include "cmd_cat.h"

/**
 * Main entry point for the standalone cat binary.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from the cat command.
 */
int main(int argc, char **argv) {
  return cmd_cat_spec.run(argc, argv);
}
