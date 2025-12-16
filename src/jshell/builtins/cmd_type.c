/**
 * @file cmd_type.c
 * @brief Display command type information builtin command implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


/**
 * Arguments structure for the type command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_str *names;
  struct arg_end *end;
  void *argtable[4];
} type_args_t;


/**
 * Builds the argtable3 structure for the type command.
 *
 * @param args Pointer to type_args_t structure to populate
 */
static void build_type_argtable(type_args_t *args) {
  args->help  = arg_lit0("h", "help", "display this help and exit");
  args->json  = arg_lit0(NULL, "json", "output in JSON format");
  args->names = arg_strn(NULL, NULL, "NAME", 1, 100, "command names to look up");
  args->end   = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->names;
  args->argtable[3] = args->end;
}


/**
 * Frees memory allocated for the type argtable.
 *
 * @param args Pointer to type_args_t structure to cleanup
 */
static void cleanup_type_argtable(type_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the type command.
 *
 * @param out Output stream to write usage information to
 */
static void type_print_usage(FILE *out) {
  type_args_t args;
  build_type_argtable(&args);
  fprintf(out, "Usage: type");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Display information about command type.\n\n");
  fprintf(out, "For each NAME, indicate how it would be interpreted if used\n");
  fprintf(out, "as a command name.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_type_argtable(&args);
}


/**
 * Escapes special characters in a string for JSON output.
 *
 * @param str Input string to escape
 * @param out Output buffer for escaped string
 * @param out_size Size of output buffer
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
 * Searches for an executable in the PATH environment variable.
 *
 * @param name Name of the executable to find
 * @return Dynamically allocated path to the executable, or NULL if not found
 */
static char *find_in_path(const char *name) {
  if (strchr(name, '/') != NULL) {
    if (access(name, X_OK) == 0) {
      return strdup(name);
    }
    return NULL;
  }

  char *path_env = getenv("PATH");
  if (path_env == NULL) {
    return NULL;
  }

  char *path_copy = strdup(path_env);
  if (path_copy == NULL) {
    return NULL;
  }

  char *result = NULL;
  char *saveptr = NULL;
  char *dir = strtok_r(path_copy, ":", &saveptr);

  while (dir != NULL) {
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, name);

    if (access(full_path, X_OK) == 0) {
      result = strdup(full_path);
      break;
    }
    dir = strtok_r(NULL, ":", &saveptr);
  }

  free(path_copy);
  return result;
}


/**
 * Executes the type command.
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, 1 on failure
 */
static int type_run(int argc, char **argv) {
  type_args_t args;
  build_type_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    type_print_usage(stdout);
    cleanup_type_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "type");
    fprintf(stderr, "Try 'type --help' for more information.\n");
    cleanup_type_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;
  int result = 0;
  int first = 1;

  if (show_json) {
    printf("[\n");
  }

  for (int i = 0; i < args.names->count; i++) {
    const char *name = args.names->sval[i];
    const jshell_cmd_spec_t *spec = jshell_find_command(name);

    char escaped_name[256];
    if (show_json) {
      escape_json_string(name, escaped_name, sizeof(escaped_name));
      if (!first) printf(",\n");
      first = 0;
    }

    if (spec != NULL) {
      const char *kind = (spec->type == CMD_BUILTIN) ? "builtin" : "external";
      if (show_json) {
        printf("{\"name\": \"%s\", \"kind\": \"%s\"}", escaped_name, kind);
      } else {
        printf("%s is a shell %s\n", name, kind);
      }
    } else {
      char *path = find_in_path(name);
      if (path != NULL) {
        if (show_json) {
          char escaped_path[PATH_MAX * 2];
          escape_json_string(path, escaped_path, sizeof(escaped_path));
          printf("{\"name\": \"%s\", \"kind\": \"external\", \"path\": \"%s\"}",
                 escaped_name, escaped_path);
        } else {
          printf("%s is %s\n", name, path);
        }
        free(path);
      } else {
        if (show_json) {
          printf("{\"name\": \"%s\", \"kind\": \"not found\"}", escaped_name);
        } else {
          fprintf(stderr, "type: %s: not found\n", name);
        }
        result = 1;
      }
    }
  }

  if (show_json) {
    printf("\n]\n");
  }

  cleanup_type_argtable(&args);
  return result;
}


/**
 * Command specification for the type builtin.
 */
const jshell_cmd_spec_t cmd_type_spec = {
  .name = "type",
  .summary = "display information about command type",
  .long_help = "For each NAME, indicate how it would be interpreted if used "
               "as a command name.",
  .type = CMD_BUILTIN,
  .run = type_run,
  .print_usage = type_print_usage
};


/**
 * Registers the type command with the shell command registry.
 */
void jshell_register_type_command(void) {
  jshell_register_command(&cmd_type_spec);
}
