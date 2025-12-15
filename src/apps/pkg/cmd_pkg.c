#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


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
  fprintf(out, "  list           list installed packages\n");
  fprintf(out, "  info NAME      show information about a package\n");
  fprintf(out, "  search NAME    search for packages in registry\n");
  fprintf(out, "  install NAME   install a package\n");
  fprintf(out, "  remove NAME    remove an installed package\n");
  fprintf(out, "  build          build a package from current directory\n");
  fprintf(out, "  check-update   check for available updates\n");
  fprintf(out, "  upgrade        upgrade all packages to latest versions\n");
  fprintf(out, "  compile        compile package from source\n");
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
  if (json_output) {
    printf("{\"status\": \"not_implemented\", "
           "\"message\": \"pkg list not yet implemented\"}\n");
  } else {
    fprintf(stderr, "pkg list: not yet implemented\n");
  }
  return 1;
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
  if (json_output) {
    printf("{\"status\": \"not_implemented\", "
           "\"message\": \"pkg info not yet implemented\", "
           "\"package\": \"%s\"}\n", name);
  } else {
    fprintf(stderr, "pkg info: not yet implemented\n");
  }
  return 1;
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


static int pkg_install(const char *name, int json_output) {
  if (name == NULL) {
    if (json_output) {
      printf("{\"status\": \"error\", "
             "\"message\": \"package name required\"}\n");
    } else {
      fprintf(stderr, "pkg install: package name required\n");
    }
    return 1;
  }
  if (json_output) {
    printf("{\"status\": \"not_implemented\", "
           "\"message\": \"pkg install not yet implemented\", "
           "\"package\": \"%s\"}\n", name);
  } else {
    fprintf(stderr, "pkg install: not yet implemented\n");
  }
  return 1;
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
  if (json_output) {
    printf("{\"status\": \"not_implemented\", "
           "\"message\": \"pkg remove not yet implemented\", "
           "\"package\": \"%s\"}\n", name);
  } else {
    fprintf(stderr, "pkg remove: not yet implemented\n");
  }
  return 1;
}


static int pkg_build(int json_output) {
  if (json_output) {
    printf("{\"status\": \"not_implemented\", "
           "\"message\": \"pkg build not yet implemented\"}\n");
  } else {
    fprintf(stderr, "pkg build: not yet implemented\n");
  }
  return 1;
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


static int pkg_compile(int json_output) {
  if (json_output) {
    printf("{\"status\": \"not_implemented\", "
           "\"message\": \"pkg compile not yet implemented\"}\n");
  } else {
    fprintf(stderr, "pkg compile: not yet implemented\n");
  }
  return 1;
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
  if (args.args->count > 0) {
    first_arg = args.args->sval[0];
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
      result = pkg_build(json_output);
      break;
    case PKG_CMD_CHECK_UPDATE:
      result = pkg_check_update(json_output);
      break;
    case PKG_CMD_UPGRADE:
      result = pkg_upgrade(json_output);
      break;
    case PKG_CMD_COMPILE:
      result = pkg_compile(json_output);
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
