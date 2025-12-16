/**
 * @file jshell_pkg_loader.c
 * @brief Package loader for jshell installed packages.
 *
 * Parses ~/.jshell/pkgs/pkgdb.json and registers installed package commands
 * with the shell's command registry. This allows installed packages to be
 * available as commands in the shell.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <sys/stat.h>

#include "jshell_pkg_loader.h"
#include "jshell_cmd_registry.h"


// ---------------------------------------------------------------------------
// Path Utilities (static - internal use only)
// ---------------------------------------------------------------------------

/**
 * Gets the jshell home directory path.
 *
 * @return Dynamically allocated path to ~/.jshell, or NULL on error.
 *         Caller must free the returned string.
 */
static char *get_home_dir(void) {
  const char *home = getenv("HOME");
  if (home == NULL) {
    return NULL;
  }

  size_t len = strlen(home) + strlen("/.jshell") + 1;
  char *path = malloc(len);
  if (path == NULL) {
    return NULL;
  }

  snprintf(path, len, "%s/.jshell", home);
  return path;
}


/**
 * Gets the jshell binary directory path.
 *
 * @return Dynamically allocated path to ~/.jshell/bin, or NULL on error.
 *         Caller must free the returned string.
 */
static char *get_bin_dir(void) {
  char *home = get_home_dir();
  if (home == NULL) {
    return NULL;
  }

  size_t len = strlen(home) + strlen("/bin") + 1;
  char *path = malloc(len);
  if (path == NULL) {
    free(home);
    return NULL;
  }

  snprintf(path, len, "%s/bin", home);
  free(home);
  return path;
}


/**
 * Gets the package database file path.
 *
 * @return Dynamically allocated path to ~/.jshell/pkgs/pkgdb.json, or NULL on
 *         error. Caller must free the returned string.
 */
static char *get_pkgdb_path(void) {
  char *home = get_home_dir();
  if (home == NULL) {
    return NULL;
  }

  size_t len = strlen(home) + strlen("/pkgs/pkgdb.json") + 1;
  char *path = malloc(len);
  if (path == NULL) {
    free(home);
    return NULL;
  }

  snprintf(path, len, "%s/pkgs/pkgdb.json", home);
  free(home);
  return path;
}


// ---------------------------------------------------------------------------
// Simple JSON Parser for pkgdb.json
// ---------------------------------------------------------------------------

/**
 * Skips whitespace characters in a string.
 *
 * @param s String to process.
 * @return Pointer to the first non-whitespace character.
 */
static const char *skip_ws(const char *s) {
  while (*s && isspace((unsigned char)*s)) {
    s++;
  }
  return s;
}


/**
 * Parses a JSON string value.
 *
 * Handles escape sequences like \n, \t, \r, \", and \\.
 *
 * @param p Pointer to current position in JSON (will be updated to position
 *          after the parsed string).
 * @return Dynamically allocated unescaped string, or NULL on parse error.
 *         Caller must free the returned string.
 */
static char *parse_str(const char **p) {
  const char *s = *p;
  s = skip_ws(s);

  if (*s != '"') {
    return NULL;
  }
  s++;

  const char *start = s;
  while (*s && *s != '"') {
    if (*s == '\\' && *(s + 1)) {
      s += 2;
    } else {
      s++;
    }
  }

  if (*s != '"') {
    return NULL;
  }

  size_t len = (size_t)(s - start);
  char *result = malloc(len + 1);
  if (result == NULL) {
    return NULL;
  }

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    if (start[i] == '\\' && i + 1 < len) {
      i++;
      switch (start[i]) {
        case 'n': result[j++] = '\n'; break;
        case 't': result[j++] = '\t'; break;
        case 'r': result[j++] = '\r'; break;
        case '"': result[j++] = '"'; break;
        case '\\': result[j++] = '\\'; break;
        default: result[j++] = start[i]; break;
      }
    } else {
      result[j++] = start[i];
    }
  }
  result[j] = '\0';

  *p = s + 1;
  return result;
}


/**
 * Parses a JSON array of strings.
 *
 * @param p Pointer to current position in JSON (will be updated to position
 *          after the parsed array).
 * @param count Output parameter for number of strings in the array.
 * @return Dynamically allocated array of string pointers, or NULL on parse
 *         error. Both the array and each string must be freed by the caller.
 */
static char **parse_str_array(const char **p, int *count) {
  const char *s = *p;
  s = skip_ws(s);

  if (*s != '[') {
    *count = 0;
    return NULL;
  }
  s++;

  char **items = NULL;
  int capacity = 0;
  *count = 0;

  while (1) {
    s = skip_ws(s);

    if (*s == ']') {
      s++;
      break;
    }

    if (*count > 0) {
      if (*s != ',') {
        goto error;
      }
      s++;
      s = skip_ws(s);
    }

    char *item = parse_str(&s);
    if (item == NULL) {
      goto error;
    }

    if (*count >= capacity) {
      capacity = capacity == 0 ? 4 : capacity * 2;
      char **new_items = realloc(items, (size_t)capacity * sizeof(char *));
      if (new_items == NULL) {
        free(item);
        goto error;
      }
      items = new_items;
    }

    items[*count] = item;
    (*count)++;
  }

  *p = s;
  return items;

error:
  for (int i = 0; i < *count; i++) {
    free(items[i]);
  }
  free(items);
  *count = 0;
  return NULL;
}


/**
 * Skips over a JSON value without parsing it.
 *
 * Handles strings, arrays, objects, and primitive values.
 *
 * @param s Current position in JSON.
 * @return Pointer to position after the skipped value.
 */
static const char *skip_val(const char *s) {
  s = skip_ws(s);

  if (*s == '"') {
    s++;
    while (*s && *s != '"') {
      if (*s == '\\' && *(s + 1)) {
        s += 2;
      } else {
        s++;
      }
    }
    if (*s == '"') s++;
    return s;
  }

  if (*s == '[') {
    s++;
    int depth = 1;
    while (*s && depth > 0) {
      if (*s == '[') depth++;
      else if (*s == ']') depth--;
      else if (*s == '"') {
        s++;
        while (*s && *s != '"') {
          if (*s == '\\' && *(s + 1)) s += 2;
          else s++;
        }
        if (*s == '"') s++;
        continue;
      }
      s++;
    }
    return s;
  }

  if (*s == '{') {
    s++;
    int depth = 1;
    while (*s && depth > 0) {
      if (*s == '{') depth++;
      else if (*s == '}') depth--;
      else if (*s == '"') {
        s++;
        while (*s && *s != '"') {
          if (*s == '\\' && *(s + 1)) s += 2;
          else s++;
        }
        if (*s == '"') s++;
        continue;
      }
      s++;
    }
    return s;
  }

  while (*s && *s != ',' && *s != '}' && *s != ']') {
    s++;
  }
  return s;
}


/**
 * Package entry structure for parsing pkgdb.json entries.
 */
typedef struct {
  char *name;         /** Package name */
  char *description;  /** Package description */
  char **files;       /** Array of file paths in the package */
  int files_count;    /** Number of files in the package */
} PkgEntry;


/**
 * Frees memory allocated for a package entry.
 *
 * @param e Package entry to free.
 */
static void free_pkg_entry(PkgEntry *e) {
  if (e == NULL) return;
  free(e->name);
  free(e->description);
  for (int i = 0; i < e->files_count; i++) {
    free(e->files[i]);
  }
  free(e->files);
}


/**
 * Parses a single package entry object from JSON.
 *
 * @param p Pointer to current position in JSON (will be updated to position
 *          after the parsed entry).
 * @param entry Package entry structure to populate.
 * @return 0 on success, -1 on parse error.
 */
static int parse_pkg_entry(const char **p, PkgEntry *entry) {
  const char *s = *p;
  s = skip_ws(s);

  if (*s != '{') {
    return -1;
  }
  s++;

  memset(entry, 0, sizeof(PkgEntry));

  int first = 1;
  while (1) {
    s = skip_ws(s);

    if (*s == '}') {
      s++;
      break;
    }

    if (!first) {
      if (*s != ',') {
        goto error;
      }
      s++;
      s = skip_ws(s);
    }
    first = 0;

    char *key = parse_str(&s);
    if (key == NULL) {
      goto error;
    }

    s = skip_ws(s);
    if (*s != ':') {
      free(key);
      goto error;
    }
    s++;
    s = skip_ws(s);

    if (strcmp(key, "name") == 0) {
      entry->name = parse_str(&s);
    } else if (strcmp(key, "description") == 0) {
      entry->description = parse_str(&s);
    } else if (strcmp(key, "files") == 0) {
      entry->files = parse_str_array(&s, &entry->files_count);
    } else {
      s = skip_val(s);
    }

    free(key);
  }

  *p = s;
  return 0;

error:
  free_pkg_entry(entry);
  return -1;
}


// ---------------------------------------------------------------------------
// Package Loading
// ---------------------------------------------------------------------------

/**
 * Reads an entire file into memory.
 *
 * @param path Path to the file to read.
 * @return Dynamically allocated string containing file contents, or NULL on
 *         error. Caller must free the returned string.
 */
static char *read_file(const char *path) {
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    return NULL;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }

  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return NULL;
  }

  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return NULL;
  }

  char *content = malloc((size_t)size + 1);
  if (content == NULL) {
    fclose(f);
    return NULL;
  }

  size_t read = fread(content, 1, (size_t)size, f);
  fclose(f);

  content[read] = '\0';
  return content;
}


/**
 * Loads installed packages from pkgdb.json and registers them with the shell.
 *
 * Parses ~/.jshell/pkgs/pkgdb.json and registers each package's commands
 * with the shell's command registry. This allows installed packages to be
 * executed as commands.
 *
 * @return Number of package commands successfully loaded, or 0 if no packages
 *         or database doesn't exist. Returns -1 on critical errors.
 */
int jshell_load_installed_packages(void) {
  char *db_path = get_pkgdb_path();
  if (db_path == NULL) {
    return 0;  // No packages, not an error
  }

  // Check if file exists
  struct stat st;
  if (stat(db_path, &st) != 0) {
    free(db_path);
    return 0;  // No database file, no packages
  }

  char *content = read_file(db_path);
  free(db_path);

  if (content == NULL) {
    return 0;  // Can't read file, no packages
  }

  char *bin_dir = get_bin_dir();
  if (bin_dir == NULL) {
    free(content);
    return -1;
  }

  int packages_loaded = 0;
  const char *s = content;
  s = skip_ws(s);

  if (*s != '{') {
    free(content);
    free(bin_dir);
    return 0;
  }
  s++;

  // Find "packages" array
  int first = 1;
  while (1) {
    s = skip_ws(s);

    if (*s == '}') {
      break;
    }

    if (!first) {
      if (*s != ',') {
        break;
      }
      s++;
      s = skip_ws(s);
    }
    first = 0;

    char *key = parse_str(&s);
    if (key == NULL) {
      break;
    }

    s = skip_ws(s);
    if (*s != ':') {
      free(key);
      break;
    }
    s++;
    s = skip_ws(s);

    if (strcmp(key, "packages") == 0) {
      // Parse packages array
      if (*s != '[') {
        free(key);
        break;
      }
      s++;

      int first_pkg = 1;
      while (1) {
        s = skip_ws(s);

        if (*s == ']') {
          s++;
          break;
        }

        if (!first_pkg) {
          if (*s != ',') {
            break;
          }
          s++;
          s = skip_ws(s);
        }
        first_pkg = 0;

        PkgEntry entry;
        if (parse_pkg_entry(&s, &entry) != 0) {
          break;
        }

        // Register command for each file in the package
        if (entry.name != NULL && entry.files != NULL) {
          for (int i = 0; i < entry.files_count; i++) {
            char *file = entry.files[i];
            // Get basename of file (e.g., "bin/ls" -> "ls")
            char *file_copy = strdup(file);
            if (file_copy == NULL) continue;
            char *cmd_name = basename(file_copy);

            // Build full path to binary
            size_t path_len = strlen(bin_dir) + strlen(cmd_name) + 2;
            char *bin_path = malloc(path_len);
            if (bin_path != NULL) {
              snprintf(bin_path, path_len, "%s/%s", bin_dir, cmd_name);

              // Check if binary exists
              if (stat(bin_path, &st) == 0) {
                if (jshell_register_package_command(cmd_name, entry.description,
                                                    bin_path) == 0) {
                  packages_loaded++;
                }
              }

              free(bin_path);
            }
            free(file_copy);
          }
        }

        free_pkg_entry(&entry);
      }
    } else {
      s = skip_val(s);
    }

    free(key);
  }

  free(content);
  free(bin_dir);

  return packages_loaded;
}


/**
 * Reloads all installed packages.
 *
 * Unregisters all currently loaded package commands and re-parses the
 * package database. Useful after package installation or removal.
 *
 * @return Number of package commands successfully loaded.
 */
int jshell_reload_packages(void) {
  jshell_unregister_all_package_commands();
  return jshell_load_installed_packages();
}
