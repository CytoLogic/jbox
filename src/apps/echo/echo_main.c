/**
 * @file echo_main.c
 * @brief Standalone binary entry point for the echo command.
 */

#include "cmd_echo.h"

/**
 * Main entry point for the standalone echo binary.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from the echo command.
 */
int main(int argc, char **argv) {
  return cmd_echo_spec.run(argc, argv);
}
