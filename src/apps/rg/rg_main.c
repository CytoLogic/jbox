/** @file rg_main.c
 *  @brief Main entry point for standalone rg binary
 */

#include "cmd_rg.h"


/**
 * Main function for standalone rg binary.
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status from rg_run
 */
int main(int argc, char **argv) {
  return cmd_rg_spec.run(argc, argv);
}
