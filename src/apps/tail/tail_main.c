/**
 * @file tail_main.c
 * @brief Main entry point for standalone tail command.
 */

#include "cmd_tail.h"


/**
 * Main entry point for standalone tail binary.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from tail_run.
 */
int main(int argc, char **argv) {
  return cmd_tail_spec.run(argc, argv);
}
