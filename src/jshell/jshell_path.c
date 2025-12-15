#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

#include "jshell_path.h"
#include "jshell_cmd_registry.h"
#include "utils/jbox_utils.h"


#define JSHELL_BIN_SUBPATH "/.jshell/bin"
#define MAX_PATH_LEN 4096


static char g_jshell_bin_dir[MAX_PATH_LEN] = {0};
static int g_path_initialized = 0;


static int make_directory_recursive(const char* path) {
  char tmp[MAX_PATH_LEN];
  char* p = NULL;
  size_t len;

  snprintf(tmp, sizeof(tmp), "%s", path);
  len = strlen(tmp);

  if (tmp[len - 1] == '/') {
    tmp[len - 1] = '\0';
  }

  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
      }
      *p = '/';
    }
  }

  if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
    return -1;
  }

  return 0;
}


static const char* get_home_directory(void) {
  const char* home = getenv("HOME");
  if (home != NULL && home[0] != '\0') {
    return home;
  }

  struct passwd* pw = getpwuid(getuid());
  if (pw != NULL && pw->pw_dir != NULL) {
    return pw->pw_dir;
  }

  return NULL;
}


void jshell_init_path(void) {
  if (g_path_initialized) {
    return;
  }

  const char* home = get_home_directory();
  if (home == NULL) {
    fprintf(stderr, "jshell: warning: could not determine home directory\n");
    g_path_initialized = 1;
    return;
  }

  snprintf(g_jshell_bin_dir, sizeof(g_jshell_bin_dir),
           "%s%s", home, JSHELL_BIN_SUBPATH);

  if (make_directory_recursive(g_jshell_bin_dir) != 0) {
    fprintf(stderr, "jshell: warning: could not create %s: %s\n",
            g_jshell_bin_dir, strerror(errno));
  }

  const char* current_path = getenv("PATH");
  if (current_path != NULL) {
    size_t new_path_len = strlen(g_jshell_bin_dir) + 1 + strlen(current_path)
                          + 1;
    char* new_path = malloc(new_path_len);
    if (new_path != NULL) {
      snprintf(new_path, new_path_len, "%s:%s", g_jshell_bin_dir, current_path);
      setenv("PATH", new_path, 1);
      free(new_path);
      DPRINT("PATH updated: %s prepended", g_jshell_bin_dir);
    }
  } else {
    setenv("PATH", g_jshell_bin_dir, 1);
  }

  g_path_initialized = 1;
  DPRINT("jshell path initialized: bin_dir=%s", g_jshell_bin_dir);
}


const char* jshell_get_bin_dir(void) {
  if (!g_path_initialized) {
    jshell_init_path();
  }
  return g_jshell_bin_dir;
}


static char* search_path_for_command(const char* cmd_name) {
  const char* path_env = getenv("PATH");
  if (path_env == NULL || cmd_name == NULL) {
    return NULL;
  }

  char* path_copy = strdup(path_env);
  if (path_copy == NULL) {
    return NULL;
  }

  char full_path[MAX_PATH_LEN];
  char* saveptr = NULL;
  char* dir = strtok_r(path_copy, ":", &saveptr);

  while (dir != NULL) {
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd_name);

    if (access(full_path, X_OK) == 0) {
      free(path_copy);
      return strdup(full_path);
    }

    dir = strtok_r(NULL, ":", &saveptr);
  }

  free(path_copy);
  return NULL;
}


char* jshell_resolve_command(const char* cmd_name) {
  if (cmd_name == NULL || cmd_name[0] == '\0') {
    return NULL;
  }

  if (cmd_name[0] == '/' || cmd_name[0] == '.') {
    if (access(cmd_name, X_OK) == 0) {
      return strdup(cmd_name);
    }
    return NULL;
  }

  const jshell_cmd_spec_t* spec = jshell_find_command(cmd_name);

  if (spec != NULL && spec->type == CMD_EXTERNAL) {
    if (g_jshell_bin_dir[0] != '\0') {
      size_t path_len = strlen(g_jshell_bin_dir) + 1 + strlen(cmd_name) + 1;
      char* local_path = malloc(path_len);
      if (local_path != NULL) {
        snprintf(local_path, path_len, "%s/%s", g_jshell_bin_dir, cmd_name);

        if (access(local_path, X_OK) == 0) {
          DPRINT("Found command in jshell bin: %s", local_path);
          return local_path;
        }
        free(local_path);
      }
    }
  }

  char* system_path = search_path_for_command(cmd_name);
  if (system_path != NULL) {
    DPRINT("Found command in system PATH: %s", system_path);
    return system_path;
  }

  return NULL;
}


void jshell_cleanup_path(void) {
  g_path_initialized = 0;
  g_jshell_bin_dir[0] = '\0';
}
