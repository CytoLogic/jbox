/**
 * @file ftpd_commands.h
 * @brief FTP command handler declarations.
 *
 * This module declares all FTP command handlers. Each handler
 * processes a specific FTP command and sends appropriate responses.
 */

#ifndef FTPD_COMMANDS_H
#define FTPD_COMMANDS_H

#include "ftpd.h"


/**
 * @brief Handle USER command - set username.
 *
 * Accepts any username without password authentication.
 * Sets the client's working directory to server_root/username.
 *
 * @param client Pointer to client structure.
 * @param arg Username argument (may be NULL or empty).
 * @return 0 to continue, -1 to disconnect.
 */
int ftpd_cmd_user(ftpd_client_t *client, const char *arg);


/**
 * @brief Handle QUIT command - terminate connection.
 *
 * Sends goodbye message and signals disconnect.
 *
 * @param client Pointer to client structure.
 * @param arg Unused argument.
 * @return -1 always (signals disconnect).
 */
int ftpd_cmd_quit(ftpd_client_t *client, const char *arg);


/**
 * @brief Handle PORT command - set data connection address.
 *
 * Parses the PORT argument in format "a1,a2,a3,a4,p1,p2" where
 * the IP is a1.a2.a3.a4 and port is p1*256+p2.
 * Server always connects to 127.0.0.1 regardless of IP provided.
 *
 * @param client Pointer to client structure.
 * @param arg PORT argument string.
 * @return 0 on success, -1 on error.
 */
int ftpd_cmd_port(ftpd_client_t *client, const char *arg);


/**
 * @brief Handle STOR command - upload file from client.
 *
 * Receives a file from the client over the data connection
 * and stores it in the client's current directory.
 * Requires prior PORT command.
 *
 * @param client Pointer to client structure.
 * @param arg Filename argument.
 * @return 0 on success, -1 on error.
 */
int ftpd_cmd_stor(ftpd_client_t *client, const char *arg);


/**
 * @brief Handle RETR command - download file to client.
 *
 * Sends a file to the client over the data connection.
 * Requires prior PORT command.
 *
 * @param client Pointer to client structure.
 * @param arg Filename argument.
 * @return 0 on success, -1 on error.
 */
int ftpd_cmd_retr(ftpd_client_t *client, const char *arg);


/**
 * @brief Handle LIST command - list directory contents.
 *
 * Sends an ls -l style directory listing over the data connection.
 * Requires prior PORT command.
 *
 * @param client Pointer to client structure.
 * @param arg Optional path argument (defaults to cwd).
 * @return 0 on success, -1 on error.
 */
int ftpd_cmd_list(ftpd_client_t *client, const char *arg);


/**
 * @brief Handle MKD command - create directory.
 *
 * Creates a new directory in the client's current directory.
 *
 * @param client Pointer to client structure.
 * @param arg Directory name argument.
 * @return 0 on success, -1 on error.
 */
int ftpd_cmd_mkd(ftpd_client_t *client, const char *arg);


/**
 * @brief Handle PWD command - print working directory.
 *
 * Sends the current working directory path to the client.
 *
 * @param client Pointer to client structure.
 * @param arg Unused argument.
 * @return 0 always.
 */
int ftpd_cmd_pwd(ftpd_client_t *client, const char *arg);


/**
 * @brief Handle CWD command - change working directory.
 *
 * Changes the client's current working directory.
 *
 * @param client Pointer to client structure.
 * @param arg Directory path argument.
 * @return 0 on success, -1 on error.
 */
int ftpd_cmd_cwd(ftpd_client_t *client, const char *arg);


/**
 * @brief Handle TYPE command - set transfer type.
 *
 * Accepts but ignores type setting (always binary).
 *
 * @param client Pointer to client structure.
 * @param arg Type argument (A or I).
 * @return 0 always.
 */
int ftpd_cmd_type(ftpd_client_t *client, const char *arg);


/**
 * @brief Handle SYST command - system type.
 *
 * Returns system type identifier.
 *
 * @param client Pointer to client structure.
 * @param arg Unused argument.
 * @return 0 always.
 */
int ftpd_cmd_syst(ftpd_client_t *client, const char *arg);


/**
 * @brief Handle NOOP command - no operation.
 *
 * Does nothing, just sends OK response.
 *
 * @param client Pointer to client structure.
 * @param arg Unused argument.
 * @return 0 always.
 */
int ftpd_cmd_noop(ftpd_client_t *client, const char *arg);


#endif /* FTPD_COMMANDS_H */
