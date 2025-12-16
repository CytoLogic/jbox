/** @file vi_main.c
 *  @brief Main entry point for standalone vi binary
 */

#include "cmd_vi.h"


/**
 * Main function for standalone vi binary.
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status from vi_run
 */
int main(int argc, char **argv) {
  return cmd_vi_spec.run(argc, argv);
}
