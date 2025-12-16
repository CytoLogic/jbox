/**
 * @file jshell_path.c
 * @brief Path management and command resolution for jshell.
 *
 * Handles initialization of jshell's binary directory (~/.jshell/bin),
 * PATH environment variable updates, and command resolution for external
 * executables.
 */

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


/** Path to jshell's binary directory */
static char g_jshell_bin_dir[MAX_PATH_LEN] = {0};

/** Flag indicating whether path has been initialized */
static int g_path_initialized = 0;


/**
 * Recursively creates directories along a path.
 *
 * @param path Directory path to create.
 * @return 0 on success, -1 on failure.
 */
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


/**
 * Gets the user's home directory.
 *
 * Tries the HOME environment variable first, then falls back to
 * querying the password database.
 *
 * @return Pointer to home directory string, or NULL if not found.
 */
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


/**
 * Initializes jshell's path system.
 *
 * Creates the ~/.jshell/bin directory if it doesn't exist and prepends
 * it to the PATH environment variable. This allows installed packages
 * to be found and executed. This function is idempotent - it can be
 * called multiple times safely.
 */
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


/**
 * Gets the path to jshell's binary directory.
 *
 * Ensures the path system is initialized before returning the directory.
 *
 * @return Pointer to the binary directory path (typically ~/.jshell/bin).
 */
const char* jshell_get_bin_dir(void) {
  if (!g_path_initialized) {
    jshell_init_path();
  }
  return g_jshell_bin_dir;
}


/**
 * Searches the system PATH for an executable command.
 *
 * Iterates through directories in the PATH environment variable
 * and checks for an executable file with the given name.
 *
 * @param cmd_name Name of the command to search for.
 * @return Dynamically allocated full path to the command if found,
 *         NULL otherwise. Caller must free the returned string.
 */
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


/**
 * Resolves a command name to its full executable path.
 *
 * Resolution order:
 * 1. If cmd_name starts with '/' or '.', treat as absolute/relative path
 * 2. If command is registered as external, check jshell bin directory
 * 3. Search system PATH
 *
 * @param cmd_name Name or path of the command to resolve.
 * @return Dynamically allocated full path to the executable if found,
 *         NULL otherwise. Caller must free the returned string.
 */
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


/**
 * Cleans up the path system state.
 *
 * Resets initialization flags and clears the bin directory path.
 * Used during shell shutdown or reinitialization.
 */
void jshell_cleanup_path(void) {
  g_path_initialized = 0;
  g_jshell_bin_dir[0] = '\0';
}
