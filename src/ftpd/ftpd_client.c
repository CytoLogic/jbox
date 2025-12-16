/**
 * @file ftpd_client.c
 * @brief FTP client session handler implementation.
 *
 * This module handles individual client connections, parsing commands
 * and dispatching them to the appropriate handlers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/socket.h>

#include "ftpd.h"
#include "ftpd_client.h"
#include "ftpd_commands.h"
#include "ftpd_data.h"


/**
 * @brief FTP command table entry.
 */
typedef struct {
  const char *name;
  int (*handler)(ftpd_client_t *client, const char *arg);
  bool requires_auth;
} ftpd_cmd_entry_t;


/** Command dispatch table. */
static const ftpd_cmd_entry_t cmd_table[] = {
  {"USER", ftpd_cmd_user, false},
  {"QUIT", ftpd_cmd_quit, false},
  {"PORT", ftpd_cmd_port, true},
  {"STOR", ftpd_cmd_stor, true},
  {"RETR", ftpd_cmd_retr, true},
  {"LIST", ftpd_cmd_list, true},
  {"MKD",  ftpd_cmd_mkd,  true},
  {"PWD",  ftpd_cmd_pwd,  true},
  {"CWD",  ftpd_cmd_cwd,  true},
  {"TYPE", ftpd_cmd_type, true},
  {"SYST", ftpd_cmd_syst, false},
  {"NOOP", ftpd_cmd_noop, false},
  {NULL, NULL, false}
};


void ftpd_client_init(ftpd_client_t *client, int ctrl_fd,
                      ftpd_server_t *server) {
  memset(client, 0, sizeof(*client));
  client->ctrl_fd = ctrl_fd;
  client->data_fd = -1;
  client->data_port = 0;
  client->authenticated = false;
  client->data_port_set = false;
  client->server = server;
  client->next = NULL;

  /* Set initial cwd to server root */
  strncpy(client->cwd, server->root_realpath, sizeof(client->cwd) - 1);
  client->cwd[sizeof(client->cwd) - 1] = '\0';
}


void ftpd_client_cleanup(ftpd_client_t *client) {
  if (!client) {
    return;
  }

  /* Close data connection if open */
  ftpd_data_close(client);

  /* Close control connection */
  if (client->ctrl_fd >= 0) {
    close(client->ctrl_fd);
    client->ctrl_fd = -1;
  }
}


int ftpd_read_command(ftpd_client_t *client, char *buf, size_t bufsize) {
  if (!client || !buf || bufsize < 2) {
    return -1;
  }

  size_t pos = 0;
  while (pos < bufsize - 1) {
    char c;
    ssize_t n = recv(client->ctrl_fd, &c, 1, 0);

    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }

    if (n == 0) {
      /* Connection closed */
      if (pos > 0) {
        buf[pos] = '\0';
        return (int)pos;
      }
      return 0;
    }

    /* Check for end of line */
    if (c == '\n') {
      /* Strip trailing CR if present */
      if (pos > 0 && buf[pos - 1] == '\r') {
        pos--;
      }
      buf[pos] = '\0';
      return (int)pos;
    }

    buf[pos++] = c;
  }

  /* Buffer full without finding newline */
  buf[bufsize - 1] = '\0';
  return (int)(bufsize - 1);
}


int ftpd_send_response(ftpd_client_t *client, int code, const char *message) {
  if (!client || client->ctrl_fd < 0) {
    return -1;
  }

  char buf[512];
  int len = snprintf(buf, sizeof(buf), "%d %s\r\n", code, message);

  if (len < 0 || len >= (int)sizeof(buf)) {
    return -1;
  }

  ssize_t sent = 0;
  while (sent < len) {
    ssize_t n = send(client->ctrl_fd, buf + sent, (size_t)(len - sent), 0);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    sent += n;
  }

  return 0;
}


int ftpd_send_response_fmt(ftpd_client_t *client, int code,
                           const char *fmt, ...) {
  char message[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);
  return ftpd_send_response(client, code, message);
}


int ftpd_dispatch_command(ftpd_client_t *client, const char *cmdline) {
  if (!client || !cmdline) {
    return -1;
  }

  /* Skip leading whitespace */
  while (*cmdline && isspace((unsigned char)*cmdline)) {
    cmdline++;
  }

  /* Empty command */
  if (*cmdline == '\0') {
    return 0;
  }

  /* Extract command name */
  char cmd[16];
  const char *p = cmdline;
  int i = 0;
  while (*p && !isspace((unsigned char)*p) && i < (int)sizeof(cmd) - 1) {
    cmd[i++] = (char)toupper((unsigned char)*p);
    p++;
  }
  cmd[i] = '\0';

  /* Skip whitespace to find argument */
  while (*p && isspace((unsigned char)*p)) {
    p++;
  }
  const char *arg = (*p) ? p : NULL;

  /* Look up command */
  for (const ftpd_cmd_entry_t *entry = cmd_table; entry->name; entry++) {
    if (strcmp(cmd, entry->name) == 0) {
      /* Check authentication */
      if (entry->requires_auth && !client->authenticated) {
        ftpd_send_response(client, 530, "Not logged in.");
        return 0;
      }
      return entry->handler(client, arg);
    }
  }

  /* Unknown command */
  ftpd_send_response_fmt(client, 500, "Unknown command: %s", cmd);
  return 0;
}


void *ftpd_client_handler(void *arg) {
  ftpd_client_t *client = (ftpd_client_t *)arg;

  /* Send welcome message */
  ftpd_send_response(client, 220, "jbox FTP server ready.");

  /* Command loop */
  char cmdbuf[FTPD_CMD_MAX];
  while (client->server->running) {
    int len = ftpd_read_command(client, cmdbuf, sizeof(cmdbuf));

    if (len < 0) {
      /* Error reading */
      break;
    }

    if (len == 0) {
      /* Connection closed */
      break;
    }

    /* Dispatch command */
    if (ftpd_dispatch_command(client, cmdbuf) < 0) {
      /* Handler signaled disconnect */
      break;
    }
  }

  /* Clean up */
  ftpd_client_cleanup(client);

  if (client->username[0]) {
    printf("ftpd: client %s disconnected\n", client->username);
  } else {
    printf("ftpd: client disconnected\n");
  }

  return NULL;
}
