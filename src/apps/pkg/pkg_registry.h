#ifndef PKG_REGISTRY_H
#define PKG_REGISTRY_H

#include <stdbool.h>

// Default registry URL (can be overridden by JSHELL_PKG_REGISTRY env var)
#define PKG_REGISTRY_DEFAULT_URL "http://localhost:3000"

// Registry package entry
typedef struct {
  char *name;
  char *latest_version;
  char *description;
  char *download_url;
} PkgRegistryEntry;

// Registry response containing multiple packages
typedef struct {
  PkgRegistryEntry *entries;
  int count;
  int capacity;
} PkgRegistryList;

// Get the registry URL (from env or default)
const char *pkg_registry_get_url(void);

// Fetch all packages from registry
// Returns NULL on error, caller must free with pkg_registry_list_free()
PkgRegistryList *pkg_registry_fetch_all(void);

// Fetch a single package by name
// Returns NULL if not found or on error, caller must free with
// pkg_registry_entry_free()
PkgRegistryEntry *pkg_registry_fetch_package(const char *name);

// Search packages by name (substring match)
// Returns NULL on error, caller must free with pkg_registry_list_free()
PkgRegistryList *pkg_registry_search(const char *query);

// Download a package tarball to a local path
// Returns 0 on success, -1 on error
int pkg_registry_download(const char *url, const char *dest_path);

// Free a registry entry
void pkg_registry_entry_free(PkgRegistryEntry *entry);

// Free a registry list
void pkg_registry_list_free(PkgRegistryList *list);

// Compare version strings (semver-style)
// Returns: -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
int pkg_version_compare(const char *v1, const char *v2);

#endif
