#ifndef PKG_JSON_H
#define PKG_JSON_H

typedef struct {
  char *name;
  char *version;
  char *description;
  char **files;
  int files_count;
  char **docs;
  int docs_count;
} PkgManifest;

// Parse pkg.json from a JSON string
// Returns NULL on parse error
PkgManifest *pkg_manifest_parse(const char *json_str);

// Load and parse pkg.json from a file path
// Returns NULL on error
PkgManifest *pkg_manifest_load(const char *path);

// Free all memory associated with a manifest
void pkg_manifest_free(PkgManifest *m);

// Validate manifest has required fields
// Returns 0 if valid, -1 if invalid
int pkg_manifest_validate(const PkgManifest *m);

#endif
