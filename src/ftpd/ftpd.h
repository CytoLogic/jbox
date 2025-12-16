/**
 * @file ftpd.h
 * @brief FTP server main header - public API for the jbox FTP daemon.
 *
 * This module provides the core FTP server functionality including
 * initialization, startup, shutdown, and cleanup operations.
 */

#ifndef FTPD_H
#define FTPD_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>

/** Default port for the FTP server. */
#define FTPD_DEFAULT_PORT 21021

/** Maximum number of simultaneous client connections. */
#define FTPD_MAX_CLIENTS 64

/** Size of read/write buffers for data transfers. */
#define FTPD_BUFFER_SIZE 4096

/** Maximum length of an FTP command line. */
#define FTPD_CMD_MAX 512

/** Maximum length of a username. */
#define FTPD_USERNAME_MAX 64


/**
 * @brief Server configuration structure.
 *
 * Contains all configurable parameters for the FTP server.
 */
typedef struct ftpd_config {
  uint16_t port;          /**< Port to listen on. */
  const char *root_dir;   /**< Root directory for FTP files. */
  int max_clients;        /**< Maximum simultaneous clients. */
} ftpd_config_t;


/**
 * @brief Client session state.
 *
 * Represents a connected FTP client with all associated state.
 */
typedef struct ftpd_client {
  int ctrl_fd;                      /**< Control connection socket. */
  int data_fd;                      /**< Active data connection socket. */
  uint32_t data_addr;               /**< Client data IP (network order). */
  uint16_t data_port;               /**< Client data port (host order). */
  char username[FTPD_USERNAME_MAX]; /**< Authenticated username. */
  char cwd[PATH_MAX];               /**< Current working directory. */
  bool authenticated;               /**< Whether USER command succeeded. */
  bool data_port_set;               /**< Whether PORT command was received. */
  pthread_t thread;                 /**< Client handler thread. */
  struct ftpd_server *server;       /**< Back-pointer to server. */
  struct ftpd_client *next;         /**< Next client in linked list. */
} ftpd_client_t;


/**
 * @brief FTP server state.
 *
 * Main server structure containing all server state and client list.
 */
typedef struct ftpd_server {
  int listen_fd;                /**< Listening socket. */
  ftpd_config_t config;         /**< Server configuration. */
  ftpd_client_t *clients;       /**< Linked list of connected clients. */
  pthread_mutex_t clients_lock; /**< Mutex for client list access. */
  volatile bool running;        /**< Server running flag. */
  char root_realpath[PATH_MAX]; /**< Resolved absolute root path. */
} ftpd_server_t;


/**
 * @brief Initialize the FTP server.
 *
 * Sets up server structures and validates configuration.
 * Does not start listening for connections.
 *
 * @param server Pointer to server structure to initialize.
 * @param config Pointer to configuration (copied internally).
 * @return 0 on success, -1 on error.
 */
int ftpd_init(ftpd_server_t *server, const ftpd_config_t *config);


/**
 * @brief Start the FTP server.
 *
 * Creates the listening socket and enters the accept loop.
 * This function blocks until ftpd_stop() is called.
 *
 * @param server Pointer to initialized server.
 * @return 0 on normal shutdown, -1 on error.
 */
int ftpd_start(ftpd_server_t *server);


/**
 * @brief Signal the server to stop.
 *
 * Sets the running flag to false and closes the listening socket
 * to interrupt the accept loop. Safe to call from signal handlers.
 *
 * @param server Pointer to running server.
 */
void ftpd_stop(ftpd_server_t *server);


/**
 * @brief Clean up server resources.
 *
 * Waits for all client threads to finish and frees resources.
 * Should be called after ftpd_start() returns.
 *
 * @param server Pointer to server to clean up.
 */
void ftpd_cleanup(ftpd_server_t *server);


#endif /* FTPD_H */
