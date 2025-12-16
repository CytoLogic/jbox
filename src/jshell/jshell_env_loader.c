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
