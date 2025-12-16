/**
 * @file ftp_main.c
 * @brief Main entry point for standalone FTP client binary.
 *
 * Provides a main() wrapper that delegates to cmd_ftp_spec.run().
 */

#include "cmd_ftp.h"


/**
 * @brief Main entry point.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status from ftp command.
 */
int main(int argc, char **argv) {
  return cmd_ftp_spec.run(argc, argv);
}
