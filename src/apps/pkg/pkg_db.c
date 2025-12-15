#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pkg_db.h"
#include "pkg_utils.h"


PkgDb *pkg_db_load(void) {
  PkgDb *db = calloc(1, sizeof(PkgDb));
  if (db == NULL) {
    return NULL;
  }

  char *db_path = pkg_get_db_path();
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


int pkg_db_save(const PkgDb *db) {
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

  for (int i = 0; i < db->count; i++) {
    fprintf(f, "%s %s\n", db->entries[i].name, db->entries[i].version);
  }

  fclose(f);
  return 0;
}


void pkg_db_free(PkgDb *db) {
  if (db == NULL) {
    return;
  }

  for (int i = 0; i < db->count; i++) {
    free(db->entries[i].name);
    free(db->entries[i].version);
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


int pkg_db_add(PkgDb *db, const char *name, const char *version) {
  if (db == NULL || name == NULL || version == NULL) {
    return -1;
  }

  PkgDbEntry *existing = pkg_db_find(db, name);
  if (existing != NULL) {
    char *new_version = strdup(version);
    if (new_version == NULL) {
      return -1;
    }
    free(existing->version);
    existing->version = new_version;
    return 0;
  }

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

  char *name_copy = strdup(name);
  char *version_copy = strdup(version);

  if (name_copy == NULL || version_copy == NULL) {
    free(name_copy);
    free(version_copy);
    return -1;
  }

  db->entries[db->count].name = name_copy;
  db->entries[db->count].version = version_copy;
  db->count++;

  return 0;
}


int pkg_db_remove(PkgDb *db, const char *name) {
  if (db == NULL || name == NULL) {
    return -1;
  }

  for (int i = 0; i < db->count; i++) {
    if (strcmp(db->entries[i].name, name) == 0) {
      free(db->entries[i].name);
      free(db->entries[i].version);

      for (int j = i; j < db->count - 1; j++) {
        db->entries[j] = db->entries[j + 1];
      }
      db->count--;

      return 0;
    }
  }

  return -1;
}
