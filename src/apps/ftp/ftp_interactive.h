/**
 * @file ftp_interactive.h
 * @brief FTP client interactive mode.
 *
 * Provides an interactive command-line interface for the FTP client.
 */

#ifndef FTP_INTERACTIVE_H
#define FTP_INTERACTIVE_H

#include "ftp_client.h"
#include <stdbool.h>


/**
 * @brief Run the FTP client in interactive mode.
 *
 * Reads commands from stdin and executes them until quit.
 * Supported commands:
 * - ls [path]      - List directory contents
 * - cd <path>      - Change directory
 * - pwd            - Print working directory
 * - get <remote> [local] - Download file
 * - put <local> [remote] - Upload file
 * - mkdir <dir>    - Create directory
 * - help           - Show available commands
 * - quit/exit      - Disconnect and exit
 *
 * @param session Pointer to connected and logged-in session.
 * @param json_output If true, output results in JSON format.
 * @return 0 on normal exit, -1 on error.
 */
int ftp_interactive(ftp_session_t *session, bool json_output);


#endif /* FTP_INTERACTIVE_H */
