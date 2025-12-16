/**
 * @file touch_main.c
 * @brief Main entry point for standalone touch command.
 */

#include "cmd_touch.h"


/**
 * Main entry point for standalone touch binary.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from touch_run.
 */
int main(int argc, char **argv) {
  return cmd_touch_spec.run(argc, argv);
}
