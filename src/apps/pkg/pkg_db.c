#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "pkg_db.h"
#include "pkg_utils.h"


// Forward declarations for JSON parsing helpers
static const char *skip_whitespace(const char *s);
static char *parse_string(const char **p);
static char **parse_string_array(const char **p, int *count);
static const char *skip_value(const char *s);
static int parse_int(const char **p);


char *pkg_db_get_timestamp(void) {
  time_t now = time(NULL);
  struct tm *tm_info = gmtime(&now);
  if (tm_info == NULL) {
    return NULL;
  }

  char *buf = malloc(32);
  if (buf == NULL) {
    return NULL;
  }

  strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", tm_info);
  return buf;
}


void pkg_db_entry_free(PkgDbEntry *entry) {
  if (entry == NULL) {
    return;
  }

  free(entry->name);
  free(entry->version);
  free(entry->installed_at);
  free(entry->description);

  for (int i = 0; i < entry->files_count; i++) {
    free(entry->files[i]);
  }
  free(entry->files);

  entry->name = NULL;
  entry->version = NULL;
  entry->installed_at = NULL;
  entry->description = NULL;
  entry->files = NULL;
  entry->files_count = 0;
}


void pkg_db_free(PkgDb *db) {
  if (db == NULL) {
    return;
  }

  for (int i = 0; i < db->count; i++) {
    pkg_db_entry_free(&db->entries[i]);
  }
  free(db->entries);
  free(db);
}


PkgDbEntry *pkg_db_find(const PkgDb *db, const char *name) {
  if (db == NULL || name == NULL) {
    return NULL;
  }

  for (int i = 0; i < db->count; i++) {
    if (strcmp(db->entries[i].name, name) == 0) {
      return &db->entries[i];
    }
  }

  return NULL;
}


static int pkg_db_ensure_capacity(PkgDb *db) {
  if (db->count >= db->capacity) {
    int new_capacity = db->capacity == 0 ? 8 : db->capacity * 2;
    PkgDbEntry *new_entries = realloc(db->entries,
                                      (size_t)new_capacity * sizeof(PkgDbEntry));
    if (new_entries == NULL) {
      return -1;
    }
    db->entries = new_entries;
    db->capacity = new_capacity;
  }
  return 0;
}


int pkg_db_add(PkgDb *db, const char *name, const char *version) {
  return pkg_db_add_full(db, name, version, NULL, NULL, 0);
}


int pkg_db_add_full(PkgDb *db, const char *name, const char *version,
                    const char *description, char **files, int files_count) {
  if (db == NULL || name == NULL || version == NULL) {
    return -1;
  }

  PkgDbEntry *existing = pkg_db_find(db, name);
  if (existing != NULL) {
    // Update existing entry
    char *new_version = strdup(version);
    if (new_version == NULL) {
      return -1;
    }
    free(existing->version);
    existing->version = new_version;

    // Update timestamp
    free(existing->installed_at);
    existing->installed_at = pkg_db_get_timestamp();

    // Update description if provided
    if (description != NULL) {
      free(existing->description);
      existing->description = strdup(description);
    }

    // Update files if provided
    if (files != NULL && files_count > 0) {
      for (int i = 0; i < existing->files_count; i++) {
        free(existing->files[i]);
      }
      free(existing->files);

      existing->files = malloc((size_t)files_count * sizeof(char *));
      if (existing->files == NULL) {
        existing->files_count = 0;
        return -1;
      }
      existing->files_count = files_count;
      for (int i = 0; i < files_count; i++) {
        existing->files[i] = strdup(files[i]);
        if (existing->files[i] == NULL) {
          for (int j = 0; j < i; j++) {
            free(existing->files[j]);
          }
          free(existing->files);
          existing->files = NULL;
          existing->files_count = 0;
          return -1;
        }
      }
    }

    return 0;
  }

  // Add new entry
  if (pkg_db_ensure_capacity(db) != 0) {
    return -1;
  }

  PkgDbEntry *entry = &db->entries[db->count];
  memset(entry, 0, sizeof(PkgDbEntry));

  entry->name = strdup(name);
  entry->version = strdup(version);
  entry->installed_at = pkg_db_get_timestamp();

  if (entry->name == NULL || entry->version == NULL) {
    pkg_db_entry_free(entry);
    return -1;
  }

  if (description != NULL) {
    entry->description = strdup(description);
  }

  if (files != NULL && files_count > 0) {
    entry->files = malloc((size_t)files_count * sizeof(char *));
    if (entry->files == NULL) {
      pkg_db_entry_free(entry);
      return -1;
    }
    entry->files_count = files_count;
    for (int i = 0; i < files_count; i++) {
      entry->files[i] = strdup(files[i]);
      if (entry->files[i] == NULL) {
        entry->files_count = i;
        pkg_db_entry_free(entry);
        return -1;
      }
    }
  }

  db->count++;
  return 0;
}


int pkg_db_remove(PkgDb *db, const char *name) {
  if (db == NULL || name == NULL) {
    return -1;
  }

  for (int i = 0; i < db->count; i++) {
    if (strcmp(db->entries[i].name, name) == 0) {
      pkg_db_entry_free(&db->entries[i]);

      for (int j = i; j < db->count - 1; j++) {
        db->entries[j] = db->entries[j + 1];
      }
      db->count--;

      return 0;
    }
  }

  return -1;
}


// ---------------------------------------------------------------------------
// JSON Parsing Helpers
// ---------------------------------------------------------------------------

static const char *skip_whitespace(const char *s) {
  while (*s && isspace((unsigned char)*s)) {
    s++;
  }
  return s;
}


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


static int parse_int(const char **p) {
  const char *s = *p;
  s = skip_whitespace(s);

  int value = 0;
  int sign = 1;

  if (*s == '-') {
    sign = -1;
    s++;
  }

  while (*s >= '0' && *s <= '9') {
    value = value * 10 + (*s - '0');
    s++;
  }

  *p = s;
  return value * sign;
}


static char **parse_string_array(const char **p, int *count) {
  const char *s = *p;
  s = skip_whitespace(s);

  if (*s != '[') {
    *count = 0;
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


// ---------------------------------------------------------------------------
// JSON Database Loading
// ---------------------------------------------------------------------------

static PkgDbEntry *parse_package_entry(const char **p) {
  const char *s = *p;
  s = skip_whitespace(s);

  if (*s != '{') {
    return NULL;
  }
  s++;

  PkgDbEntry *entry = calloc(1, sizeof(PkgDbEntry));
  if (entry == NULL) {
    return NULL;
  }

  int first = 1;
  while (1) {
    s = skip_whitespace(s);

    if (*s == '}') {
      s++;
      break;
    }

    if (!first) {
      if (*s != ',') {
        goto error;
      }
      s++;
      s = skip_whitespace(s);
    }
    first = 0;

    char *key = parse_string(&s);
    if (key == NULL) {
      goto error;
    }

    s = skip_whitespace(s);
    if (*s != ':') {
      free(key);
      goto error;
    }
    s++;
    s = skip_whitespace(s);

    if (strcmp(key, "name") == 0) {
      entry->name = parse_string(&s);
    } else if (strcmp(key, "version") == 0) {
      entry->version = parse_string(&s);
    } else if (strcmp(key, "installed_at") == 0) {
      entry->installed_at = parse_string(&s);
    } else if (strcmp(key, "description") == 0) {
      entry->description = parse_string(&s);
    } else if (strcmp(key, "files") == 0) {
      entry->files = parse_string_array(&s, &entry->files_count);
    } else {
      s = skip_value(s);
    }

    free(key);
  }

  *p = s;
  return entry;

error:
  pkg_db_entry_free(entry);
  free(entry);
  return NULL;
}


PkgDb *pkg_db_load_json(void) {
  PkgDb *db = calloc(1, sizeof(PkgDb));
  if (db == NULL) {
    return NULL;
  }
  db->db_version = PKG_DB_VERSION;

  char *db_path = pkg_get_db_path();
  if (db_path == NULL) {
    free(db);
    return NULL;
  }

  char *content = pkg_read_file(db_path);
  free(db_path);

  if (content == NULL) {
    // File doesn't exist, return empty db
    return db;
  }

  const char *s = content;
  s = skip_whitespace(s);

  if (*s != '{') {
    free(content);
    return db;
  }
  s++;

  int first = 1;
  while (1) {
    s = skip_whitespace(s);

    if (*s == '}') {
      break;
    }

    if (!first) {
      if (*s != ',') {
        break;
      }
      s++;
      s = skip_whitespace(s);
    }
    first = 0;

    char *key = parse_string(&s);
    if (key == NULL) {
      break;
    }

    s = skip_whitespace(s);
    if (*s != ':') {
      free(key);
      break;
    }
    s++;
    s = skip_whitespace(s);

    if (strcmp(key, "version") == 0) {
      db->db_version = parse_int(&s);
    } else if (strcmp(key, "packages") == 0) {
      // Parse packages array
      if (*s != '[') {
        free(key);
        break;
      }
      s++;

      int first_pkg = 1;
      while (1) {
        s = skip_whitespace(s);

        if (*s == ']') {
          s++;
          break;
        }

        if (!first_pkg) {
          if (*s != ',') {
            break;
          }
          s++;
          s = skip_whitespace(s);
        }
        first_pkg = 0;

        PkgDbEntry *entry = parse_package_entry(&s);
        if (entry == NULL) {
          break;
        }

        if (pkg_db_ensure_capacity(db) != 0) {
          pkg_db_entry_free(entry);
          free(entry);
          break;
        }

        db->entries[db->count] = *entry;
        free(entry);  // Just free the container, not the contents
        db->count++;
      }
    } else {
      s = skip_value(s);
    }

    free(key);
  }

  free(content);
  return db;
}


// ---------------------------------------------------------------------------
// JSON Database Saving
// ---------------------------------------------------------------------------

static void write_escaped_string(FILE *f, const char *str) {
  fputc('"', f);
  if (str != NULL) {
    for (const char *p = str; *p; p++) {
      switch (*p) {
        case '"': fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n", f); break;
        case '\t': fputs("\\t", f); break;
        case '\r': fputs("\\r", f); break;
        default: fputc(*p, f); break;
      }
    }
  }
  fputc('"', f);
}


int pkg_db_save_json(const PkgDb *db) {
  if (pkg_ensure_dirs() != 0) {
    return -1;
  }

  char *db_path = pkg_get_db_path();
  if (db_path == NULL) {
    return -1;
  }

  FILE *f = fopen(db_path, "w");
  free(db_path);

  if (f == NULL) {
    return -1;
  }

  fprintf(f, "{\n");
  fprintf(f, "  \"version\": %d,\n", PKG_DB_VERSION);
  fprintf(f, "  \"packages\": [");

  for (int i = 0; i < db->count; i++) {
    if (i > 0) {
      fprintf(f, ",");
    }
    fprintf(f, "\n    {\n");

    fprintf(f, "      \"name\": ");
    write_escaped_string(f, db->entries[i].name);
    fprintf(f, ",\n");

    fprintf(f, "      \"version\": ");
    write_escaped_string(f, db->entries[i].version);

    if (db->entries[i].installed_at != NULL) {
      fprintf(f, ",\n      \"installed_at\": ");
      write_escaped_string(f, db->entries[i].installed_at);
    }

    if (db->entries[i].description != NULL) {
      fprintf(f, ",\n      \"description\": ");
      write_escaped_string(f, db->entries[i].description);
    }

    if (db->entries[i].files != NULL && db->entries[i].files_count > 0) {
      fprintf(f, ",\n      \"files\": [");
      for (int j = 0; j < db->entries[i].files_count; j++) {
        if (j > 0) {
          fprintf(f, ", ");
        }
        write_escaped_string(f, db->entries[i].files[j]);
      }
      fprintf(f, "]");
    }

    fprintf(f, "\n    }");
  }

  if (db->count > 0) {
    fprintf(f, "\n  ");
  }
  fprintf(f, "]\n}\n");

  fclose(f);
  return 0;
}


// ---------------------------------------------------------------------------
// Legacy TXT Database Loading
// ---------------------------------------------------------------------------

static PkgDb *pkg_db_load_txt(void) {
  PkgDb *db = calloc(1, sizeof(PkgDb));
  if (db == NULL) {
    return NULL;
  }
  db->db_version = PKG_DB_VERSION;

  char *db_path = pkg_get_db_path_txt();
  if (db_path == NULL) {
    free(db);
    return NULL;
  }

  FILE *f = fopen(db_path, "r");
  free(db_path);

  if (f == NULL) {
    if (errno == ENOENT) {
      return db;
    }
    free(db);
    return NULL;
  }

  char line[1024];
  while (fgets(line, sizeof(line), f) != NULL) {
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }

    if (line[0] == '\0') {
      continue;
    }

    char *space = strchr(line, ' ');
    if (space == NULL) {
      continue;
    }

    *space = '\0';
    const char *name = line;
    const char *version = space + 1;

    if (pkg_db_add(db, name, version) != 0) {
      fclose(f);
      pkg_db_free(db);
      return NULL;
    }
  }

  fclose(f);
  return db;
}


// ---------------------------------------------------------------------------
// Migration from TXT to JSON
// ---------------------------------------------------------------------------

int pkg_db_migrate_from_txt(void) {
  // Check if txt database exists
  char *txt_path = pkg_get_db_path_txt();
  if (txt_path == NULL) {
    return -1;
  }

  struct stat st;
  if (stat(txt_path, &st) != 0) {
    free(txt_path);
    return 0;  // Nothing to migrate
  }

  // Load from txt
  PkgDb *db = pkg_db_load_txt();
  free(txt_path);

  if (db == NULL) {
    return -1;
  }

  // Save as JSON
  int result = pkg_db_save_json(db);
  pkg_db_free(db);

  if (result != 0) {
    return -1;
  }

  // Rename old txt file to backup
  txt_path = pkg_get_db_path_txt();
  if (txt_path != NULL) {
    size_t len = strlen(txt_path) + strlen(".bak") + 1;
    char *bak_path = malloc(len);
    if (bak_path != NULL) {
      snprintf(bak_path, len, "%s.bak", txt_path);
      rename(txt_path, bak_path);
      free(bak_path);
    }
    free(txt_path);
  }

  return 0;
}


// ---------------------------------------------------------------------------
// Main Load/Save API
// ---------------------------------------------------------------------------

PkgDb *pkg_db_load(void) {
  // First, try to load JSON database
  char *json_path = pkg_get_db_path();
  if (json_path == NULL) {
    return NULL;
  }

  struct stat st;
  int json_exists = (stat(json_path, &st) == 0);
  free(json_path);

  if (json_exists) {
    return pkg_db_load_json();
  }

  // Check if txt database exists
  char *txt_path = pkg_get_db_path_txt();
  if (txt_path == NULL) {
    return NULL;
  }

  int txt_exists = (stat(txt_path, &st) == 0);
  free(txt_path);

  if (txt_exists) {
    // Migrate from txt to JSON
    if (pkg_db_migrate_from_txt() == 0) {
      return pkg_db_load_json();
    }
    // If migration failed, fall back to txt
    return pkg_db_load_txt();
  }

  // No database exists, return empty db
  PkgDb *db = calloc(1, sizeof(PkgDb));
  if (db != NULL) {
    db->db_version = PKG_DB_VERSION;
  }
  return db;
}


int pkg_db_save(const PkgDb *db) {
  return pkg_db_save_json(db);
}
