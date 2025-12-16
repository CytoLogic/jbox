/**
 * @file date_main.c
 * @brief Standalone binary entry point for the date command.
 */

#include "cmd_date.h"

/**
 * Main entry point for the standalone date binary.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from the date command.
 */
int main(int argc, char **argv) {
  return cmd_date_spec.run(argc, argv);
}
