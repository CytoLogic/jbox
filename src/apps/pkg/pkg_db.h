#ifndef PKG_DB_H
#define PKG_DB_H

#define PKG_DB_VERSION 1

typedef struct {
  char *name;
  char *version;
  char *installed_at;      // ISO 8601 timestamp
  char *description;       // Optional description
  char **files;            // Array of installed files
  int files_count;
} PkgDbEntry;

typedef struct {
  int db_version;
  PkgDbEntry *entries;
  int count;
  int capacity;
} PkgDb;

// Load package database (prefers JSON, falls back to txt with migration)
// Returns empty db if file doesn't exist
// Returns NULL on error
PkgDb *pkg_db_load(void);

// Save package database to JSON format
// Returns 0 on success, -1 on error
int pkg_db_save(const PkgDb *db);

// Free all memory associated with database
void pkg_db_free(PkgDb *db);

// Free all memory associated with a single entry (does not free entry itself)
void pkg_db_entry_free(PkgDbEntry *entry);

// Find entry by name (returns pointer to entry in db, or NULL)
PkgDbEntry *pkg_db_find(const PkgDb *db, const char *name);

// Add entry to database (copies all strings)
// Returns 0 on success, -1 on error
int pkg_db_add(PkgDb *db, const char *name, const char *version);

// Add entry with full metadata (copies all strings and arrays)
// Returns 0 on success, -1 on error
int pkg_db_add_full(PkgDb *db, const char *name, const char *version,
                    const char *description, char **files, int files_count);

// Remove entry from database by name
// Returns 0 on success, -1 if not found
int pkg_db_remove(PkgDb *db, const char *name);

// Load package database from JSON file
// Returns empty db if file doesn't exist
// Returns NULL on error
PkgDb *pkg_db_load_json(void);

// Save package database to JSON file
// Returns 0 on success, -1 on error
int pkg_db_save_json(const PkgDb *db);

// Migrate from txt format to JSON format
// Returns 0 on success, -1 on error
int pkg_db_migrate_from_txt(void);

// Get current ISO 8601 timestamp (caller must free)
char *pkg_db_get_timestamp(void);

#endif
