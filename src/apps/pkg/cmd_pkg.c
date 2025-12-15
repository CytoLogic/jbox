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
  fprintf(out, "  search NAME               search for packages (future)\n");
  fprintf(out, "  install <tarball>         install package from tarball\n");
  fprintf(out, "  remove NAME               remove an installed package\n");
  fprintf(out, "  build <src> <out.tar.gz>  build a package for distribution\n");
  fprintf(out, "  check-update              check for updates (future)\n");
  fprintf(out, "  upgrade                   upgrade packages (future)\n");
  fprintf(out, "  compile [name]            compile apps (all if name omitted)\n");
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


static int pkg_search(const char *name, int json_output) {
  if (name == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"search term required\"}\n");
    } else {
      fprintf(stderr, "pkg search: search term required\n");
    }
    return 1;
  }
  if (json_output) {
    printf("{\"status\": \"not_implemented\", "
           "\"message\": \"pkg search not yet implemented\", "
           "\"query\": \"%s\"}\n", name);
  } else {
    fprintf(stderr, "pkg search: not yet implemented\n");
  }
  return 1;
}


static int pkg_install(const char *tarball, int json_output) {
  if (tarball == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"tarball path required\"}\n");
    } else {
      fprintf(stderr, "pkg install: tarball path required\n");
    }
    return 1;
  }

  struct stat st;
  if (stat(tarball, &st) != 0) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"tarball not found\", \"path\": \"%s\"}\n", tarball);
    } else {
      fprintf(stderr, "pkg install: tarball not found: %s\n", tarball);
    }
    return 1;
  }

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

  pkg_db_add(db, m->name, m->version);
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
  if (json_output) {
    printf("{\"status\": \"not_implemented\", "
           "\"message\": \"pkg check-update not yet implemented\"}\n");
  } else {
    fprintf(stderr, "pkg check-update: not yet implemented\n");
  }
  return 1;
}


static int pkg_upgrade(int json_output) {
  if (json_output) {
    printf("{\"status\": \"not_implemented\", "
           "\"message\": \"pkg upgrade not yet implemented\"}\n");
  } else {
    fprintf(stderr, "pkg upgrade: not yet implemented\n");
  }
  return 1;
}


static int pkg_compile(const char *app_name, int json_output) {
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

  snprintf(apps_dir, sizeof(apps_dir), "%s/src/apps", cwd);

  struct stat st;
  if (stat(apps_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
    snprintf(apps_dir, sizeof(apps_dir), "../src/apps");
    if (stat(apps_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
      if (json_output) {
        printf("{\"status\": \"error\", "
               "\"message\": \"src/apps directory not found\"}\n");
      } else {
        fprintf(stderr, "pkg compile: src/apps directory not found\n");
      }
      return 1;
    }
  }

  if (app_name != NULL) {
    char app_dir[8448];
    snprintf(app_dir, sizeof(app_dir), "%s/%s", apps_dir, app_name);

    if (stat(app_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
      if (json_output) {
        printf("{\"status\": \"error\", \"message\": \"app not found\", "
               "\"name\": \"%s\"}\n", app_name);
      } else {
        fprintf(stderr, "pkg compile: app '%s' not found\n", app_name);
      }
      return 1;
    }

    char makefile_path[8512];
    snprintf(makefile_path, sizeof(makefile_path), "%s/Makefile", app_dir);

    if (stat(makefile_path, &st) != 0) {
      if (json_output) {
        printf("{\"status\": \"error\", "
               "\"message\": \"Makefile not found\", \"app\": \"%s\"}\n",
               app_name);
      } else {
        fprintf(stderr, "pkg compile: Makefile not found for %s\n", app_name);
      }
      return 1;
    }

    if (!json_output) {
      printf("Compiling %s... ", app_name);
      fflush(stdout);
    }

    char *make_argv[] = {"make", "-C", app_dir, NULL};
    int result = pkg_run_command(make_argv);

    if (json_output) {
      if (result == 0) {
        printf("{\"status\": \"ok\", \"name\": \"%s\"}\n", app_name);
      } else {
        printf("{\"status\": \"error\", \"name\": \"%s\", "
               "\"message\": \"compilation failed\"}\n", app_name);
      }
    } else {
      printf("%s\n", result == 0 ? "ok" : "FAILED");
    }

    return result;
  }

  DIR *dir = opendir(apps_dir);
  if (dir == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"failed to open apps directory\"}\n");
    } else {
      fprintf(stderr, "pkg compile: failed to open apps directory\n");
    }
    return 1;
  }

  int success_count = 0;
  int total_count = 0;

  char **results_names = NULL;
  int *results_status = NULL;
  int results_count = 0;
  int results_capacity = 0;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.') continue;

    char app_dir[8448];
    snprintf(app_dir, sizeof(app_dir), "%s/%s", apps_dir, entry->d_name);

    if (stat(app_dir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

    char makefile_path[8512];
    snprintf(makefile_path, sizeof(makefile_path), "%s/Makefile", app_dir);

    if (stat(makefile_path, &st) != 0) continue;

    total_count++;

    if (!json_output) {
      printf("Compiling %s... ", entry->d_name);
      fflush(stdout);
    }

    char *make_argv[] = {"make", "-C", app_dir, NULL};
    int result = pkg_run_command(make_argv);

    if (result == 0) {
      success_count++;
    }

    if (!json_output) {
      printf("%s\n", result == 0 ? "ok" : "FAILED");
    }

    if (json_output) {
      if (results_count >= results_capacity) {
        results_capacity = results_capacity == 0 ? 16 : results_capacity * 2;
        results_names = realloc(results_names,
                                (size_t)results_capacity * sizeof(char *));
        results_status = realloc(results_status,
                                 (size_t)results_capacity * sizeof(int));
      }
      results_names[results_count] = strdup(entry->d_name);
      results_status[results_count] = result;
      results_count++;
    }
  }

  closedir(dir);

  if (json_output) {
    printf("{\"status\": \"%s\", \"results\": [",
           success_count == total_count ? "ok" : "partial");
    for (int i = 0; i < results_count; i++) {
      if (i > 0) printf(", ");
      printf("{\"name\": \"%s\", \"status\": \"%s\"}",
             results_names[i], results_status[i] == 0 ? "ok" : "error");
      free(results_names[i]);
    }
    printf("], \"success_count\": %d, \"total_count\": %d}\n",
           success_count, total_count);
    free(results_names);
    free(results_status);
  } else {
    printf("\nCompiled %d/%d apps successfully.\n", success_count, total_count);
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
