#ifndef PKG_DB_H
#define PKG_DB_H

typedef struct {
  char *name;
  char *version;
} PkgDbEntry;

typedef struct {
  PkgDbEntry *entries;
  int count;
  int capacity;
} PkgDb;

// Load package database from ~/.jshell/pkgdb.txt
// Returns empty db if file doesn't exist
// Returns NULL on error
PkgDb *pkg_db_load(void);

// Save package database to ~/.jshell/pkgdb.txt
// Returns 0 on success, -1 on error
int pkg_db_save(const PkgDb *db);

// Free all memory associated with database
void pkg_db_free(PkgDb *db);

// Find entry by name (returns pointer to entry in db, or NULL)
PkgDbEntry *pkg_db_find(const PkgDb *db, const char *name);

// Add entry to database (copies name and version strings)
// Returns 0 on success, -1 on error
int pkg_db_add(PkgDb *db, const char *name, const char *version);

// Remove entry from database by name
// Returns 0 on success, -1 if not found
int pkg_db_remove(PkgDb *db, const char *name);

#endif
