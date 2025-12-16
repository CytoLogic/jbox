/**
 * @file cmd_ftp.h
 * @brief FTP client command specification.
 *
 * Declares the command spec and registration function for the FTP client.
 */

#ifndef CMD_FTP_H
#define CMD_FTP_H

#include "jshell/jshell_cmd_registry.h"

/** FTP client command specification. */
extern const jshell_cmd_spec_t cmd_ftp_spec;

/**
 * @brief Register the ftp command with the shell.
 */
void jshell_register_ftp_command(void);

#endif /* CMD_FTP_H */
