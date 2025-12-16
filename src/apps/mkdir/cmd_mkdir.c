/** @file cmd_mkdir.c
 *  @brief Implementation of the mkdir command for creating directories.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


/**
 * @brief Argument structure for mkdir command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *parents;
  struct arg_lit *json;
  struct arg_file *dirs;
  struct arg_end *end;
  void *argtable[5];
} mkdir_args_t;


/**
 * @brief Builds the argtable3 structure for mkdir command arguments.
 * @param args Pointer to mkdir_args_t structure to populate.
 */
static void build_mkdir_argtable(mkdir_args_t *args) {
  args->help    = arg_lit0("h", "help", "display this help and exit");
  args->parents = arg_lit0("p", "parents", "make parent directories as needed");
  args->json    = arg_lit0(NULL, "json", "output in JSON format");
  args->dirs    = arg_filen(NULL, NULL, "DIR", 1, 100, "directories to create");
  args->end     = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->parents;
  args->argtable[2] = args->json;
  args->argtable[3] = args->dirs;
  args->argtable[4] = args->end;
}


/**
 * @brief Frees memory allocated for the mkdir argtable.
 * @param args Pointer to mkdir_args_t structure to clean up.
 */
static void cleanup_mkdir_argtable(mkdir_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * @brief Prints usage information for the mkdir command.
 * @param out File stream to write the usage information to.
 */
static void mkdir_print_usage(FILE *out) {
  mkdir_args_t args;
  build_mkdir_argtable(&args);
  fprintf(out, "Usage: mkdir");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Create the DIRECTORY(ies), if they do not already exist.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_mkdir_argtable(&args);
}


/**
 * @brief Escapes special characters in a string for JSON output.
 * @param str Input string to escape.
 * @param out Output buffer for escaped string.
 * @param out_size Size of the output buffer.
 */
static void escape_json_string(const char *str, char *out, size_t out_size) {
  size_t j = 0;
  for (size_t i = 0; str[i] && j < out_size - 1; i++) {
    char c = str[i];
    if (c == '"' || c == '\\') {
      if (j + 2 >= out_size) break;
      out[j++] = '\\';
      out[j++] = c;
    } else if (c == '\n') {
      if (j + 2 >= out_size) break;
      out[j++] = '\\';
      out[j++] = 'n';
    } else if (c == '\r') {
      if (j + 2 >= out_size) break;
      out[j++] = '\\';
      out[j++] = 'r';
    } else if (c == '\t') {
      if (j + 2 >= out_size) break;
      out[j++] = '\\';
      out[j++] = 't';
    } else {
      out[j++] = c;
    }
  }
  out[j] = '\0';
}


/**
 * @brief Creates a directory path with all parent directories as needed.
 * @param path Directory path to create.
 * @param mode Permissions mode for created directories.
 * @return 0 on success, -1 on failure.
 */
static int mkdir_parents(const char *path, mode_t mode) {
  char *path_copy = strdup(path);
  if (!path_copy) {
    return -1;
  }

  char *p = path_copy;
  if (*p == '/') {
    p++;
  }

  while (*p) {
    while (*p && *p != '/') {
      p++;
    }

    char saved = *p;
    *p = '\0';

    if (mkdir(path_copy, mode) != 0 && errno != EEXIST) {
      free(path_copy);
      return -1;
    }

    *p = saved;
    if (*p) {
      p++;
    }
  }

  free(path_copy);
  return 0;
}


/**
 * @brief Creates a single directory and outputs result.
 * @param path Directory path to create.
 * @param parents If non-zero, create parent directories as needed.
 * @param show_json If non-zero, output in JSON format.
 * @param first_entry Pointer to flag tracking first JSON entry.
 * @return 0 on success, non-zero on failure.
 */
static int make_dir(const char *path, int parents, int show_json,
                    int *first_entry) {
  int result;

  if (parents) {
    result = mkdir_parents(path, 0755);
  } else {
    result = mkdir(path, 0755);
  }

  if (show_json) {
    char escaped_path[512];
    escape_json_string(path, escaped_path, sizeof(escaped_path));

    if (!*first_entry) {
      printf(",\n");
    }
    *first_entry = 0;

    if (result == 0) {
      printf("{\"path\": \"%s\", \"status\": \"ok\"}", escaped_path);
    } else {
      char escaped_error[256];
      escape_json_string(strerror(errno), escaped_error,
                         sizeof(escaped_error));
      printf("{\"path\": \"%s\", \"status\": \"error\", \"message\": \"%s\"}",
             escaped_path, escaped_error);
    }
  } else {
    if (result != 0) {
      fprintf(stderr, "mkdir: cannot create directory '%s': %s\n",
              path, strerror(errno));
    }
  }

  return result;
}


/**
 * @brief Main entry point for the mkdir command.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, 1 on failure.
 */
static int mkdir_run(int argc, char **argv) {
  mkdir_args_t args;
  build_mkdir_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    mkdir_print_usage(stdout);
    cleanup_mkdir_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "mkdir");
    fprintf(stderr, "Try 'mkdir --help' for more information.\n");
    cleanup_mkdir_argtable(&args);
    return 1;
  }

  int parents = args.parents->count > 0;
  int show_json = args.json->count > 0;
  int first_entry = 1;
  int result = 0;

  if (show_json) {
    printf("[\n");
  }

  for (int i = 0; i < args.dirs->count; i++) {
    if (make_dir(args.dirs->filename[i], parents, show_json,
                 &first_entry) != 0) {
      result = 1;
    }
  }

  if (show_json) {
    printf("\n]\n");
  }

  cleanup_mkdir_argtable(&args);
  return result;
}


/**
 * @brief Command specification for mkdir.
 */
const jshell_cmd_spec_t cmd_mkdir_spec = {
  .name = "mkdir",
  .summary = "make directories",
  .long_help = "Create the DIRECTORY(ies), if they do not already exist. "
               "With -p, create parent directories as needed.",
  .type = CMD_EXTERNAL,
  .run = mkdir_run,
  .print_usage = mkdir_print_usage
};


/**
 * @brief Registers the mkdir command with the shell command registry.
 */
void jshell_register_mkdir_command(void) {
  jshell_register_command(&cmd_mkdir_spec);
}
