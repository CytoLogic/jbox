/** @file pkg_json.c
 *  @brief Package manifest (pkg.json) parsing and validation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "pkg_json.h"
#include "pkg_utils.h"


/** Skips whitespace characters in a string.
 *  @param s Input string
 *  @return Pointer to first non-whitespace character
 */
static const char *skip_whitespace(const char *s) {
  while (*s && isspace((unsigned char)*s)) {
    s++;
  }
  return s;
}


/** Parses a JSON string value.
 *  @param p Pointer to string pointer (updated after parse)
 *  @return Allocated string value, or NULL on error. Caller must free.
 */
static char *parse_string(const char **p) {
  const char *s = *p;
  s = skip_whitespace(s);

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


/** Parses a JSON array of strings.
 *  @param p Pointer to string pointer (updated after parse)
 *  @param count Output parameter for array length
 *  @return Allocated array of strings, or NULL on error. Caller must free.
 */
static char **parse_string_array(const char **p, int *count) {
  const char *s = *p;
  s = skip_whitespace(s);

  if (*s != '[') {
    return NULL;
  }
  s++;

  char **items = NULL;
  int capacity = 0;
  *count = 0;

  while (1) {
    s = skip_whitespace(s);

    if (*s == ']') {
      s++;
      break;
    }

    if (*count > 0) {
      if (*s != ',') {
        goto error;
      }
      s++;
      s = skip_whitespace(s);
    }

    char *item = parse_string(&s);
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


/** Parses a JSON key-value pair.
 *  @param p Pointer to string pointer (updated after parse)
 *  @param key Output parameter for key string (caller must free)
 *  @param value_start Output parameter pointing to value
 *  @return 0 on success, -1 on error
 */
static int parse_key_value(const char **p, char **key, const char **value_start) {
  const char *s = *p;
  s = skip_whitespace(s);

  *key = parse_string(&s);
  if (*key == NULL) {
    return -1;
  }

  s = skip_whitespace(s);
  if (*s != ':') {
    free(*key);
    *key = NULL;
    return -1;
  }
  s++;

  s = skip_whitespace(s);
  *value_start = s;
  *p = s;
  return 0;
}


/** Skips over a JSON value without parsing.
 *  @param s Input string
 *  @return Pointer past the JSON value
 */
static const char *skip_value(const char *s) {
  s = skip_whitespace(s);

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


/** Parses a package manifest from JSON string.
 *  @param json_str JSON string containing manifest
 *  @return Allocated PkgManifest, or NULL on error. Caller must free with pkg_manifest_free.
 */
PkgManifest *pkg_manifest_parse(const char *json_str) {
  PkgManifest *m = calloc(1, sizeof(PkgManifest));
  if (m == NULL) {
    return NULL;
  }

  const char *s = json_str;
  s = skip_whitespace(s);

  if (*s != '{') {
    free(m);
    return NULL;
  }
  s++;

  while (1) {
    s = skip_whitespace(s);

    if (*s == '}') {
      break;
    }

    if (m->name != NULL || m->version != NULL || m->description != NULL
        || m->files != NULL || m->docs != NULL) {
      if (*s != ',') {
        goto error;
      }
      s++;
      s = skip_whitespace(s);
    }

    char *key = NULL;
    const char *value_start = NULL;

    if (parse_key_value(&s, &key, &value_start) != 0) {
      goto error;
    }

    if (strcmp(key, "name") == 0) {
      m->name = parse_string(&s);
      if (m->name == NULL) {
        free(key);
        goto error;
      }
    } else if (strcmp(key, "version") == 0) {
      m->version = parse_string(&s);
      if (m->version == NULL) {
        free(key);
        goto error;
      }
    } else if (strcmp(key, "description") == 0) {
      m->description = parse_string(&s);
      if (m->description == NULL) {
        free(key);
        goto error;
      }
    } else if (strcmp(key, "files") == 0) {
      m->files = parse_string_array(&s, &m->files_count);
      if (m->files == NULL && m->files_count != 0) {
        free(key);
        goto error;
      }
    } else if (strcmp(key, "docs") == 0) {
      m->docs = parse_string_array(&s, &m->docs_count);
      if (m->docs == NULL && m->docs_count != 0) {
        free(key);
        goto error;
      }
    } else {
      s = skip_value(s);
    }

    free(key);
  }

  return m;

error:
  pkg_manifest_free(m);
  return NULL;
}


/** Loads a package manifest from a file.
 *  @param path Path to pkg.json file
 *  @return Allocated PkgManifest, or NULL on error. Caller must free with pkg_manifest_free.
 */
PkgManifest *pkg_manifest_load(const char *path) {
  char *content = pkg_read_file(path);
  if (content == NULL) {
    return NULL;
  }

  PkgManifest *m = pkg_manifest_parse(content);
  free(content);
  return m;
}


/** Frees a package manifest and all its contents.
 *  @param m Pointer to manifest to free
 */
void pkg_manifest_free(PkgManifest *m) {
  if (m == NULL) {
    return;
  }

  free(m->name);
  free(m->version);
  free(m->description);

  for (int i = 0; i < m->files_count; i++) {
    free(m->files[i]);
  }
  free(m->files);

  for (int i = 0; i < m->docs_count; i++) {
    free(m->docs[i]);
  }
  free(m->docs);

  free(m);
}


/** Validates that a package manifest has all required fields.
 *  @param m Pointer to manifest to validate
 *  @return 0 if valid, -1 if invalid
 */
int pkg_manifest_validate(const PkgManifest *m) {
  if (m == NULL) {
    return -1;
  }

  if (m->name == NULL || strlen(m->name) == 0) {
    return -1;
  }

  if (m->version == NULL || strlen(m->version) == 0) {
    return -1;
  }

  if (m->files == NULL || m->files_count == 0) {
    return -1;
  }

  return 0;
}
