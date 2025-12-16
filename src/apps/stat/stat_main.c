/**
 * @file stat_main.c
 * @brief Main entry point for standalone stat command.
 */

#include "cmd_stat.h"


/**
 * Main entry point for standalone stat binary.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from stat_run.
 */
int main(int argc, char **argv) {
  return cmd_stat_spec.run(argc, argv);
}
