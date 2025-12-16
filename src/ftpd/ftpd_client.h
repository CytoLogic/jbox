/**
 * @file ftpd_client.h
 * @brief FTP client session handler.
 *
 * This module handles individual client connections, including
 * command parsing and response generation.
 */

#ifndef FTPD_CLIENT_H
#define FTPD_CLIENT_H

#include "ftpd.h"
#include <stdio.h>


/**
 * @brief Client handler thread entry point.
 *
 * Main function for handling a single client connection.
 * Runs in its own thread and processes commands until
 * the client disconnects or QUIT is received.
 *
 * @param arg Pointer to ftpd_client_t structure.
 * @return NULL always.
 */
void *ftpd_client_handler(void *arg);


/**
 * @brief Read a command line from the control connection.
 *
 * Reads characters until CRLF is encountered or buffer is full.
 * The CRLF is stripped from the returned string.
 *
 * @param client Pointer to client structure.
 * @param buf Buffer to store the command.
 * @param bufsize Size of the buffer.
 * @return Number of bytes read, 0 on connection closed, -1 on error.
 */
int ftpd_read_command(ftpd_client_t *client, char *buf, size_t bufsize);


/**
 * @brief Send a response to the client.
 *
 * Formats and sends an FTP response with the given code and message.
 * Automatically appends CRLF.
 *
 * @param client Pointer to client structure.
 * @param code FTP response code (e.g., 200, 550).
 * @param message Response message text.
 * @return 0 on success, -1 on error.
 */
int ftpd_send_response(ftpd_client_t *client, int code, const char *message);


/**
 * @brief Send a formatted response to the client.
 *
 * Like ftpd_send_response but with printf-style formatting.
 *
 * @param client Pointer to client structure.
 * @param code FTP response code.
 * @param fmt Printf-style format string.
 * @param ... Format arguments.
 * @return 0 on success, -1 on error.
 */
int ftpd_send_response_fmt(ftpd_client_t *client, int code,
                           const char *fmt, ...);


/**
 * @brief Dispatch a command to the appropriate handler.
 *
 * Parses the command string and calls the corresponding
 * command handler function.
 *
 * @param client Pointer to client structure.
 * @param cmdline Full command line (command + arguments).
 * @return 0 to continue, -1 to disconnect client.
 */
int ftpd_dispatch_command(ftpd_client_t *client, const char *cmdline);


/**
 * @brief Initialize a new client structure.
 *
 * Sets up default values for a newly connected client.
 *
 * @param client Pointer to client structure to initialize.
 * @param ctrl_fd Control connection socket.
 * @param server Pointer to parent server.
 */
void ftpd_client_init(ftpd_client_t *client, int ctrl_fd,
                      ftpd_server_t *server);


/**
 * @brief Clean up client resources.
 *
 * Closes all open sockets and frees resources.
 *
 * @param client Pointer to client structure.
 */
void ftpd_client_cleanup(ftpd_client_t *client);


#endif /* FTPD_CLIENT_H */
