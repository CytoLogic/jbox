/**
 * @file jshell_env_loader.c
 * @brief Environment variable loader for jshell.
 *
 * Loads environment variables from ~/.jshell/env file at shell startup.
 * The file format supports KEY=VALUE pairs with optional quotes and comments.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>

#include "jshell_env_loader.h"
#include "utils/jbox_utils.h"


#define JSHELL_ENV_SUBPATH "/.jshell/env"
#define MAX_LINE_LEN 4096
#define MAX_PATH_LEN 4096


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
 * Removes leading and trailing whitespace from a string in-place.
 *
 * @param str String to trim (will be modified).
 * @return Pointer to the trimmed string (points into the original buffer).
 */
static char* trim_whitespace(char* str) {
  while (isspace((unsigned char)*str)) {
    str++;
  }

  if (*str == '\0') {
    return str;
  }

  char* end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end)) {
    end--;
  }
  end[1] = '\0';

  return str;
}


/**
 * Validates an environment variable name.
 *
 * Valid names start with a letter or underscore, followed by letters,
 * digits, or underscores.
 *
 * @param name Variable name to validate.
 * @return 1 if valid, 0 if invalid.
 */
static int is_valid_var_name(const char* name) {
  if (name == NULL || *name == '\0') {
    return 0;
  }

  if (!isalpha((unsigned char)*name) && *name != '_') {
    return 0;
  }

  for (const char* p = name + 1; *p != '\0'; p++) {
    if (!isalnum((unsigned char)*p) && *p != '_') {
      return 0;
    }
  }

  return 1;
}


/**
 * Parses a single line from the env file.
 *
 * Supports the following formats:
 * - KEY=value
 * - KEY="value with spaces"
 * - KEY='value with spaces'
 * - # comment lines (ignored)
 * - blank lines (ignored)
 *
 * @param line Line to parse.
 * @param name_out Output parameter for variable name (caller must free).
 * @param value_out Output parameter for variable value (caller must free).
 * @return 1 if a valid KEY=VALUE pair was parsed, 0 if line was empty/comment,
 *         -1 on error.
 */
static int parse_env_line(const char* line, char** name_out, char** value_out) {
  *name_out = NULL;
  *value_out = NULL;

  char* line_copy = strdup(line);
  if (line_copy == NULL) {
    return -1;
  }

  char* trimmed = trim_whitespace(line_copy);

  if (*trimmed == '\0' || *trimmed == '#') {
    free(line_copy);
    return 0;
  }

  char* equals = strchr(trimmed, '=');
  if (equals == NULL) {
    free(line_copy);
    return -1;
  }

  *equals = '\0';
  char* name = trim_whitespace(trimmed);
  char* value_start = equals + 1;

  if (!is_valid_var_name(name)) {
    free(line_copy);
    return -1;
  }

  value_start = trim_whitespace(value_start);

  char* value = NULL;
  size_t value_len = strlen(value_start);

  if (value_len >= 2 && value_start[0] == '"'
      && value_start[value_len - 1] == '"') {
    value_start[value_len - 1] = '\0';
    value = strdup(value_start + 1);
  } else if (value_len >= 2 && value_start[0] == '\''
             && value_start[value_len - 1] == '\'') {
    value_start[value_len - 1] = '\0';
    value = strdup(value_start + 1);
  } else {
    value = strdup(value_start);
  }

  if (value == NULL) {
    free(line_copy);
    return -1;
  }

  *name_out = strdup(name);
  if (*name_out == NULL) {
    free(value);
    free(line_copy);
    return -1;
  }

  *value_out = value;
  free(line_copy);
  return 1;
}


/**
 * Loads environment variables from the ~/.jshell/env file.
 *
 * This function should be called during shell initialization. If the file
 * doesn't exist, the function returns silently. Syntax errors in the file
 * are reported as warnings but don't stop processing.
 */
void jshell_load_env_file(void) {
  const char* home = get_home_directory();
  if (home == NULL) {
    DPRINT("Could not determine home directory for env file");
    return;
  }

  char env_path[MAX_PATH_LEN];
  snprintf(env_path, sizeof(env_path), "%s%s", home, JSHELL_ENV_SUBPATH);

  FILE* fp = fopen(env_path, "r");
  if (fp == NULL) {
    DPRINT("No env file found at %s", env_path);
    return;
  }

  DPRINT("Loading environment from %s", env_path);

  char line[MAX_LINE_LEN];
  int line_num = 0;
  int loaded_count = 0;

  while (fgets(line, sizeof(line), fp) != NULL) {
    line_num++;

    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }

    char* name = NULL;
    char* value = NULL;
    int result = parse_env_line(line, &name, &value);

    if (result < 0) {
      fprintf(stderr, "jshell: warning: invalid env file syntax at line %d\n",
              line_num);
      continue;
    }

    if (result == 0) {
      continue;
    }

    if (setenv(name, value, 1) == 0) {
      DPRINT("Set %s from env file", name);
      loaded_count++;
    } else {
      fprintf(stderr, "jshell: warning: failed to set %s\n", name);
    }

    free(name);
    free(value);
  }

  fclose(fp);
  DPRINT("Loaded %d environment variables from %s", loaded_count, env_path);
}
