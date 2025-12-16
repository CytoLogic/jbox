/**
 * @file ftp_client.h
 * @brief FTP client session management.
 *
 * This module provides the core FTP client functionality including
 * connection management, authentication, and file transfer operations.
 */

#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** Maximum length of an FTP response line. */
#define FTP_RESPONSE_MAX 512

/** Buffer size for file transfers. */
#define FTP_BUFFER_SIZE 4096


/**
 * @brief FTP client session state.
 *
 * Represents a connection to an FTP server with all associated state.
 */
typedef struct {
  int ctrl_fd;                        /**< Control connection socket. */
  int data_listen_fd;                 /**< Data port listening socket. */
  uint16_t data_port;                 /**< Data port number. */
  char last_response[FTP_RESPONSE_MAX]; /**< Last response from server. */
  int last_code;                      /**< Last response code. */
  bool connected;                     /**< Whether connected to server. */
  bool logged_in;                     /**< Whether logged in. */
} ftp_session_t;


/**
 * @brief Initialize an FTP session structure.
 *
 * @param session Pointer to session structure to initialize.
 */
void ftp_session_init(ftp_session_t *session);


/**
 * @brief Connect to an FTP server.
 *
 * Establishes a control connection to the specified host and port.
 * Reads and stores the welcome message.
 *
 * @param session Pointer to session structure.
 * @param host Server hostname or IP address.
 * @param port Server port number.
 * @return 0 on success, -1 on error.
 */
int ftp_connect(ftp_session_t *session, const char *host, uint16_t port);


/**
 * @brief Log in to the FTP server.
 *
 * Sends USER command to authenticate with the server.
 *
 * @param session Pointer to connected session.
 * @param username Username to log in as.
 * @return 0 on success (230 response), -1 on error.
 */
int ftp_login(ftp_session_t *session, const char *username);


/**
 * @brief Disconnect from the FTP server.
 *
 * Sends QUIT command and closes the control connection.
 *
 * @param session Pointer to session.
 * @return 0 on success, -1 on error.
 */
int ftp_quit(ftp_session_t *session);


/**
 * @brief Close the session without sending QUIT.
 *
 * Closes all open sockets and resets session state.
 *
 * @param session Pointer to session.
 */
void ftp_close(ftp_session_t *session);


/**
 * @brief Get directory listing from server.
 *
 * Retrieves an ls -l style directory listing from the server.
 *
 * @param session Pointer to logged-in session.
 * @param path Optional path to list (NULL for current directory).
 * @param output Pointer to store allocated listing string. Caller must free.
 * @return 0 on success, -1 on error.
 */
int ftp_list(ftp_session_t *session, const char *path, char **output);


/**
 * @brief Download a file from the server.
 *
 * Retrieves a file from the server and saves it locally.
 *
 * @param session Pointer to logged-in session.
 * @param remote Remote filename to download.
 * @param local Local filename to save to (NULL to use remote name).
 * @return 0 on success, -1 on error.
 */
int ftp_get(ftp_session_t *session, const char *remote, const char *local);


/**
 * @brief Upload a file to the server.
 *
 * Sends a local file to the server.
 *
 * @param session Pointer to logged-in session.
 * @param local Local filename to upload.
 * @param remote Remote filename to save as (NULL to use local name).
 * @return 0 on success, -1 on error.
 */
int ftp_put(ftp_session_t *session, const char *local, const char *remote);


/**
 * @brief Create a directory on the server.
 *
 * @param session Pointer to logged-in session.
 * @param dirname Directory name to create.
 * @return 0 on success, -1 on error.
 */
int ftp_mkdir(ftp_session_t *session, const char *dirname);


/**
 * @brief Get current working directory.
 *
 * @param session Pointer to logged-in session.
 * @param path Buffer to store path.
 * @param pathsize Size of path buffer.
 * @return 0 on success, -1 on error.
 */
int ftp_pwd(ftp_session_t *session, char *path, size_t pathsize);


/**
 * @brief Change working directory.
 *
 * @param session Pointer to logged-in session.
 * @param path Directory to change to.
 * @return 0 on success, -1 on error.
 */
int ftp_cd(ftp_session_t *session, const char *path);


/**
 * @brief Send a raw FTP command.
 *
 * Sends the command and reads the response.
 *
 * @param session Pointer to session.
 * @param cmd Command string (without CRLF).
 * @return Response code, or -1 on error.
 */
int ftp_command(ftp_session_t *session, const char *cmd);


/**
 * @brief Get the last response message.
 *
 * @param session Pointer to session.
 * @return Pointer to last response string.
 */
const char *ftp_last_response(ftp_session_t *session);


/**
 * @brief Get the last response code.
 *
 * @param session Pointer to session.
 * @return Last response code, or -1 if none.
 */
int ftp_last_code(ftp_session_t *session);


#endif /* FTP_CLIENT_H */
