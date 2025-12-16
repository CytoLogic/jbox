/**
 * @file ftpd_data.h
 * @brief FTP data connection management.
 *
 * This module handles the data connection for file transfers.
 * The FTP protocol uses separate connections for control (commands)
 * and data (file transfers).
 */

#ifndef FTPD_DATA_H
#define FTPD_DATA_H

#include "ftpd.h"
#include <stddef.h>


/**
 * @brief Establish data connection to client.
 *
 * Connects to the client's data port as specified by a prior
 * PORT command. The connection is stored in client->data_fd.
 *
 * @param client Pointer to client structure with data_port set.
 * @return 0 on success, -1 on error.
 */
int ftpd_data_connect(ftpd_client_t *client);


/**
 * @brief Send data over the data connection.
 *
 * Writes data to the established data connection.
 *
 * @param client Pointer to client structure with data_fd set.
 * @param data Pointer to data buffer.
 * @param len Number of bytes to send.
 * @return Number of bytes sent, or -1 on error.
 */
ssize_t ftpd_data_send(ftpd_client_t *client, const void *data, size_t len);


/**
 * @brief Receive data from the data connection.
 *
 * Reads data from the established data connection.
 *
 * @param client Pointer to client structure with data_fd set.
 * @param buf Buffer to store received data.
 * @param len Maximum number of bytes to read.
 * @return Number of bytes received, 0 on EOF, or -1 on error.
 */
ssize_t ftpd_data_recv(ftpd_client_t *client, void *buf, size_t len);


/**
 * @brief Send a file over the data connection.
 *
 * Opens the specified file and sends its contents over
 * the data connection. The data connection must already
 * be established.
 *
 * @param client Pointer to client structure.
 * @param filepath Path to file to send.
 * @return 0 on success, -1 on error.
 */
int ftpd_data_send_file(ftpd_client_t *client, const char *filepath);


/**
 * @brief Receive a file over the data connection.
 *
 * Receives data from the data connection and writes it
 * to the specified file. Creates the file if it doesn't exist.
 *
 * @param client Pointer to client structure.
 * @param filepath Path to file to create/overwrite.
 * @return 0 on success, -1 on error.
 */
int ftpd_data_recv_file(ftpd_client_t *client, const char *filepath);


/**
 * @brief Close the data connection.
 *
 * Closes the data connection socket and resets data_fd to -1.
 *
 * @param client Pointer to client structure.
 */
void ftpd_data_close(ftpd_client_t *client);


#endif /* FTPD_DATA_H */
