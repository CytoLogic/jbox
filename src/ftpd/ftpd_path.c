/**
 * @file ftpd_path.c
 * @brief FTP path resolution and security implementation.
 *
 * This module provides secure path resolution to prevent directory
 * traversal attacks. All paths are validated to ensure they remain
 * within the server root directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "ftpd.h"
#include "ftpd_path.h"


char *ftpd_path_join(const char *base, const char *name,
                     char *buf, size_t bufsize) {
  if (!base || !name || !buf || bufsize == 0) {
    return NULL;
  }

  size_t base_len = strlen(base);
  size_t name_len = strlen(name);

  /* Check if we need a separator */
  int need_sep = (base_len > 0 && base[base_len - 1] != '/') ? 1 : 0;

  /* Check total length */
  if (base_len + need_sep + name_len >= bufsize) {
    return NULL;
  }

  /* Build the path */
  strcpy(buf, base);
  if (need_sep) {
    buf[base_len] = '/';
    strcpy(buf + base_len + 1, name);
  } else {
    strcpy(buf + base_len, name);
  }

  return buf;
}


bool ftpd_path_is_safe(const char *path, const char *server_root) {
  if (!path || !server_root) {
    return false;
  }

  size_t root_len = strlen(server_root);

  /* Path must start with server_root */
  if (strncmp(path, server_root, root_len) != 0) {
    return false;
  }

  /* If path is exactly server_root, it's safe */
  if (path[root_len] == '\0') {
    return true;
  }

  /* Path must have a separator after server_root */
  if (path[root_len] != '/') {
    return false;
  }

  return true;
}


char *ftpd_resolve_path(ftpd_client_t *client, const char *path,
                        const char *server_root) {
  if (!client || !server_root) {
    return NULL;
  }

  char workpath[PATH_MAX];

  if (!path || path[0] == '\0') {
    /* Use current working directory */
    strncpy(workpath, client->cwd, sizeof(workpath) - 1);
    workpath[sizeof(workpath) - 1] = '\0';
  } else if (path[0] == '/') {
    /* Absolute path - relative to server root */
    if (snprintf(workpath, sizeof(workpath), "%s%s",
                 server_root, path) >= (int)sizeof(workpath)) {
      return NULL;
    }
  } else {
    /* Relative path - relative to cwd */
    if (ftpd_path_join(client->cwd, path, workpath, sizeof(workpath)) == NULL) {
      return NULL;
    }
  }

  /* Resolve to canonical path */
  char *resolved = realpath(workpath, NULL);

  /* If file doesn't exist, realpath fails. Try parent directory. */
  if (!resolved) {
    /* For non-existent files, construct the path manually */
    /* Find the last component */
    char *last_slash = strrchr(workpath, '/');
    if (last_slash && last_slash != workpath) {
      *last_slash = '\0';
      char *parent = realpath(workpath, NULL);
      if (parent) {
        /* Reconstruct with resolved parent */
        size_t plen = strlen(parent);
        size_t nlen = strlen(last_slash + 1);
        resolved = malloc(plen + 1 + nlen + 1);
        if (resolved) {
          sprintf(resolved, "%s/%s", parent, last_slash + 1);
        }
        free(parent);
      }
    }
  }

  if (!resolved) {
    return NULL;
  }

  /* Verify path is within server root */
  if (!ftpd_path_is_safe(resolved, server_root)) {
    free(resolved);
    return NULL;
  }

  return resolved;
}


char *ftpd_path_to_display(const char *abspath, const char *server_root,
                           char *buf, size_t bufsize) {
  if (!abspath || !server_root || !buf || bufsize == 0) {
    return NULL;
  }

  size_t root_len = strlen(server_root);

  /* Path must start with server_root */
  if (strncmp(abspath, server_root, root_len) != 0) {
    return NULL;
  }

  const char *relpath = abspath + root_len;

  /* Handle root directory */
  if (relpath[0] == '\0') {
    if (bufsize < 2) {
      return NULL;
    }
    buf[0] = '/';
    buf[1] = '\0';
    return buf;
  }

  /* Copy the relative path (should start with /) */
  if (relpath[0] != '/') {
    /* Shouldn't happen, but handle it */
    if (snprintf(buf, bufsize, "/%s", relpath) >= (int)bufsize) {
      return NULL;
    }
  } else {
    if (strlen(relpath) >= bufsize) {
      return NULL;
    }
    strcpy(buf, relpath);
  }

  return buf;
}
