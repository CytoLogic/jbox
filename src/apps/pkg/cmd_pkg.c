#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "pkg_utils.h"
#include "pkg_json.h"
#include "pkg_db.h"
#include "pkg_registry.h"


typedef enum {
  PKG_CMD_NONE,
  PKG_CMD_LIST,
  PKG_CMD_INFO,
  PKG_CMD_SEARCH,
  PKG_CMD_INSTALL,
  PKG_CMD_REMOVE,
  PKG_CMD_BUILD,
  PKG_CMD_CHECK_UPDATE,
  PKG_CMD_UPGRADE,
  PKG_CMD_COMPILE
} pkg_subcommand_t;


typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_rex *subcmd;
  struct arg_str *args;
  struct arg_end *end;
  void *argtable[5];
} pkg_args_t;


// Forward declarations
static int compile_package_dir(const char *name, const char *dir,
                               int json_output, int verbose);


static void build_pkg_argtable(pkg_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->subcmd = arg_rex1(NULL, NULL,
    "list|info|search|install|remove|build|check-update|upgrade|compile",
    "COMMAND", ARG_REX_ICASE, "subcommand to run");
  args->args = arg_strn(NULL, NULL, "ARG", 0, 10, "subcommand arguments");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->subcmd;
  args->argtable[3] = args->args;
  args->argtable[4] = args->end;
}


static void cleanup_pkg_argtable(pkg_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void pkg_print_usage(FILE *out) {
  fprintf(out, "Usage: pkg [OPTIONS] COMMAND [ARGS...]\n\n");
  fprintf(out, "Manage jshell packages.\n\n");
  fprintf(out, "Options:\n");
  fprintf(out, "  -h, --help     display this help and exit\n");
  fprintf(out, "  --json         output in JSON format (where applicable)\n\n");
  fprintf(out, "Commands:\n");
  fprintf(out, "  list                      list installed packages\n");
  fprintf(out, "  info NAME                 show information about a package\n");
  fprintf(out, "  search QUERY              search registry for packages\n");
  fprintf(out, "  install <name|tarball>    install package by name or from tarball\n");
  fprintf(out, "  install all               install all packages from registry\n");
  fprintf(out, "  remove NAME               remove an installed package\n");
  fprintf(out, "  build <src> <out.tar.gz>  build a package for distribution\n");
  fprintf(out, "  check-update              check for available updates\n");
  fprintf(out, "  upgrade                   upgrade all packages with updates\n");
  fprintf(out, "  compile [name]            recompile installed package from source\n");
  fprintf(out, "\nEnvironment:\n");
  fprintf(out, "  JSHELL_PKG_REGISTRY       registry URL (default: %s)\n",
          PKG_REGISTRY_DEFAULT_URL);
}


static pkg_subcommand_t parse_subcommand(const char *cmd) {
  if (strcmp(cmd, "list") == 0) return PKG_CMD_LIST;
  if (strcmp(cmd, "info") == 0) return PKG_CMD_INFO;
  if (strcmp(cmd, "search") == 0) return PKG_CMD_SEARCH;
  if (strcmp(cmd, "install") == 0) return PKG_CMD_INSTALL;
  if (strcmp(cmd, "remove") == 0) return PKG_CMD_REMOVE;
  if (strcmp(cmd, "build") == 0) return PKG_CMD_BUILD;
  if (strcmp(cmd, "check-update") == 0) return PKG_CMD_CHECK_UPDATE;
  if (strcmp(cmd, "upgrade") == 0) return PKG_CMD_UPGRADE;
  if (strcmp(cmd, "compile") == 0) return PKG_CMD_COMPILE;
  return PKG_CMD_NONE;
}


static int pkg_list(int json_output) {
  PkgDb *db = pkg_db_load();
  if (db == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to load package database\"}\n");
    } else {
      fprintf(stderr, "pkg list: failed to load package database\n");
    }
    return 1;
  }

  if (json_output) {
    printf("{\"packages\": [");
    for (int i = 0; i < db->count; i++) {
      if (i > 0) printf(", ");
      printf("{\"name\": \"%s\", \"version\": \"%s\"}",
             db->entries[i].name, db->entries[i].version);
    }
    printf("]}\n");
  } else {
    if (db->count == 0) {
      printf("No packages installed.\n");
    } else {
      for (int i = 0; i < db->count; i++) {
        printf("%-20s %s\n", db->entries[i].name, db->entries[i].version);
      }
    }
  }

  pkg_db_free(db);
  return 0;
}


static int pkg_info(const char *name, int json_output) {
  if (name == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"package name required\"}\n");
    } else {
      fprintf(stderr, "pkg info: package name required\n");
    }
    return 1;
  }

  PkgDb *db = pkg_db_load();
  if (db == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to load package database\"}\n");
    } else {
      fprintf(stderr, "pkg info: failed to load package database\n");
    }
    return 1;
  }

  PkgDbEntry *entry = pkg_db_find(db, name);
  if (entry == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", \"message\": \"package not installed\", "
             "\"name\": \"%s\"}\n", name);
    } else {
      fprintf(stderr, "pkg info: package '%s' not installed\n", name);
    }
    pkg_db_free(db);
    return 1;
  }

  char *pkgs_dir = pkg_get_pkgs_dir();
  if (pkgs_dir == NULL) {
    pkg_db_free(db);
    return 1;
  }

  size_t pkg_path_len = strlen(pkgs_dir) + strlen(name) + strlen(entry->version)
                        + 3;
  char *pkg_path = malloc(pkg_path_len);
  if (pkg_path == NULL) {
    free(pkgs_dir);
    pkg_db_free(db);
    return 1;
  }
  snprintf(pkg_path, pkg_path_len, "%s/%s-%s", pkgs_dir, name, entry->version);
  free(pkgs_dir);

  size_t manifest_path_len = strlen(pkg_path) + strlen("/pkg.json") + 1;
  char *manifest_path = malloc(manifest_path_len);
  if (manifest_path == NULL) {
    free(pkg_path);
    pkg_db_free(db);
    return 1;
  }
  snprintf(manifest_path, manifest_path_len, "%s/pkg.json", pkg_path);

  PkgManifest *m = pkg_manifest_load(manifest_path);
  free(manifest_path);

  if (m == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to load package manifest\"}\n");
    } else {
      fprintf(stderr, "pkg info: failed to load package manifest\n");
    }
    free(pkg_path);
    pkg_db_free(db);
    return 1;
  }

  if (json_output) {
    printf("{\"name\": \"%s\", \"version\": \"%s\"",
           m->name, m->version);
    if (m->description) {
      printf(", \"description\": \"%s\"", m->description);
    }
    printf(", \"files\": [");
    for (int i = 0; i < m->files_count; i++) {
      if (i > 0) printf(", ");
      printf("\"%s\"", m->files[i]);
    }
    printf("], \"path\": \"%s\"}\n", pkg_path);
  } else {
    printf("Name:        %s\n", m->name);
    printf("Version:     %s\n", m->version);
    if (m->description) {
      printf("Description: %s\n", m->description);
    }
    printf("Files:       ");
    for (int i = 0; i < m->files_count; i++) {
      if (i > 0) printf(", ");
      printf("%s", m->files[i]);
    }
    printf("\n");
    printf("Location:    %s\n", pkg_path);
  }

  pkg_manifest_free(m);
  free(pkg_path);
  pkg_db_free(db);
  return 0;
}


static int pkg_search(const char *query, int json_output) {
  if (query == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"search term required\"}\n");
    } else {
      fprintf(stderr, "pkg search: search term required\n");
    }
    return 1;
  }

  PkgRegistryList *results = pkg_registry_search(query);
  if (results == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to connect to registry\", "
             "\"registry\": \"%s\"}\n", pkg_registry_get_url());
    } else {
      fprintf(stderr, "pkg search: failed to connect to registry at %s\n",
              pkg_registry_get_url());
    }
    return 1;
  }

  if (json_output) {
    printf("{\"status\": \"ok\", \"query\": \"%s\", \"results\": [", query);
    for (int i = 0; i < results->count; i++) {
      if (i > 0) printf(", ");
      printf("{\"name\": \"%s\"", results->entries[i].name);
      if (results->entries[i].latest_version) {
        printf(", \"version\": \"%s\"", results->entries[i].latest_version);
      }
      if (results->entries[i].description) {
        printf(", \"description\": \"%s\"", results->entries[i].description);
      }
      printf("}");
    }
    printf("]}\n");
  } else {
    if (results->count == 0) {
      printf("No packages found matching '%s'.\n", query);
    } else {
      printf("Found %d package(s) matching '%s':\n\n", results->count, query);
      for (int i = 0; i < results->count; i++) {
        printf("  %-15s", results->entries[i].name);
        if (results->entries[i].latest_version) {
          printf(" %-10s", results->entries[i].latest_version);
        }
        if (results->entries[i].description) {
          printf(" %s", results->entries[i].description);
        }
        printf("\n");
      }
    }
  }

  pkg_registry_list_free(results);
  return 0;
}


// Forward declaration for internal tarball install
static int pkg_install_from_tarball(const char *tarball, int json_output);

// Forward declaration for single package install (non-all)
static int pkg_install_single(const char *arg, int json_output);


static int pkg_install_all(int json_output) {
  if (!json_output) {
    printf("Fetching package list from registry...\n");
  }

  PkgRegistryList *all_packages = pkg_registry_fetch_all();
  if (all_packages == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to connect to registry\", "
             "\"registry\": \"%s\"}\n", pkg_registry_get_url());
    } else {
      fprintf(stderr, "pkg install all: failed to connect to registry at %s\n",
              pkg_registry_get_url());
    }
    return 1;
  }

  if (all_packages->count == 0) {
    if (json_output) {
      printf("{\"status\": \"ok\", \"installed\": [], \"skipped\": [], "
             "\"failed\": [], \"message\": \"no packages available\"}\n");
    } else {
      printf("No packages available in registry.\n");
    }
    pkg_registry_list_free(all_packages);
    return 0;
  }

  PkgDb *db = pkg_db_load();
  if (db == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to load package database\"}\n");
    } else {
      fprintf(stderr, "pkg install all: failed to load package database\n");
    }
    pkg_registry_list_free(all_packages);
    return 1;
  }

  // Track results
  typedef struct {
    char *name;
    char *version;
    int status;  // 0 = installed, 1 = skipped (already installed), -1 = failed
    char *error;
  } install_result_t;

  install_result_t *results = malloc((size_t)all_packages->count
                                     * sizeof(install_result_t));
  if (results == NULL) {
    pkg_db_free(db);
    pkg_registry_list_free(all_packages);
    return 1;
  }

  int installed_count = 0;
  int skipped_count = 0;
  int failed_count = 0;

  if (!json_output) {
    printf("Found %d package(s) in registry.\n\n", all_packages->count);
  }

  for (int i = 0; i < all_packages->count; i++) {
    PkgRegistryEntry *pkg = &all_packages->entries[i];
    results[i].name = strdup(pkg->name);
    results[i].version = pkg->latest_version ? strdup(pkg->latest_version)
                                             : NULL;
    results[i].error = NULL;

    // Check if already installed
    PkgDbEntry *existing = pkg_db_find(db, pkg->name);
    if (existing != NULL) {
      results[i].status = 1;  // skipped
      skipped_count++;
      if (!json_output) {
        printf("  Skipping %s (already installed: %s)\n",
               pkg->name, existing->version);
      }
      continue;
    }

    if (!json_output) {
      printf("  Installing %s", pkg->name);
      if (pkg->latest_version) {
        printf(" %s", pkg->latest_version);
      }
      printf("...\n");
    }

    if (pkg->download_url == NULL) {
      results[i].status = -1;
      results[i].error = strdup("no download URL");
      failed_count++;
      if (!json_output) {
        printf("    FAILED: no download URL\n");
      }
      continue;
    }

    // Download to temp file
    char temp_path[] = "/tmp/pkg-install-all-XXXXXX.tar.gz";
    int fd = mkstemps(temp_path, 7);
    if (fd < 0) {
      results[i].status = -1;
      results[i].error = strdup("temp file creation failed");
      failed_count++;
      if (!json_output) {
        printf("    FAILED: could not create temp file\n");
      }
      continue;
    }
    close(fd);

    if (pkg_registry_download(pkg->download_url, temp_path) != 0) {
      results[i].status = -1;
      results[i].error = strdup("download failed");
      failed_count++;
      remove(temp_path);
      if (!json_output) {
        printf("    FAILED: download error\n");
      }
      continue;
    }

    // Install silently
    FILE *devnull = fopen("/dev/null", "w");
    FILE *orig_stdout = stdout;
    FILE *orig_stderr = stderr;
    if (devnull) {
      stdout = devnull;
      stderr = devnull;
    }

    int install_result = pkg_install_from_tarball(temp_path, 0);

    if (devnull) {
      stdout = orig_stdout;
      stderr = orig_stderr;
      fclose(devnull);
    }

    remove(temp_path);

    if (install_result == 0) {
      results[i].status = 0;
      installed_count++;
      if (!json_output) {
        printf("    OK\n");
      }
      // Reload db to see newly installed package
      pkg_db_free(db);
      db = pkg_db_load();
      if (db == NULL) {
        // Try to continue anyway
        db = pkg_db_load();
      }
    } else {
      results[i].status = -1;
      results[i].error = strdup("installation failed");
      failed_count++;
      if (!json_output) {
        printf("    FAILED: installation error\n");
      }
    }
  }

  pkg_db_free(db);

  // Output results
  if (json_output) {
    printf("{\"status\": \"%s\", \"installed\": [",
           failed_count == 0 ? "ok" : "partial");
    int first = 1;
    for (int i = 0; i < all_packages->count; i++) {
      if (results[i].status != 0) continue;
      if (!first) printf(", ");
      first = 0;
      printf("{\"name\": \"%s\"", results[i].name);
      if (results[i].version) {
        printf(", \"version\": \"%s\"", results[i].version);
      }
      printf("}");
    }
    printf("], \"skipped\": [");
    first = 1;
    for (int i = 0; i < all_packages->count; i++) {
      if (results[i].status != 1) continue;
      if (!first) printf(", ");
      first = 0;
      printf("\"%s\"", results[i].name);
    }
    printf("], \"failed\": [");
    first = 1;
    for (int i = 0; i < all_packages->count; i++) {
      if (results[i].status != -1) continue;
      if (!first) printf(", ");
      first = 0;
      printf("{\"name\": \"%s\", \"error\": \"%s\"}",
             results[i].name, results[i].error ? results[i].error : "unknown");
    }
    printf("]}\n");
  } else {
    printf("\n");
    printf("Installed: %d, Skipped: %d, Failed: %d\n",
           installed_count, skipped_count, failed_count);
  }

  // Cleanup
  for (int i = 0; i < all_packages->count; i++) {
    free(results[i].name);
    free(results[i].version);
    free(results[i].error);
  }
  free(results);
  pkg_registry_list_free(all_packages);

  return failed_count > 0 ? 1 : 0;
}


static int pkg_install(const char *arg, int json_output) {
  // Handle "pkg install all" special case
  if (arg != NULL && strcmp(arg, "all") == 0) {
    return pkg_install_all(json_output);
  }

  return pkg_install_single(arg, json_output);
}


static int pkg_install_single(const char *arg, int json_output) {
  if (arg == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"package name or tarball path required\"}\n");
    } else {
      fprintf(stderr, "pkg install: package name or tarball path required\n");
    }
    return 1;
  }

  struct stat st;
  // Check if arg is a local file
  if (stat(arg, &st) == 0) {
    // It's a local file, install directly
    return pkg_install_from_tarball(arg, json_output);
  }

  // Not a local file - try to fetch from registry
  // Parse package name (could be "echo" or "echo-0.0.1")
  char *pkg_name = strdup(arg);
  if (pkg_name == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", \"message\": \"memory allocation failed\"}\n");
    } else {
      fprintf(stderr, "pkg install: memory allocation failed\n");
    }
    return 1;
  }

  // Check if version is specified (e.g., "echo-0.0.1")
  // Find last dash followed by a digit
  char *version_sep = NULL;
  for (char *p = pkg_name + strlen(pkg_name) - 1; p > pkg_name; p--) {
    if (*p == '-' && p[1] >= '0' && p[1] <= '9') {
      version_sep = p;
      break;
    }
  }

  char *requested_version = NULL;
  if (version_sep != NULL) {
    *version_sep = '\0';
    requested_version = version_sep + 1;
  }

  if (!json_output) {
    if (requested_version) {
      printf("Fetching %s version %s from registry...\n", pkg_name,
             requested_version);
    } else {
      printf("Fetching %s from registry...\n", pkg_name);
    }
  }

  // Query registry for package
  PkgRegistryEntry *entry = pkg_registry_fetch_package(pkg_name);
  if (entry == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"package not found in registry\", "
             "\"name\": \"%s\"}\n", pkg_name);
    } else {
      fprintf(stderr, "pkg install: package '%s' not found in registry\n",
              pkg_name);
    }
    free(pkg_name);
    return 1;
  }

  // Check version if specified
  if (requested_version != NULL &&
      strcmp(requested_version, entry->latest_version) != 0) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"requested version not available\", "
             "\"name\": \"%s\", \"requested\": \"%s\", "
             "\"available\": \"%s\"}\n",
             pkg_name, requested_version, entry->latest_version);
    } else {
      fprintf(stderr, "pkg install: version %s not available for '%s' "
              "(available: %s)\n", requested_version, pkg_name,
              entry->latest_version);
    }
    free(pkg_name);
    pkg_registry_entry_free(entry);
    return 1;
  }

  if (entry->download_url == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"no download URL for package\", "
             "\"name\": \"%s\"}\n", pkg_name);
    } else {
      fprintf(stderr, "pkg install: no download URL for '%s'\n", pkg_name);
    }
    free(pkg_name);
    pkg_registry_entry_free(entry);
    return 1;
  }

  // Download to temp file
  char temp_path[] = "/tmp/pkg-install-XXXXXX.tar.gz";
  int fd = mkstemps(temp_path, 7);
  if (fd < 0) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to create temp file\"}\n");
    } else {
      fprintf(stderr, "pkg install: failed to create temp file\n");
    }
    free(pkg_name);
    pkg_registry_entry_free(entry);
    return 1;
  }
  close(fd);

  if (!json_output) {
    printf("Downloading %s...\n", entry->download_url);
  }

  if (pkg_registry_download(entry->download_url, temp_path) != 0) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"download failed\", "
             "\"url\": \"%s\"}\n", entry->download_url);
    } else {
      fprintf(stderr, "pkg install: download failed from %s\n",
              entry->download_url);
    }
    remove(temp_path);
    free(pkg_name);
    pkg_registry_entry_free(entry);
    return 1;
  }

  free(pkg_name);
  pkg_registry_entry_free(entry);

  // Install from downloaded tarball
  int result = pkg_install_from_tarball(temp_path, json_output);
  remove(temp_path);
  return result;
}


static int pkg_install_from_tarball(const char *tarball, int json_output) {
  if (pkg_ensure_dirs() != 0) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to create package directories\"}\n");
    } else {
      fprintf(stderr, "pkg install: failed to create package directories\n");
    }
    return 1;
  }

  char temp_dir[] = "/tmp/pkg-install-XXXXXX";
  if (mkdtemp(temp_dir) == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to create temp directory\"}\n");
    } else {
      fprintf(stderr, "pkg install: failed to create temp directory\n");
    }
    return 1;
  }

  char *tar_argv[] = {"tar", "-xzf", (char *)tarball, "-C", temp_dir, NULL};
  if (pkg_run_command(tar_argv) != 0) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to extract tarball\"}\n");
    } else {
      fprintf(stderr, "pkg install: failed to extract tarball\n");
    }
    pkg_remove_dir_recursive(temp_dir);
    return 1;
  }

  size_t manifest_path_len = strlen(temp_dir) + strlen("/pkg.json") + 1;
  char *manifest_path = malloc(manifest_path_len);
  if (manifest_path == NULL) {
    pkg_remove_dir_recursive(temp_dir);
    return 1;
  }
  snprintf(manifest_path, manifest_path_len, "%s/pkg.json", temp_dir);

  PkgManifest *m = pkg_manifest_load(manifest_path);
  free(manifest_path);

  if (m == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"pkg.json not found in tarball\"}\n");
    } else {
      fprintf(stderr, "pkg install: pkg.json not found in tarball\n");
    }
    pkg_remove_dir_recursive(temp_dir);
    return 1;
  }

  if (pkg_manifest_validate(m) != 0) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"invalid pkg.json\"}\n");
    } else {
      fprintf(stderr, "pkg install: invalid pkg.json\n");
    }
    pkg_manifest_free(m);
    pkg_remove_dir_recursive(temp_dir);
    return 1;
  }

  PkgDb *db = pkg_db_load();
  if (db == NULL) {
    pkg_manifest_free(m);
    pkg_remove_dir_recursive(temp_dir);
    return 1;
  }

  PkgDbEntry *existing = pkg_db_find(db, m->name);
  if (existing != NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"package already installed\", "
             "\"name\": \"%s\", \"version\": \"%s\"}\n",
             m->name, existing->version);
    } else {
      fprintf(stderr, "pkg install: package '%s' already installed "
              "(version %s)\n", m->name, existing->version);
    }
    pkg_db_free(db);
    pkg_manifest_free(m);
    pkg_remove_dir_recursive(temp_dir);
    return 1;
  }

  char *pkgs_dir = pkg_get_pkgs_dir();
  if (pkgs_dir == NULL) {
    pkg_db_free(db);
    pkg_manifest_free(m);
    pkg_remove_dir_recursive(temp_dir);
    return 1;
  }

  size_t install_path_len = strlen(pkgs_dir) + strlen(m->name)
                            + strlen(m->version) + 3;
  char *install_path = malloc(install_path_len);
  if (install_path == NULL) {
    free(pkgs_dir);
    pkg_db_free(db);
    pkg_manifest_free(m);
    pkg_remove_dir_recursive(temp_dir);
    return 1;
  }
  snprintf(install_path, install_path_len, "%s/%s-%s",
           pkgs_dir, m->name, m->version);
  free(pkgs_dir);

  if (rename(temp_dir, install_path) != 0) {
    char *mv_argv[] = {"mv", temp_dir, install_path, NULL};
    if (pkg_run_command(mv_argv) != 0) {
      if (json_output) {
        printf("{\"status\": \"error\", "
               "\"message\": \"failed to move package to install location\"}\n");
      } else {
        fprintf(stderr, "pkg install: failed to move package\n");
      }
      free(install_path);
      pkg_db_free(db);
      pkg_manifest_free(m);
      pkg_remove_dir_recursive(temp_dir);
      return 1;
    }
  }

  // Compile package if it has a Makefile
  char makefile_path[4096];
  snprintf(makefile_path, sizeof(makefile_path), "%s/Makefile", install_path);
  struct stat makefile_st;
  if (stat(makefile_path, &makefile_st) == 0) {
    if (!json_output) {
      printf("Compiling %s...\n", m->name);
    }
    int compile_result = compile_package_dir(m->name, install_path, 0, 0);
    if (compile_result != 0) {
      if (!json_output) {
        fprintf(stderr, "Warning: compilation failed, using pre-built binary\n");
      }
    }
  }

  char *bin_dir = pkg_get_bin_dir();
  if (bin_dir == NULL) {
    free(install_path);
    pkg_db_free(db);
    pkg_manifest_free(m);
    return 1;
  }

  for (int i = 0; i < m->files_count; i++) {
    char *file_path_copy = strdup(m->files[i]);
    if (file_path_copy == NULL) continue;
    char *file_name = basename(file_path_copy);

    size_t src_len = strlen(install_path) + strlen(m->files[i]) + 2;
    char *src_path = malloc(src_len);
    if (src_path == NULL) {
      free(file_path_copy);
      continue;
    }
    snprintf(src_path, src_len, "%s/%s", install_path, m->files[i]);

    size_t link_len = strlen(bin_dir) + strlen(file_name) + 2;
    char *link_path = malloc(link_len);
    if (link_path == NULL) {
      free(src_path);
      free(file_path_copy);
      continue;
    }
    snprintf(link_path, link_len, "%s/%s", bin_dir, file_name);

    unlink(link_path);
    symlink(src_path, link_path);
    chmod(src_path, 0755);

    free(link_path);
    free(src_path);
    free(file_path_copy);
  }

  free(bin_dir);

  pkg_db_add_full(db, m->name, m->version, m->description,
                  m->files, m->files_count);
  pkg_db_save(db);

  if (json_output) {
    printf("{\"status\": \"ok\", \"name\": \"%s\", \"version\": \"%s\", "
           "\"path\": \"%s\"}\n", m->name, m->version, install_path);
  } else {
    printf("Installed %s version %s\n", m->name, m->version);
  }

  free(install_path);
  pkg_db_free(db);
  pkg_manifest_free(m);
  return 0;
}


static int pkg_remove(const char *name, int json_output) {
  if (name == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"package name required\"}\n");
    } else {
      fprintf(stderr, "pkg remove: package name required\n");
    }
    return 1;
  }

  PkgDb *db = pkg_db_load();
  if (db == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to load package database\"}\n");
    } else {
      fprintf(stderr, "pkg remove: failed to load package database\n");
    }
    return 1;
  }

  PkgDbEntry *entry = pkg_db_find(db, name);
  if (entry == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", \"message\": \"package not installed\", "
             "\"name\": \"%s\"}\n", name);
    } else {
      fprintf(stderr, "pkg remove: package '%s' not installed\n", name);
    }
    pkg_db_free(db);
    return 1;
  }

  char *version = strdup(entry->version);
  if (version == NULL) {
    pkg_db_free(db);
    return 1;
  }

  char *pkgs_dir = pkg_get_pkgs_dir();
  if (pkgs_dir == NULL) {
    free(version);
    pkg_db_free(db);
    return 1;
  }

  size_t pkg_path_len = strlen(pkgs_dir) + strlen(name) + strlen(version) + 3;
  char *pkg_path = malloc(pkg_path_len);
  if (pkg_path == NULL) {
    free(pkgs_dir);
    free(version);
    pkg_db_free(db);
    return 1;
  }
  snprintf(pkg_path, pkg_path_len, "%s/%s-%s", pkgs_dir, name, version);
  free(pkgs_dir);

  size_t manifest_path_len = strlen(pkg_path) + strlen("/pkg.json") + 1;
  char *manifest_path = malloc(manifest_path_len);
  if (manifest_path == NULL) {
    free(pkg_path);
    free(version);
    pkg_db_free(db);
    return 1;
  }
  snprintf(manifest_path, manifest_path_len, "%s/pkg.json", pkg_path);

  PkgManifest *m = pkg_manifest_load(manifest_path);
  free(manifest_path);

  char *bin_dir = pkg_get_bin_dir();
  if (bin_dir != NULL && m != NULL) {
    for (int i = 0; i < m->files_count; i++) {
      char *file_path_copy = strdup(m->files[i]);
      if (file_path_copy == NULL) continue;
      char *file_name = basename(file_path_copy);

      size_t link_len = strlen(bin_dir) + strlen(file_name) + 2;
      char *link_path = malloc(link_len);
      if (link_path != NULL) {
        snprintf(link_path, link_len, "%s/%s", bin_dir, file_name);

        char link_target[4096];
        ssize_t len = readlink(link_path, link_target, sizeof(link_target) - 1);
        if (len > 0) {
          link_target[len] = '\0';
          if (strstr(link_target, pkg_path) != NULL) {
            unlink(link_path);
          }
        }
        free(link_path);
      }
      free(file_path_copy);
    }
    free(bin_dir);
  }

  if (m != NULL) {
    pkg_manifest_free(m);
  }

  if (pkg_remove_dir_recursive(pkg_path) != 0) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to remove package directory\"}\n");
    } else {
      fprintf(stderr, "pkg remove: failed to remove package directory\n");
    }
    free(pkg_path);
    free(version);
    pkg_db_free(db);
    return 1;
  }

  free(pkg_path);

  pkg_db_remove(db, name);
  if (pkg_db_save(db) != 0) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to update package database\"}\n");
    } else {
      fprintf(stderr, "pkg remove: failed to update package database\n");
    }
    free(version);
    pkg_db_free(db);
    return 1;
  }

  if (json_output) {
    printf("{\"status\": \"ok\", \"name\": \"%s\", \"version\": \"%s\"}\n",
           name, version);
  } else {
    printf("Removed %s version %s\n", name, version);
  }

  free(version);
  pkg_db_free(db);
  return 0;
}


static int pkg_build(const char *src_dir, const char *output_tar,
                     int json_output) {
  if (src_dir == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"source directory required\"}\n");
    } else {
      fprintf(stderr, "pkg build: source directory required\n");
      fprintf(stderr, "Usage: pkg build <src-dir> <output.tar.gz>\n");
    }
    return 1;
  }

  if (output_tar == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"output tarball path required\"}\n");
    } else {
      fprintf(stderr, "pkg build: output tarball path required\n");
      fprintf(stderr, "Usage: pkg build <src-dir> <output.tar.gz>\n");
    }
    return 1;
  }

  struct stat st;
  if (stat(src_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"source directory not found\", "
             "\"path\": \"%s\"}\n", src_dir);
    } else {
      fprintf(stderr, "pkg build: source directory not found: %s\n", src_dir);
    }
    return 1;
  }

  size_t manifest_path_len = strlen(src_dir) + strlen("/pkg.json") + 1;
  char *manifest_path = malloc(manifest_path_len);
  if (manifest_path == NULL) {
    return 1;
  }
  snprintf(manifest_path, manifest_path_len, "%s/pkg.json", src_dir);

  PkgManifest *m = pkg_manifest_load(manifest_path);
  free(manifest_path);

  if (m == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"pkg.json not found in source directory\"}\n");
    } else {
      fprintf(stderr, "pkg build: pkg.json not found in %s\n", src_dir);
    }
    return 1;
  }

  if (pkg_manifest_validate(m) != 0) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"invalid pkg.json\"}\n");
    } else {
      fprintf(stderr, "pkg build: invalid pkg.json\n");
    }
    pkg_manifest_free(m);
    return 1;
  }

  for (int i = 0; i < m->files_count; i++) {
    size_t file_path_len = strlen(src_dir) + strlen(m->files[i]) + 2;
    char *file_path = malloc(file_path_len);
    if (file_path == NULL) {
      pkg_manifest_free(m);
      return 1;
    }
    snprintf(file_path, file_path_len, "%s/%s", src_dir, m->files[i]);

    if (stat(file_path, &st) != 0) {
      if (json_output) {
        printf("{\"status\": \"error\", "
               "\"message\": \"file not found\", \"file\": \"%s\"}\n",
               m->files[i]);
      } else {
        fprintf(stderr, "pkg build: file not found: %s\n", m->files[i]);
      }
      free(file_path);
      pkg_manifest_free(m);
      return 1;
    }
    free(file_path);
  }

  char *tar_argv[] = {"tar", "-czf", (char *)output_tar, "-C", (char *)src_dir,
                      ".", NULL};
  if (pkg_run_command(tar_argv) != 0) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to create tarball\"}\n");
    } else {
      fprintf(stderr, "pkg build: failed to create tarball\n");
    }
    pkg_manifest_free(m);
    return 1;
  }

  if (json_output) {
    printf("{\"status\": \"ok\", \"package\": \"%s\", \"version\": \"%s\", "
           "\"output\": \"%s\"}\n", m->name, m->version, output_tar);
  } else {
    printf("Created package: %s\n", output_tar);
  }

  pkg_manifest_free(m);
  return 0;
}


static int pkg_check_update(int json_output) {
  PkgDb *db = pkg_db_load();
  if (db == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to load package database\"}\n");
    } else {
      fprintf(stderr, "pkg check-update: failed to load package database\n");
    }
    return 1;
  }

  if (db->count == 0) {
    if (json_output) {
      printf("{\"status\": \"ok\", \"summary\": {\"up_to_date\": 0, "
             "\"updates_available\": 0, \"errors\": 0}, \"packages\": []}\n");
    } else {
      printf("No packages installed.\n");
    }
    pkg_db_free(db);
    return 0;
  }

  // Track all packages with their status
  typedef struct {
    char *name;
    char *installed;
    char *available;
    int has_update;   // 1 = update available, 0 = up to date, -1 = error
  } pkg_status_t;

  pkg_status_t *packages = malloc((size_t)db->count * sizeof(pkg_status_t));
  if (packages == NULL) {
    pkg_db_free(db);
    return 1;
  }

  int up_to_date = 0;
  int updates_available = 0;
  int errors = 0;

  for (int i = 0; i < db->count; i++) {
    PkgDbEntry *entry = &db->entries[i];
    packages[i].name = strdup(entry->name);
    packages[i].installed = strdup(entry->version);
    packages[i].available = NULL;
    packages[i].has_update = -1;

    PkgRegistryEntry *reg_entry = pkg_registry_fetch_package(entry->name);
    if (reg_entry == NULL) {
      errors++;
      continue;
    }

    packages[i].available = strdup(reg_entry->latest_version);

    int cmp = pkg_version_compare(entry->version, reg_entry->latest_version);
    if (cmp < 0) {
      packages[i].has_update = 1;
      updates_available++;
    } else {
      packages[i].has_update = 0;
      up_to_date++;
    }

    pkg_registry_entry_free(reg_entry);
  }

  if (json_output) {
    printf("{\"status\": \"ok\", \"summary\": {\"up_to_date\": %d, "
           "\"updates_available\": %d, \"errors\": %d}, \"packages\": [",
           up_to_date, updates_available, errors);
    for (int i = 0; i < db->count; i++) {
      if (i > 0) printf(", ");
      printf("{\"name\": \"%s\", \"installed\": \"%s\"",
             packages[i].name, packages[i].installed);
      if (packages[i].available) {
        printf(", \"available\": \"%s\"", packages[i].available);
      }
      const char *status;
      if (packages[i].has_update == 1) {
        status = "update_available";
      } else if (packages[i].has_update == 0) {
        status = "up_to_date";
      } else {
        status = "error";
      }
      printf(", \"status\": \"%s\"}", status);
    }
    printf("]}\n");
  } else {
    // Print status for each package
    printf("Checking %d package(s) for updates...\n\n", db->count);

    for (int i = 0; i < db->count; i++) {
      if (packages[i].has_update == 1) {
        printf("  Update available for %s: %s \u2192 %s\n",
               packages[i].name, packages[i].installed, packages[i].available);
      } else if (packages[i].has_update == 0) {
        printf("  %s is up to date (%s)\n",
               packages[i].name, packages[i].installed);
      } else {
        printf("  %s: could not check (registry error)\n", packages[i].name);
      }
    }

    // Print summary
    printf("\n");
    if (updates_available > 0 && up_to_date > 0) {
      printf("%d package(s) up to date, %d update(s) available\n",
             up_to_date, updates_available);
    } else if (updates_available > 0) {
      printf("%d update(s) available\n", updates_available);
    } else if (up_to_date > 0) {
      printf("All %d package(s) are up to date.\n", up_to_date);
    }

    if (errors > 0) {
      printf("Warning: could not check %d package(s) against registry\n",
             errors);
    }

    if (updates_available > 0) {
      printf("\nRun 'pkg upgrade' to install updates.\n");
    }
  }

  // Cleanup
  for (int i = 0; i < db->count; i++) {
    free(packages[i].name);
    free(packages[i].installed);
    free(packages[i].available);
  }
  free(packages);
  pkg_db_free(db);

  return 0;
}


static int pkg_upgrade(int json_output) {
  if (!json_output) {
    printf("Checking for updates...\n");
  }

  PkgDb *db = pkg_db_load();
  if (db == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to load package database\"}\n");
    } else {
      fprintf(stderr, "pkg upgrade: failed to load package database\n");
    }
    return 1;
  }

  if (db->count == 0) {
    if (json_output) {
      printf("{\"status\": \"ok\", \"upgraded\": [], \"failed\": [], "
             "\"up_to_date\": []}\n");
    } else {
      printf("No packages installed.\n");
    }
    pkg_db_free(db);
    return 0;
  }

  // Collect packages that need updating and up-to-date packages
  typedef struct {
    char *name;
    char *installed;
    char *available;
    char *download_url;
  } upgrade_entry_t;

  upgrade_entry_t *upgrades = NULL;
  int upgrade_count = 0;
  int upgrade_capacity = 0;

  char **up_to_date = NULL;
  int up_to_date_count = 0;
  int up_to_date_capacity = 0;

  for (int i = 0; i < db->count; i++) {
    PkgDbEntry *entry = &db->entries[i];

    PkgRegistryEntry *reg_entry = pkg_registry_fetch_package(entry->name);
    if (reg_entry == NULL) continue;

    int cmp = pkg_version_compare(entry->version, reg_entry->latest_version);
    if (cmp < 0) {
      if (upgrade_count >= upgrade_capacity) {
        upgrade_capacity = upgrade_capacity == 0 ? 8 : upgrade_capacity * 2;
        upgrades = realloc(upgrades,
                           (size_t)upgrade_capacity * sizeof(upgrade_entry_t));
      }
      upgrades[upgrade_count].name = strdup(entry->name);
      upgrades[upgrade_count].installed = strdup(entry->version);
      upgrades[upgrade_count].available = strdup(reg_entry->latest_version);
      upgrades[upgrade_count].download_url = reg_entry->download_url ?
                                             strdup(reg_entry->download_url) :
                                             NULL;
      upgrade_count++;
    } else {
      // Track up-to-date packages
      if (up_to_date_count >= up_to_date_capacity) {
        up_to_date_capacity = up_to_date_capacity == 0 ? 8 :
                              up_to_date_capacity * 2;
        up_to_date = realloc(up_to_date,
                             (size_t)up_to_date_capacity * sizeof(char *));
      }
      up_to_date[up_to_date_count] = strdup(entry->name);
      up_to_date_count++;
    }

    pkg_registry_entry_free(reg_entry);
  }

  pkg_db_free(db);

  if (upgrade_count == 0) {
    if (json_output) {
      printf("{\"status\": \"ok\", \"upgraded\": [], \"failed\": [], "
             "\"up_to_date\": [");
      for (int i = 0; i < up_to_date_count; i++) {
        if (i > 0) printf(", ");
        printf("\"%s\"", up_to_date[i]);
      }
      printf("]}\n");
    } else {
      printf("All packages are up to date.\n");
    }

    for (int i = 0; i < up_to_date_count; i++) free(up_to_date[i]);
    free(up_to_date);
    return 0;
  }

  if (!json_output) {
    printf("Found %d update(s) available.\n\n", upgrade_count);
  }

  // Perform upgrades
  typedef struct {
    char *name;
    char *from;
    char *to;
    int success;
    char *error;
  } upgrade_result_t;

  upgrade_result_t *results = malloc((size_t)upgrade_count
                                     * sizeof(upgrade_result_t));
  int success_count = 0;
  int fail_count = 0;

  for (int i = 0; i < upgrade_count; i++) {
    results[i].name = strdup(upgrades[i].name);
    results[i].from = strdup(upgrades[i].installed);
    results[i].to = strdup(upgrades[i].available);
    results[i].success = 0;
    results[i].error = NULL;

    if (!json_output) {
      printf("Downloading %s %s...\n", upgrades[i].name, upgrades[i].available);
    }

    if (!upgrades[i].download_url) {
      if (!json_output) {
        printf("  FAILED: no download URL\n");
      }
      results[i].error = strdup("no download URL");
      fail_count++;
      continue;
    }

    // Download to temp file
    char temp_path[] = "/tmp/pkg-upgrade-XXXXXX.tar.gz";
    int fd = mkstemps(temp_path, 7);
    if (fd < 0) {
      if (!json_output) {
        printf("  FAILED: could not create temp file\n");
      }
      results[i].error = strdup("temp file creation failed");
      fail_count++;
      continue;
    }
    close(fd);

    if (pkg_registry_download(upgrades[i].download_url, temp_path) != 0) {
      if (!json_output) {
        printf("  FAILED: download error\n");
      }
      results[i].error = strdup("download failed");
      remove(temp_path);
      fail_count++;
      continue;
    }

    if (!json_output) {
      printf("Installing %s %s...\n", upgrades[i].name, upgrades[i].available);
    }

    // Remove old version (silently)
    FILE *devnull = fopen("/dev/null", "w");
    FILE *orig_stdout = stdout;
    FILE *orig_stderr = stderr;
    if (devnull) {
      stdout = devnull;
      stderr = devnull;
    }

    pkg_remove(upgrades[i].name, 0);

    if (devnull) {
      stdout = orig_stdout;
      stderr = orig_stderr;
      fclose(devnull);
    }

    // Install new version (silently)
    devnull = fopen("/dev/null", "w");
    if (devnull) {
      stdout = devnull;
      stderr = devnull;
    }

    int install_result = pkg_install(temp_path, 0);

    if (devnull) {
      stdout = orig_stdout;
      stderr = orig_stderr;
      fclose(devnull);
    }

    remove(temp_path);

    if (install_result == 0) {
      results[i].success = 1;
      success_count++;
      if (!json_output) {
        printf("  Upgraded %s: %s \u2192 %s\n",
               upgrades[i].name, upgrades[i].installed, upgrades[i].available);
      }
    } else {
      results[i].error = strdup("installation failed");
      fail_count++;
      if (!json_output) {
        printf("  FAILED: installation error\n");
      }
    }
  }

  // Output results
  if (json_output) {
    printf("{\"status\": \"%s\", \"upgraded\": [",
           fail_count == 0 ? "ok" : "partial");
    int first = 1;
    for (int i = 0; i < upgrade_count; i++) {
      if (!results[i].success) continue;
      if (!first) printf(", ");
      first = 0;
      printf("{\"name\": \"%s\", \"from\": \"%s\", \"to\": \"%s\"}",
             results[i].name, results[i].from, results[i].to);
    }
    printf("], \"failed\": [");
    first = 1;
    for (int i = 0; i < upgrade_count; i++) {
      if (results[i].success) continue;
      if (!first) printf(", ");
      first = 0;
      printf("{\"name\": \"%s\", \"from\": \"%s\", \"to\": \"%s\", "
             "\"error\": \"%s\"}",
             results[i].name, results[i].from, results[i].to,
             results[i].error ? results[i].error : "unknown");
    }
    printf("], \"up_to_date\": [");
    for (int i = 0; i < up_to_date_count; i++) {
      if (i > 0) printf(", ");
      printf("\"%s\"", up_to_date[i]);
    }
    printf("]}\n");
  } else {
    printf("\n");
    if (success_count > 0) {
      printf("Upgraded %d package(s) successfully.\n", success_count);
    }
    if (fail_count > 0) {
      printf("%d package(s) failed to upgrade.\n", fail_count);
    }
  }

  // Cleanup
  for (int i = 0; i < upgrade_count; i++) {
    free(upgrades[i].name);
    free(upgrades[i].installed);
    free(upgrades[i].available);
    free(upgrades[i].download_url);
    free(results[i].name);
    free(results[i].from);
    free(results[i].to);
    free(results[i].error);
  }
  free(upgrades);
  free(results);
  for (int i = 0; i < up_to_date_count; i++) free(up_to_date[i]);
  free(up_to_date);

  return fail_count > 0 ? 1 : 0;
}


// Helper: compile a package from a directory with Makefile
static int compile_package_dir(const char *name, const char *dir,
                               int json_output, int verbose) {
  struct stat st;
  char makefile_path[4096];
  snprintf(makefile_path, sizeof(makefile_path), "%s/Makefile", dir);

  if (stat(makefile_path, &st) != 0) {
    return -1;  // No Makefile
  }

  if (verbose && !json_output) {
    printf("Compiling %s... ", name);
    fflush(stdout);
  }

  char *make_argv[] = {"make", "-C", (char *)dir, NULL};
  int result = pkg_run_command(make_argv);

  if (verbose && !json_output) {
    printf("%s\n", result == 0 ? "ok" : "FAILED");
  }

  return result;
}


// Helper: get installed package directory path
static char *get_installed_pkg_dir(const char *name) {
  PkgDb *db = pkg_db_load();
  if (db == NULL) return NULL;

  PkgDbEntry *entry = pkg_db_find(db, name);
  if (entry == NULL) {
    pkg_db_free(db);
    return NULL;
  }

  char *pkgs_dir = pkg_get_pkgs_dir();
  if (pkgs_dir == NULL) {
    pkg_db_free(db);
    return NULL;
  }

  size_t path_len = strlen(pkgs_dir) + strlen(name) + strlen(entry->version) + 3;
  char *pkg_dir = malloc(path_len);
  if (pkg_dir != NULL) {
    snprintf(pkg_dir, path_len, "%s/%s-%s", pkgs_dir, name, entry->version);
  }

  free(pkgs_dir);
  pkg_db_free(db);
  return pkg_dir;
}


// Helper: update symlink for compiled binary
static int update_pkg_symlink(const char *name, const char *pkg_dir) {
  char *bin_dir = pkg_get_bin_dir();
  if (bin_dir == NULL) return -1;

  // Look for binary in bin/ subdirectory of package
  char src_path[4096];
  snprintf(src_path, sizeof(src_path), "%s/bin/%s", pkg_dir, name);

  struct stat st;
  if (stat(src_path, &st) != 0) {
    // Try direct binary in package dir
    snprintf(src_path, sizeof(src_path), "%s/%s", pkg_dir, name);
    if (stat(src_path, &st) != 0) {
      free(bin_dir);
      return -1;
    }
  }

  char link_path[4096];
  snprintf(link_path, sizeof(link_path), "%s/%s", bin_dir, name);

  // Remove old symlink and create new one
  unlink(link_path);
  int result = symlink(src_path, link_path);

  free(bin_dir);
  return result;
}


static int pkg_compile(const char *app_name, int json_output) {
  struct stat st;

  // If specific app name given, try installed package first, then src/apps
  if (app_name != NULL) {
    // Try installed package first
    char *pkg_dir = get_installed_pkg_dir(app_name);
    if (pkg_dir != NULL) {
      int result = compile_package_dir(app_name, pkg_dir, json_output, 1);
      if (result >= 0) {
        // Found and attempted compile
        if (result == 0) {
          // Update symlink
          update_pkg_symlink(app_name, pkg_dir);
          if (json_output) {
            printf("{\"status\": \"ok\", \"name\": \"%s\", "
                   "\"source\": \"installed\", \"path\": \"%s\"}\n",
                   app_name, pkg_dir);
          }
        } else {
          if (json_output) {
            printf("{\"status\": \"error\", \"name\": \"%s\", "
                   "\"source\": \"installed\", \"message\": \"compilation failed\"}\n",
                   app_name);
          }
        }
        free(pkg_dir);
        return result;
      }
      free(pkg_dir);
      // No Makefile in installed package, try src/apps
    }

    // Try src/apps directory
    char apps_dir[8192];
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      if (json_output) {
        printf("{\"status\": \"error\", "
               "\"message\": \"failed to get current directory\"}\n");
      } else {
        fprintf(stderr, "pkg compile: failed to get current directory\n");
      }
      return 1;
    }

    snprintf(apps_dir, sizeof(apps_dir), "%s/src/apps/%s", cwd, app_name);
    if (stat(apps_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
      snprintf(apps_dir, sizeof(apps_dir), "../src/apps/%s", app_name);
      if (stat(apps_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        if (json_output) {
          printf("{\"status\": \"error\", \"message\": \"app not found\", "
                 "\"name\": \"%s\"}\n", app_name);
        } else {
          fprintf(stderr, "pkg compile: app '%s' not found\n", app_name);
        }
        return 1;
      }
    }

    int result = compile_package_dir(app_name, apps_dir, json_output, 1);
    if (result < 0) {
      if (json_output) {
        printf("{\"status\": \"error\", "
               "\"message\": \"Makefile not found\", \"app\": \"%s\"}\n",
               app_name);
      } else {
        fprintf(stderr, "pkg compile: Makefile not found for %s\n", app_name);
      }
      return 1;
    }

    if (json_output) {
      if (result == 0) {
        printf("{\"status\": \"ok\", \"name\": \"%s\", "
               "\"source\": \"src/apps\"}\n", app_name);
      } else {
        printf("{\"status\": \"error\", \"name\": \"%s\", "
               "\"source\": \"src/apps\", \"message\": \"compilation failed\"}\n",
               app_name);
      }
    }
    return result;
  }

  // No app name - compile all installed packages that have source,
  // then fall back to src/apps

  int success_count = 0;
  int total_count = 0;
  int skipped_count = 0;

  char **results_names = NULL;
  char **results_sources = NULL;
  int *results_status = NULL;
  int results_count = 0;
  int results_capacity = 0;

  // First, compile installed packages
  PkgDb *db = pkg_db_load();
  if (db != NULL && db->count > 0) {
    if (!json_output) {
      printf("Compiling installed packages...\n");
    }

    for (int i = 0; i < db->count; i++) {
      char *pkg_dir = get_installed_pkg_dir(db->entries[i].name);
      if (pkg_dir == NULL) continue;

      int result = compile_package_dir(db->entries[i].name, pkg_dir,
                                       json_output, 1);
      if (result < 0) {
        // No Makefile, skip
        skipped_count++;
        free(pkg_dir);
        continue;
      }

      total_count++;
      if (result == 0) {
        success_count++;
        update_pkg_symlink(db->entries[i].name, pkg_dir);
      }

      if (json_output) {
        if (results_count >= results_capacity) {
          results_capacity = results_capacity == 0 ? 16 : results_capacity * 2;
          results_names = realloc(results_names,
                                  (size_t)results_capacity * sizeof(char *));
          results_sources = realloc(results_sources,
                                    (size_t)results_capacity * sizeof(char *));
          results_status = realloc(results_status,
                                   (size_t)results_capacity * sizeof(int));
        }
        results_names[results_count] = strdup(db->entries[i].name);
        results_sources[results_count] = strdup("installed");
        results_status[results_count] = result;
        results_count++;
      }

      free(pkg_dir);
    }

    pkg_db_free(db);
  }

  // If no installed packages were compiled, try src/apps
  if (total_count == 0) {
    char apps_dir[8192];
    char cwd[4096];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      snprintf(apps_dir, sizeof(apps_dir), "%s/src/apps", cwd);
      if (stat(apps_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(apps_dir, sizeof(apps_dir), "../src/apps");
      }

      DIR *dir = opendir(apps_dir);
      if (dir != NULL) {
        if (!json_output) {
          printf("Compiling apps from source...\n");
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
          if (entry->d_name[0] == '.') continue;

          char app_dir[8448];
          snprintf(app_dir, sizeof(app_dir), "%s/%s", apps_dir, entry->d_name);

          if (stat(app_dir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

          int result = compile_package_dir(entry->d_name, app_dir,
                                           json_output, 1);
          if (result < 0) continue;  // No Makefile

          total_count++;
          if (result == 0) {
            success_count++;
          }

          if (json_output) {
            if (results_count >= results_capacity) {
              results_capacity = results_capacity == 0 ? 16 :
                                 results_capacity * 2;
              results_names = realloc(results_names,
                                      (size_t)results_capacity * sizeof(char *));
              results_sources = realloc(results_sources,
                                        (size_t)results_capacity * sizeof(char *));
              results_status = realloc(results_status,
                                       (size_t)results_capacity * sizeof(int));
            }
            results_names[results_count] = strdup(entry->d_name);
            results_sources[results_count] = strdup("src/apps");
            results_status[results_count] = result;
            results_count++;
          }
        }
        closedir(dir);
      }
    }
  }

  if (total_count == 0) {
    if (json_output) {
      printf("{\"status\": \"ok\", \"results\": [], \"message\": "
             "\"no packages with source found\"}\n");
    } else {
      printf("No packages with source code found.\n");
    }
    return 0;
  }

  if (json_output) {
    printf("{\"status\": \"%s\", \"results\": [",
           success_count == total_count ? "ok" : "partial");
    for (int i = 0; i < results_count; i++) {
      if (i > 0) printf(", ");
      printf("{\"name\": \"%s\", \"source\": \"%s\", \"status\": \"%s\"}",
             results_names[i], results_sources[i],
             results_status[i] == 0 ? "ok" : "error");
      free(results_names[i]);
      free(results_sources[i]);
    }
    printf("], \"success_count\": %d, \"total_count\": %d",
           success_count, total_count);
    if (skipped_count > 0) {
      printf(", \"skipped_count\": %d", skipped_count);
    }
    printf("}\n");
    free(results_names);
    free(results_sources);
    free(results_status);
  } else {
    printf("\nCompiled %d/%d package(s) successfully.\n",
           success_count, total_count);
    if (skipped_count > 0) {
      printf("Skipped %d package(s) without source code.\n", skipped_count);
    }
  }

  return success_count == total_count ? 0 : 1;
}


static int pkg_run(int argc, char **argv) {
  pkg_args_t args;
  build_pkg_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    pkg_print_usage(stdout);
    cleanup_pkg_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "pkg");
    fprintf(stderr, "Try 'pkg --help' for more information.\n");
    cleanup_pkg_argtable(&args);
    return 1;
  }

  int json_output = args.json->count > 0;
  const char *subcmd_str = args.subcmd->sval[0];
  pkg_subcommand_t subcmd = parse_subcommand(subcmd_str);

  const char *first_arg = NULL;
  const char *second_arg = NULL;
  if (args.args->count > 0) {
    first_arg = args.args->sval[0];
  }
  if (args.args->count > 1) {
    second_arg = args.args->sval[1];
  }

  int result = 0;

  switch (subcmd) {
    case PKG_CMD_LIST:
      result = pkg_list(json_output);
      break;
    case PKG_CMD_INFO:
      result = pkg_info(first_arg, json_output);
      break;
    case PKG_CMD_SEARCH:
      result = pkg_search(first_arg, json_output);
      break;
    case PKG_CMD_INSTALL:
      result = pkg_install(first_arg, json_output);
      break;
    case PKG_CMD_REMOVE:
      result = pkg_remove(first_arg, json_output);
      break;
    case PKG_CMD_BUILD:
      result = pkg_build(first_arg, second_arg, json_output);
      break;
    case PKG_CMD_CHECK_UPDATE:
      result = pkg_check_update(json_output);
      break;
    case PKG_CMD_UPGRADE:
      result = pkg_upgrade(json_output);
      break;
    case PKG_CMD_COMPILE:
      result = pkg_compile(first_arg, json_output);
      break;
    case PKG_CMD_NONE:
      fprintf(stderr, "pkg: unknown command '%s'\n", subcmd_str);
      fprintf(stderr, "Try 'pkg --help' for more information.\n");
      result = 1;
      break;
  }

  cleanup_pkg_argtable(&args);
  return result;
}


const jshell_cmd_spec_t cmd_pkg_spec = {
  .name = "pkg",
  .summary = "manage jshell packages",
  .long_help = "Build, install, list, remove, compile, and upgrade packages "
               "for the jshell.",
  .type = CMD_EXTERNAL,
  .run = pkg_run,
  .print_usage = pkg_print_usage
};


void jshell_register_pkg_command(void) {
  jshell_register_command(&cmd_pkg_spec);
}
