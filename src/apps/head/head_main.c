/**
 * @file head_main.c
 * @brief Standalone binary entry point for the head command.
 */

#include "cmd_head.h"

/**
 * Main entry point for the standalone head binary.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from the head command.
 */
int main(int argc, char **argv) {
  return cmd_head_spec.run(argc, argv);
}
