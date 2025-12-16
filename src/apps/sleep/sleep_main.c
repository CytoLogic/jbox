/**
 * @file sleep_main.c
 * @brief Main entry point for standalone sleep command.
 */

#include "cmd_sleep.h"


/**
 * Main entry point for standalone sleep binary.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from sleep_run.
 */
int main(int argc, char **argv) {
  return cmd_sleep_spec.run(argc, argv);
}
