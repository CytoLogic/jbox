/**
 * @file ftpd_commands.c
 * @brief FTP command handler implementations.
 *
 * This module implements all FTP command handlers including
 * USER, QUIT, PORT, STOR, RETR, LIST, MKD, PWD, CWD, TYPE, SYST, and NOOP.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

#include "ftpd.h"
#include "ftpd_commands.h"
#include "ftpd_client.h"
#include "ftpd_data.h"
#include "ftpd_path.h"


int ftpd_cmd_user(ftpd_client_t *client, const char *arg) {
  if (!arg || arg[0] == '\0') {
    ftpd_send_response(client, 501, "Syntax error: USER <username>");
    return 0;
  }

  /* Store username */
  strncpy(client->username, arg, sizeof(client->username) - 1);
  client->username[sizeof(client->username) - 1] = '\0';

  /* Set cwd to server root (user works relative to root) */
  strncpy(client->cwd, client->server->root_realpath,
          sizeof(client->cwd) - 1);
  client->cwd[sizeof(client->cwd) - 1] = '\0';

  client->authenticated = true;

  ftpd_send_response_fmt(client, 230, "User %s logged in.", client->username);
  printf("ftpd: user %s logged in\n", client->username);

  return 0;
}


int ftpd_cmd_quit(ftpd_client_t *client, const char *arg) {
  (void)arg;
  ftpd_send_response(client, 221, "Goodbye.");
  return -1;  /* Signal disconnect */
}


int ftpd_cmd_port(ftpd_client_t *client, const char *arg) {
  if (!arg || arg[0] == '\0') {
    ftpd_send_response(client, 501, "Syntax error: PORT a1,a2,a3,a4,p1,p2");
    return 0;
  }

  /* Parse PORT argument: a1,a2,a3,a4,p1,p2 */
  unsigned int a1, a2, a3, a4, p1, p2;
  if (sscanf(arg, "%u,%u,%u,%u,%u,%u", &a1, &a2, &a3, &a4, &p1, &p2) != 6) {
    ftpd_send_response(client, 501, "Syntax error in PORT command.");
    return 0;
  }

  /* Validate ranges */
  if (a1 > 255 || a2 > 255 || a3 > 255 || a4 > 255 ||
      p1 > 255 || p2 > 255) {
    ftpd_send_response(client, 501, "Invalid PORT parameters.");
    return 0;
  }

  /* Calculate port (p1*256 + p2) */
  uint16_t port = (uint16_t)((p1 << 8) | p2);
  if (port < 1024) {
    ftpd_send_response(client, 501, "Port must be >= 1024.");
    return 0;
  }

  /* Store (we always connect to 127.0.0.1, ignoring the IP) */
  client->data_port = port;
  client->data_port_set = true;

  ftpd_send_response(client, 200, "PORT command successful.");
  return 0;
}


int ftpd_cmd_stor(ftpd_client_t *client, const char *arg) {
  if (!arg || arg[0] == '\0') {
    ftpd_send_response(client, 501, "Syntax error: STOR <filename>");
    return 0;
  }

  if (!client->data_port_set) {
    ftpd_send_response(client, 425, "Use PORT first.");
    return 0;
  }

  /* Resolve path */
  char *filepath = ftpd_resolve_path(client, arg,
                                     client->server->root_realpath);
  if (!filepath) {
    ftpd_send_response(client, 553, "Invalid filename.");
    return 0;
  }

  /* Open data connection */
  if (ftpd_data_connect(client) < 0) {
    ftpd_send_response(client, 425, "Can't open data connection.");
    free(filepath);
    return 0;
  }

  ftpd_send_response(client, 150, "Opening BINARY mode data connection.");

  /* Receive file */
  if (ftpd_data_recv_file(client, filepath) < 0) {
    ftpd_send_response(client, 426, "Transfer aborted.");
  } else {
    ftpd_send_response(client, 226, "Transfer complete.");
  }

  ftpd_data_close(client);
  free(filepath);
  return 0;
}


int ftpd_cmd_retr(ftpd_client_t *client, const char *arg) {
  if (!arg || arg[0] == '\0') {
    ftpd_send_response(client, 501, "Syntax error: RETR <filename>");
    return 0;
  }

  if (!client->data_port_set) {
    ftpd_send_response(client, 425, "Use PORT first.");
    return 0;
  }

  /* Resolve path */
  char *filepath = ftpd_resolve_path(client, arg,
                                     client->server->root_realpath);
  if (!filepath) {
    ftpd_send_response(client, 550, "File not found.");
    return 0;
  }

  /* Check file exists and is readable */
  struct stat st;
  if (stat(filepath, &st) < 0 || !S_ISREG(st.st_mode)) {
    ftpd_send_response(client, 550, "File not found or not a regular file.");
    free(filepath);
    return 0;
  }

  /* Open data connection */
  if (ftpd_data_connect(client) < 0) {
    ftpd_send_response(client, 425, "Can't open data connection.");
    free(filepath);
    return 0;
  }

  ftpd_send_response_fmt(client, 150,
                         "Opening BINARY mode data connection (%ld bytes).",
                         (long)st.st_size);

  /* Send file */
  if (ftpd_data_send_file(client, filepath) < 0) {
    ftpd_send_response(client, 426, "Transfer aborted.");
  } else {
    ftpd_send_response(client, 226, "Transfer complete.");
  }

  ftpd_data_close(client);
  free(filepath);
  return 0;
}


/**
 * @brief Format file permissions as a string.
 *
 * @param mode File mode.
 * @param buf Buffer for result (at least 11 bytes).
 */
static void format_permissions(mode_t mode, char *buf) {
  buf[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : '-';
  buf[1] = (mode & S_IRUSR) ? 'r' : '-';
  buf[2] = (mode & S_IWUSR) ? 'w' : '-';
  buf[3] = (mode & S_IXUSR) ? 'x' : '-';
  buf[4] = (mode & S_IRGRP) ? 'r' : '-';
  buf[5] = (mode & S_IWGRP) ? 'w' : '-';
  buf[6] = (mode & S_IXGRP) ? 'x' : '-';
  buf[7] = (mode & S_IROTH) ? 'r' : '-';
  buf[8] = (mode & S_IWOTH) ? 'w' : '-';
  buf[9] = (mode & S_IXOTH) ? 'x' : '-';
  buf[10] = '\0';
}


/**
 * @brief Format a directory entry in ls -l style.
 *
 * @param dirpath Directory path.
 * @param entry Directory entry.
 * @param buf Output buffer.
 * @param bufsize Buffer size.
 * @return Length of formatted string, or -1 on error.
 */
static int format_dir_entry(const char *dirpath, struct dirent *entry,
                            char *buf, size_t bufsize) {
  char fullpath[PATH_MAX];
  if (snprintf(fullpath, sizeof(fullpath), "%s/%s",
               dirpath, entry->d_name) >= (int)sizeof(fullpath)) {
    return -1;
  }

  struct stat st;
  if (lstat(fullpath, &st) < 0) {
    return -1;
  }

  char perms[12];
  format_permissions(st.st_mode, perms);

  /* Get owner and group names */
  struct passwd *pw = getpwuid(st.st_uid);
  struct group *gr = getgrgid(st.st_gid);
  const char *owner = pw ? pw->pw_name : "?";
  const char *group = gr ? gr->gr_name : "?";

  /* Format time */
  char timebuf[32];
  struct tm *tm = localtime(&st.st_mtime);
  strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm);

  return snprintf(buf, bufsize, "%s %3lu %-8s %-8s %8ld %s %s\r\n",
                  perms,
                  (unsigned long)st.st_nlink,
                  owner,
                  group,
                  (long)st.st_size,
                  timebuf,
                  entry->d_name);
}


int ftpd_cmd_list(ftpd_client_t *client, const char *arg) {
  if (!client->data_port_set) {
    ftpd_send_response(client, 425, "Use PORT first.");
    return 0;
  }

  /* Resolve path */
  char *dirpath = ftpd_resolve_path(client, arg,
                                    client->server->root_realpath);
  if (!dirpath) {
    dirpath = strdup(client->cwd);
    if (!dirpath) {
      ftpd_send_response(client, 550, "Memory error.");
      return 0;
    }
  }

  /* Open directory */
  DIR *dir = opendir(dirpath);
  if (!dir) {
    ftpd_send_response(client, 550, "Failed to open directory.");
    free(dirpath);
    return 0;
  }

  /* Open data connection */
  if (ftpd_data_connect(client) < 0) {
    ftpd_send_response(client, 425, "Can't open data connection.");
    closedir(dir);
    free(dirpath);
    return 0;
  }

  ftpd_send_response(client, 150, "Opening ASCII mode data connection.");

  /* Send directory listing */
  struct dirent *entry;
  char linebuf[512];
  int result = 0;

  while ((entry = readdir(dir)) != NULL) {
    /* Skip . and .. */
    if (strcmp(entry->d_name, ".") == 0 ||
        strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    int len = format_dir_entry(dirpath, entry, linebuf, sizeof(linebuf));
    if (len > 0) {
      if (ftpd_data_send(client, linebuf, (size_t)len) < 0) {
        result = -1;
        break;
      }
    }
  }

  closedir(dir);
  ftpd_data_close(client);
  free(dirpath);

  if (result < 0) {
    ftpd_send_response(client, 426, "Transfer aborted.");
  } else {
    ftpd_send_response(client, 226, "Transfer complete.");
  }

  return 0;
}


int ftpd_cmd_mkd(ftpd_client_t *client, const char *arg) {
  if (!arg || arg[0] == '\0') {
    ftpd_send_response(client, 501, "Syntax error: MKD <dirname>");
    return 0;
  }

  /* Resolve path */
  char *dirpath = ftpd_resolve_path(client, arg,
                                    client->server->root_realpath);
  if (!dirpath) {
    ftpd_send_response(client, 553, "Invalid directory name.");
    return 0;
  }

  /* Create directory */
  if (mkdir(dirpath, 0755) < 0) {
    if (errno == EEXIST) {
      ftpd_send_response(client, 550, "Directory already exists.");
    } else {
      ftpd_send_response_fmt(client, 550, "mkdir failed: %s", strerror(errno));
    }
    free(dirpath);
    return 0;
  }

  /* Get display path */
  char displaypath[PATH_MAX];
  ftpd_path_to_display(dirpath, client->server->root_realpath,
                       displaypath, sizeof(displaypath));

  ftpd_send_response_fmt(client, 257, "\"%s\" directory created.", displaypath);
  free(dirpath);
  return 0;
}


int ftpd_cmd_pwd(ftpd_client_t *client, const char *arg) {
  (void)arg;

  char displaypath[PATH_MAX];
  ftpd_path_to_display(client->cwd, client->server->root_realpath,
                       displaypath, sizeof(displaypath));

  ftpd_send_response_fmt(client, 257, "\"%s\" is current directory.",
                         displaypath);
  return 0;
}


int ftpd_cmd_cwd(ftpd_client_t *client, const char *arg) {
  if (!arg || arg[0] == '\0') {
    ftpd_send_response(client, 501, "Syntax error: CWD <path>");
    return 0;
  }

  /* Resolve path */
  char *newpath = ftpd_resolve_path(client, arg,
                                    client->server->root_realpath);
  if (!newpath) {
    ftpd_send_response(client, 550, "Failed to change directory.");
    return 0;
  }

  /* Check it's a directory */
  struct stat st;
  if (stat(newpath, &st) < 0 || !S_ISDIR(st.st_mode)) {
    ftpd_send_response(client, 550, "Not a directory.");
    free(newpath);
    return 0;
  }

  /* Update cwd */
  strncpy(client->cwd, newpath, sizeof(client->cwd) - 1);
  client->cwd[sizeof(client->cwd) - 1] = '\0';
  free(newpath);

  ftpd_send_response(client, 250, "Directory changed.");
  return 0;
}


int ftpd_cmd_type(ftpd_client_t *client, const char *arg) {
  (void)arg;
  /* Accept any type but always use binary */
  ftpd_send_response(client, 200, "Type set to I (binary).");
  return 0;
}


int ftpd_cmd_syst(ftpd_client_t *client, const char *arg) {
  (void)arg;
  ftpd_send_response(client, 215, "UNIX Type: L8");
  return 0;
}


int ftpd_cmd_noop(ftpd_client_t *client, const char *arg) {
  (void)arg;
  ftpd_send_response(client, 200, "NOOP ok.");
  return 0;
}
