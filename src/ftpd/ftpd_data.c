/**
 * @file ftpd_data.c
 * @brief FTP data connection management implementation.
 *
 * This module handles the data connection for file transfers.
 * In active mode (PORT), the server connects to the client's data port.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ftpd.h"
#include "ftpd_data.h"


int ftpd_data_connect(ftpd_client_t *client) {
  if (!client || !client->data_port_set) {
    return -1;
  }

  /* Create socket */
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("ftpd: data socket");
    return -1;
  }

  /* Connect to client's data port */
  struct sockaddr_in data_addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = htonl(INADDR_LOOPBACK),  /* Always use 127.0.0.1 */
    .sin_port = htons(client->data_port)
  };

  if (connect(sock, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
    perror("ftpd: data connect");
    close(sock);
    return -1;
  }

  client->data_fd = sock;
  return 0;
}


ssize_t ftpd_data_send(ftpd_client_t *client, const void *data, size_t len) {
  if (!client || client->data_fd < 0 || !data) {
    return -1;
  }

  const char *ptr = (const char *)data;
  size_t remaining = len;

  while (remaining > 0) {
    ssize_t n = send(client->data_fd, ptr, remaining, 0);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    ptr += n;
    remaining -= (size_t)n;
  }

  return (ssize_t)len;
}


ssize_t ftpd_data_recv(ftpd_client_t *client, void *buf, size_t len) {
  if (!client || client->data_fd < 0 || !buf) {
    return -1;
  }

  ssize_t n;
  do {
    n = recv(client->data_fd, buf, len, 0);
  } while (n < 0 && errno == EINTR);

  return n;
}


int ftpd_data_send_file(ftpd_client_t *client, const char *filepath) {
  if (!client || !filepath || client->data_fd < 0) {
    return -1;
  }

  int fd = open(filepath, O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  char buf[FTPD_BUFFER_SIZE];
  ssize_t n;
  int result = 0;

  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    if (ftpd_data_send(client, buf, (size_t)n) < 0) {
      result = -1;
      break;
    }
  }

  if (n < 0) {
    result = -1;
  }

  close(fd);
  return result;
}


int ftpd_data_recv_file(ftpd_client_t *client, const char *filepath) {
  if (!client || !filepath || client->data_fd < 0) {
    return -1;
  }

  int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    return -1;
  }

  char buf[FTPD_BUFFER_SIZE];
  ssize_t n;
  int result = 0;

  while ((n = ftpd_data_recv(client, buf, sizeof(buf))) > 0) {
    ssize_t written = 0;
    while (written < n) {
      ssize_t w = write(fd, buf + written, (size_t)(n - written));
      if (w < 0) {
        if (errno == EINTR) {
          continue;
        }
        result = -1;
        goto done;
      }
      written += w;
    }
  }

  if (n < 0) {
    result = -1;
  }

done:
  close(fd);
  return result;
}


void ftpd_data_close(ftpd_client_t *client) {
  if (!client) {
    return;
  }

  if (client->data_fd >= 0) {
    close(client->data_fd);
    client->data_fd = -1;
  }

  /* Reset port setting after data transfer */
  client->data_port_set = false;
}
