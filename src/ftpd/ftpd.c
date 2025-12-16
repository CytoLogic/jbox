/**
 * @file ftpd.c
 * @brief FTP server core implementation.
 *
 * This module implements the main FTP server functionality including
 * socket setup, client acceptance, and lifecycle management.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ftpd.h"
#include "ftpd_client.h"


/**
 * @brief Add a client to the server's client list.
 *
 * Thread-safe addition to the linked list of clients.
 *
 * @param server Pointer to server structure.
 * @param client Pointer to client to add.
 */
static void add_client(ftpd_server_t *server, ftpd_client_t *client) {
  pthread_mutex_lock(&server->clients_lock);
  client->next = server->clients;
  server->clients = client;
  pthread_mutex_unlock(&server->clients_lock);
}


/**
 * @brief Remove a client from the server's client list.
 *
 * Thread-safe removal from the linked list of clients.
 *
 * @param server Pointer to server structure.
 * @param client Pointer to client to remove.
 */
static void remove_client(ftpd_server_t *server, ftpd_client_t *client) {
  pthread_mutex_lock(&server->clients_lock);

  ftpd_client_t **pp = &server->clients;
  while (*pp) {
    if (*pp == client) {
      *pp = client->next;
      break;
    }
    pp = &(*pp)->next;
  }

  pthread_mutex_unlock(&server->clients_lock);
}


/**
 * @brief Thread wrapper for client handler.
 *
 * Wraps the client handler to perform cleanup after disconnection.
 *
 * @param arg Pointer to ftpd_client_t structure.
 * @return NULL always.
 */
static void *client_thread_wrapper(void *arg) {
  ftpd_client_t *client = (ftpd_client_t *)arg;
  ftpd_server_t *server = client->server;

  /* Run the client handler */
  ftpd_client_handler(arg);

  /* Remove from client list and free */
  remove_client(server, client);
  free(client);

  return NULL;
}


/**
 * @brief Accept a new client connection.
 *
 * Accepts a connection, creates a client structure, and spawns
 * a handler thread.
 *
 * @param server Pointer to server structure.
 * @return 0 on success, -1 on error.
 */
static int accept_client(ftpd_server_t *server) {
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  int client_fd = accept(server->listen_fd,
                         (struct sockaddr *)&client_addr,
                         &addr_len);
  if (client_fd < 0) {
    if (errno == EINTR || errno == ECONNABORTED) {
      return 0;  /* Non-fatal, try again */
    }
    if (!server->running) {
      return 0;  /* Server shutting down */
    }
    perror("ftpd: accept");
    return -1;
  }

  /* Allocate client structure */
  ftpd_client_t *client = calloc(1, sizeof(ftpd_client_t));
  if (!client) {
    fprintf(stderr, "ftpd: out of memory for client\n");
    close(client_fd);
    return -1;
  }

  /* Initialize client */
  ftpd_client_init(client, client_fd, server);

  /* Add to client list */
  add_client(server, client);

  /* Spawn handler thread */
  int err = pthread_create(&client->thread, NULL,
                           client_thread_wrapper, client);
  if (err != 0) {
    fprintf(stderr, "ftpd: pthread_create: %s\n", strerror(err));
    remove_client(server, client);
    close(client_fd);
    free(client);
    return -1;
  }

  /* Detach thread so it cleans up automatically */
  pthread_detach(client->thread);

  char addr_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
  printf("ftpd: client connected from %s:%d\n",
         addr_str, ntohs(client_addr.sin_port));

  return 0;
}


int ftpd_init(ftpd_server_t *server, const ftpd_config_t *config) {
  if (!server || !config || !config->root_dir) {
    return -1;
  }

  memset(server, 0, sizeof(*server));
  server->listen_fd = -1;
  server->config = *config;
  server->running = false;

  /* Resolve root directory to absolute path */
  if (!realpath(config->root_dir, server->root_realpath)) {
    perror("ftpd: realpath");
    fprintf(stderr, "ftpd: cannot resolve root directory: %s\n",
            config->root_dir);
    return -1;
  }

  /* Initialize mutex */
  int err = pthread_mutex_init(&server->clients_lock, NULL);
  if (err != 0) {
    fprintf(stderr, "ftpd: pthread_mutex_init: %s\n", strerror(err));
    return -1;
  }

  return 0;
}


int ftpd_start(ftpd_server_t *server) {
  if (!server) {
    return -1;
  }

  /* Create listening socket */
  server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server->listen_fd < 0) {
    perror("ftpd: socket");
    return -1;
  }

  /* Allow address reuse */
  int optval = 1;
  if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR,
                 &optval, sizeof(optval)) < 0) {
    perror("ftpd: setsockopt SO_REUSEADDR");
  }

  /* Bind to port */
  struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = htonl(INADDR_ANY),
    .sin_port = htons(server->config.port)
  };

  if (bind(server->listen_fd, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    perror("ftpd: bind");
    close(server->listen_fd);
    server->listen_fd = -1;
    return -1;
  }

  /* Start listening */
  int max_clients = server->config.max_clients;
  if (max_clients <= 0) {
    max_clients = FTPD_MAX_CLIENTS;
  }

  if (listen(server->listen_fd, max_clients) < 0) {
    perror("ftpd: listen");
    close(server->listen_fd);
    server->listen_fd = -1;
    return -1;
  }

  printf("ftpd: listening on port %d\n", server->config.port);
  printf("ftpd: serving files from %s\n", server->root_realpath);

  server->running = true;

  /* Accept loop */
  while (server->running) {
    if (accept_client(server) < 0) {
      /* Fatal error in accept */
      break;
    }
  }

  return 0;
}


void ftpd_stop(ftpd_server_t *server) {
  if (!server) {
    return;
  }

  server->running = false;

  /* Close listening socket to interrupt accept() */
  if (server->listen_fd >= 0) {
    shutdown(server->listen_fd, SHUT_RDWR);
    close(server->listen_fd);
    server->listen_fd = -1;
  }
}


void ftpd_cleanup(ftpd_server_t *server) {
  if (!server) {
    return;
  }

  /* Close listening socket if still open */
  if (server->listen_fd >= 0) {
    close(server->listen_fd);
    server->listen_fd = -1;
  }

  /* Close all client connections */
  pthread_mutex_lock(&server->clients_lock);
  ftpd_client_t *client = server->clients;
  while (client) {
    ftpd_client_t *next = client->next;
    /* Close client socket to interrupt any blocking operations */
    if (client->ctrl_fd >= 0) {
      shutdown(client->ctrl_fd, SHUT_RDWR);
      close(client->ctrl_fd);
      client->ctrl_fd = -1;
    }
    client = next;
  }
  pthread_mutex_unlock(&server->clients_lock);

  /* Give threads a moment to exit */
  usleep(100000);  /* 100ms */

  /* Destroy mutex */
  pthread_mutex_destroy(&server->clients_lock);

  printf("ftpd: server stopped\n");
}
