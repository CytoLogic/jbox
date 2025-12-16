/**
 * @file cmd_env.c
 * @brief Print environment variables builtin command implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"

extern char **environ;


/**
 * Arguments structure for the env command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_end *end;
  void *argtable[3];
} env_args_t;


/**
 * Builds the argtable3 structure for the env command.
 *
 * @param args Pointer to env_args_t structure to populate
 */
static void build_env_argtable(env_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->end  = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->end;
}


/**
 * Frees memory allocated for the env argtable.
 *
 * @param args Pointer to env_args_t structure to cleanup
 */
static void cleanup_env_argtable(env_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the env command.
 *
 * @param out Output stream to write usage information to
 */
static void env_print_usage(FILE *out) {
  env_args_t args;
  build_env_argtable(&args);
  fprintf(out, "Usage: env");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Print environment variables.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_env_argtable(&args);
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
 * Executes the env command.
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, 1 on failure
 */
static int env_run(int argc, char **argv) {
  env_args_t args;
  build_env_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    env_print_usage(stdout);
    cleanup_env_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "env");
    fprintf(stderr, "Try 'env --help' for more information.\n");
    cleanup_env_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;

  if (show_json) {
    printf("{\"env\": {\n");
    int first = 1;
    for (char **e = environ; *e != NULL; e++) {
      char *eq = strchr(*e, '=');
      if (eq == NULL) continue;

      size_t key_len = (size_t)(eq - *e);
      char key[key_len + 1];
      memcpy(key, *e, key_len);
      key[key_len] = '\0';

      char *value = eq + 1;

      char escaped_key[1024];
      char escaped_value[4096];
      escape_json_string(key, escaped_key, sizeof(escaped_key));
      escape_json_string(value, escaped_value, sizeof(escaped_value));

      if (!first) {
        printf(",\n");
      }
      first = 0;
      printf("  \"%s\": \"%s\"", escaped_key, escaped_value);
    }
    printf("\n}}\n");
  } else {
    for (char **e = environ; *e != NULL; e++) {
      printf("%s\n", *e);
    }
  }

  cleanup_env_argtable(&args);
  return 0;
}


/**
 * Command specification for the env builtin.
 */
const jshell_cmd_spec_t cmd_env_spec = {
  .name = "env",
  .summary = "print environment variables",
  .long_help = "Print the current environment variables.",
  .type = CMD_BUILTIN,
  .run = env_run,
  .print_usage = env_print_usage
};


/**
 * Registers the env command with the shell command registry.
 */
void jshell_register_env_command(void) {
  jshell_register_command(&cmd_env_spec);
}
