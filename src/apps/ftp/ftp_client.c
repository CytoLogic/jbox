/**
 * @file ftp_client.c
 * @brief FTP client session management implementation.
 *
 * This module implements the core FTP client functionality including
 * connection management, data transfers, and command execution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "ftp_client.h"


/**
 * @brief Read a response line from the control connection.
 *
 * @param session Pointer to session.
 * @return Response code, or -1 on error.
 */
static int read_response(ftp_session_t *session) {
  if (!session || session->ctrl_fd < 0) {
    return -1;
  }

  char *buf = session->last_response;
  size_t bufsize = sizeof(session->last_response);
  size_t pos = 0;

  while (pos < bufsize - 1) {
    char c;
    ssize_t n = recv(session->ctrl_fd, &c, 1, 0);

    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) {
      /* Connection closed */
      if (pos > 0) break;
      return -1;
    }

    if (c == '\n') {
      if (pos > 0 && buf[pos - 1] == '\r') {
        pos--;
      }
      break;
    }

    buf[pos++] = c;
  }

  buf[pos] = '\0';

  /* Parse response code */
  if (pos >= 3 && isdigit((unsigned char)buf[0]) &&
      isdigit((unsigned char)buf[1]) && isdigit((unsigned char)buf[2])) {
    session->last_code = (buf[0] - '0') * 100 +
                         (buf[1] - '0') * 10 +
                         (buf[2] - '0');
  } else {
    session->last_code = -1;
  }

  return session->last_code;
}


/**
 * @brief Send a command to the server.
 *
 * @param session Pointer to session.
 * @param cmd Command string (without CRLF).
 * @return 0 on success, -1 on error.
 */
static int send_command(ftp_session_t *session, const char *cmd) {
  if (!session || session->ctrl_fd < 0 || !cmd) {
    return -1;
  }

  size_t len = strlen(cmd);
  char *buf = malloc(len + 3);
  if (!buf) return -1;

  memcpy(buf, cmd, len);
  buf[len] = '\r';
  buf[len + 1] = '\n';
  buf[len + 2] = '\0';

  size_t total = len + 2;
  size_t sent = 0;

  while (sent < total) {
    ssize_t n = send(session->ctrl_fd, buf + sent, total - sent, 0);
    if (n < 0) {
      if (errno == EINTR) continue;
      free(buf);
      return -1;
    }
    sent += (size_t)n;
  }

  free(buf);
  return 0;
}


/**
 * @brief Set up a listening socket for data connection.
 *
 * Binds to an ephemeral port and starts listening.
 *
 * @param session Pointer to session.
 * @return 0 on success, -1 on error.
 */
static int setup_data_listener(ftp_session_t *session) {
  if (!session) return -1;

  /* Close any existing data listener */
  if (session->data_listen_fd >= 0) {
    close(session->data_listen_fd);
    session->data_listen_fd = -1;
  }

  /* Create socket */
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return -1;

  /* Allow address reuse */
  int optval = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  /* Bind to ephemeral port */
  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    .sin_port = 0
  };

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock);
    return -1;
  }

  /* Get assigned port */
  socklen_t addrlen = sizeof(addr);
  if (getsockname(sock, (struct sockaddr *)&addr, &addrlen) < 0) {
    close(sock);
    return -1;
  }

  session->data_port = ntohs(addr.sin_port);

  /* Start listening */
  if (listen(sock, 1) < 0) {
    close(sock);
    return -1;
  }

  session->data_listen_fd = sock;
  return 0;
}


/**
 * @brief Send PORT command to server.
 *
 * @param session Pointer to session with data_port set.
 * @return 0 on success (200 response), -1 on error.
 */
static int send_port_command(ftp_session_t *session) {
  if (!session || session->data_port == 0) return -1;

  char cmd[64];
  uint16_t port = session->data_port;
  snprintf(cmd, sizeof(cmd), "PORT 127,0,0,1,%u,%u",
           (port >> 8) & 0xFF, port & 0xFF);

  if (send_command(session, cmd) < 0) return -1;

  int code = read_response(session);
  return (code == 200) ? 0 : -1;
}


/**
 * @brief Accept incoming data connection.
 *
 * @param session Pointer to session with data_listen_fd set.
 * @return Data socket fd, or -1 on error.
 */
static int accept_data_connection(ftp_session_t *session) {
  if (!session || session->data_listen_fd < 0) return -1;

  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  int data_fd = accept(session->data_listen_fd,
                       (struct sockaddr *)&addr, &addrlen);

  /* Close listener after accepting */
  close(session->data_listen_fd);
  session->data_listen_fd = -1;

  return data_fd;
}


void ftp_session_init(ftp_session_t *session) {
  if (!session) return;

  memset(session, 0, sizeof(*session));
  session->ctrl_fd = -1;
  session->data_listen_fd = -1;
  session->data_port = 0;
  session->last_code = -1;
  session->connected = false;
  session->logged_in = false;
}


int ftp_connect(ftp_session_t *session, const char *host, uint16_t port) {
  if (!session || !host) return -1;

  /* Resolve hostname */
  struct addrinfo hints = {
    .ai_family = AF_INET,
    .ai_socktype = SOCK_STREAM
  };
  struct addrinfo *result = NULL;

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%u", port);

  int err = getaddrinfo(host, port_str, &hints, &result);
  if (err != 0 || !result) {
    return -1;
  }

  /* Create socket */
  int sock = socket(result->ai_family, result->ai_socktype,
                    result->ai_protocol);
  if (sock < 0) {
    freeaddrinfo(result);
    return -1;
  }

  /* Connect */
  if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
    close(sock);
    freeaddrinfo(result);
    return -1;
  }

  freeaddrinfo(result);

  session->ctrl_fd = sock;
  session->connected = true;

  /* Read welcome message */
  int code = read_response(session);
  if (code != 220) {
    close(sock);
    session->ctrl_fd = -1;
    session->connected = false;
    return -1;
  }

  return 0;
}


int ftp_login(ftp_session_t *session, const char *username) {
  if (!session || !session->connected || !username) return -1;

  char cmd[128];
  snprintf(cmd, sizeof(cmd), "USER %s", username);

  if (send_command(session, cmd) < 0) return -1;

  int code = read_response(session);
  if (code == 230) {
    session->logged_in = true;
    return 0;
  }

  return -1;
}


int ftp_quit(ftp_session_t *session) {
  if (!session || !session->connected) return -1;

  send_command(session, "QUIT");
  read_response(session);

  ftp_close(session);
  return 0;
}


void ftp_close(ftp_session_t *session) {
  if (!session) return;

  if (session->data_listen_fd >= 0) {
    close(session->data_listen_fd);
    session->data_listen_fd = -1;
  }

  if (session->ctrl_fd >= 0) {
    close(session->ctrl_fd);
    session->ctrl_fd = -1;
  }

  session->connected = false;
  session->logged_in = false;
}


int ftp_list(ftp_session_t *session, const char *path, char **output) {
  if (!session || !session->logged_in || !output) return -1;

  *output = NULL;

  /* Setup data connection */
  if (setup_data_listener(session) < 0) return -1;
  if (send_port_command(session) < 0) return -1;

  /* Send LIST command */
  char cmd[512];
  if (path && path[0]) {
    snprintf(cmd, sizeof(cmd), "LIST %s", path);
  } else {
    strcpy(cmd, "LIST");
  }

  if (send_command(session, cmd) < 0) return -1;

  int code = read_response(session);
  if (code != 150 && code != 125) {
    if (session->data_listen_fd >= 0) {
      close(session->data_listen_fd);
      session->data_listen_fd = -1;
    }
    return -1;
  }

  /* Accept data connection */
  int data_fd = accept_data_connection(session);
  if (data_fd < 0) return -1;

  /* Read listing */
  size_t capacity = 4096;
  size_t len = 0;
  char *buf = malloc(capacity);
  if (!buf) {
    close(data_fd);
    return -1;
  }

  ssize_t n;
  while ((n = recv(data_fd, buf + len, capacity - len - 1, 0)) > 0) {
    len += (size_t)n;
    if (len >= capacity - 1) {
      capacity *= 2;
      char *newbuf = realloc(buf, capacity);
      if (!newbuf) {
        free(buf);
        close(data_fd);
        return -1;
      }
      buf = newbuf;
    }
  }

  buf[len] = '\0';
  close(data_fd);

  /* Read completion response */
  code = read_response(session);
  if (code != 226) {
    free(buf);
    return -1;
  }

  *output = buf;
  return 0;
}


int ftp_get(ftp_session_t *session, const char *remote, const char *local) {
  if (!session || !session->logged_in || !remote) return -1;

  /* Determine local filename */
  const char *localname = local;
  if (!localname || !localname[0]) {
    localname = strrchr(remote, '/');
    localname = localname ? localname + 1 : remote;
  }

  /* Setup data connection */
  if (setup_data_listener(session) < 0) return -1;
  if (send_port_command(session) < 0) return -1;

  /* Send RETR command */
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "RETR %s", remote);

  if (send_command(session, cmd) < 0) return -1;

  int code = read_response(session);
  if (code != 150 && code != 125) {
    if (session->data_listen_fd >= 0) {
      close(session->data_listen_fd);
      session->data_listen_fd = -1;
    }
    return -1;
  }

  /* Accept data connection */
  int data_fd = accept_data_connection(session);
  if (data_fd < 0) return -1;

  /* Open local file */
  int file_fd = open(localname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (file_fd < 0) {
    close(data_fd);
    return -1;
  }

  /* Receive and write data */
  char buf[FTP_BUFFER_SIZE];
  ssize_t n;
  int result = 0;

  while ((n = recv(data_fd, buf, sizeof(buf), 0)) > 0) {
    ssize_t written = 0;
    while (written < n) {
      ssize_t w = write(file_fd, buf + written, (size_t)(n - written));
      if (w < 0) {
        if (errno == EINTR) continue;
        result = -1;
        goto done;
      }
      written += w;
    }
  }

  if (n < 0) result = -1;

done:
  close(file_fd);
  close(data_fd);

  /* Read completion response */
  code = read_response(session);
  if (code != 226) result = -1;

  return result;
}


int ftp_put(ftp_session_t *session, const char *local, const char *remote) {
  if (!session || !session->logged_in || !local) return -1;

  /* Determine remote filename */
  const char *remotename = remote;
  if (!remotename || !remotename[0]) {
    remotename = strrchr(local, '/');
    remotename = remotename ? remotename + 1 : local;
  }

  /* Open local file */
  int file_fd = open(local, O_RDONLY);
  if (file_fd < 0) return -1;

  /* Setup data connection */
  if (setup_data_listener(session) < 0) {
    close(file_fd);
    return -1;
  }
  if (send_port_command(session) < 0) {
    close(file_fd);
    return -1;
  }

  /* Send STOR command */
  char cmd[512];
  snprintf(cmd, sizeof(cmd), "STOR %s", remotename);

  if (send_command(session, cmd) < 0) {
    close(file_fd);
    return -1;
  }

  int code = read_response(session);
  if (code != 150 && code != 125) {
    close(file_fd);
    if (session->data_listen_fd >= 0) {
      close(session->data_listen_fd);
      session->data_listen_fd = -1;
    }
    return -1;
  }

  /* Accept data connection */
  int data_fd = accept_data_connection(session);
  if (data_fd < 0) {
    close(file_fd);
    return -1;
  }

  /* Read and send data */
  char buf[FTP_BUFFER_SIZE];
  ssize_t n;
  int result = 0;

  while ((n = read(file_fd, buf, sizeof(buf))) > 0) {
    ssize_t sent = 0;
    while (sent < n) {
      ssize_t s = send(data_fd, buf + sent, (size_t)(n - sent), 0);
      if (s < 0) {
        if (errno == EINTR) continue;
        result = -1;
        goto done;
      }
      sent += s;
    }
  }

  if (n < 0) result = -1;

done:
  close(file_fd);
  close(data_fd);

  /* Read completion response */
  code = read_response(session);
  if (code != 226) result = -1;

  return result;
}


int ftp_mkdir(ftp_session_t *session, const char *dirname) {
  if (!session || !session->logged_in || !dirname) return -1;

  char cmd[512];
  snprintf(cmd, sizeof(cmd), "MKD %s", dirname);

  if (send_command(session, cmd) < 0) return -1;

  int code = read_response(session);
  return (code == 257) ? 0 : -1;
}


int ftp_pwd(ftp_session_t *session, char *path, size_t pathsize) {
  if (!session || !session->logged_in || !path || pathsize == 0) return -1;

  if (send_command(session, "PWD") < 0) return -1;

  int code = read_response(session);
  if (code != 257) return -1;

  /* Parse path from response: 257 "/path" ... */
  const char *start = strchr(session->last_response, '"');
  if (!start) return -1;
  start++;

  const char *end = strchr(start, '"');
  if (!end) return -1;

  size_t len = (size_t)(end - start);
  if (len >= pathsize) len = pathsize - 1;

  memcpy(path, start, len);
  path[len] = '\0';

  return 0;
}


int ftp_cd(ftp_session_t *session, const char *path) {
  if (!session || !session->logged_in || !path) return -1;

  char cmd[512];
  snprintf(cmd, sizeof(cmd), "CWD %s", path);

  if (send_command(session, cmd) < 0) return -1;

  int code = read_response(session);
  return (code == 250) ? 0 : -1;
}


int ftp_command(ftp_session_t *session, const char *cmd) {
  if (!session || !session->connected || !cmd) return -1;

  if (send_command(session, cmd) < 0) return -1;
  return read_response(session);
}


const char *ftp_last_response(ftp_session_t *session) {
  if (!session) return "";
  return session->last_response;
}


int ftp_last_code(ftp_session_t *session) {
  if (!session) return -1;
  return session->last_code;
}
