/**
 * @file cmd_export.c
 * @brief Set environment variables builtin command implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


/**
 * Arguments structure for the export command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_str *vars;
  struct arg_end *end;
  void *argtable[4];
} export_args_t;


/**
 * Builds the argtable3 structure for the export command.
 *
 * @param args Pointer to export_args_t structure to populate
 */
static void build_export_argtable(export_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->vars = arg_strn(NULL, NULL, "KEY=VALUE", 0, 100,
                        "environment variables to set");
  args->end  = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->vars;
  args->argtable[3] = args->end;
}


/**
 * Frees memory allocated for the export argtable.
 *
 * @param args Pointer to export_args_t structure to cleanup
 */
static void cleanup_export_argtable(export_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the export command.
 *
 * @param out Output stream to write usage information to
 */
static void export_print_usage(FILE *out) {
  export_args_t args;
  build_export_argtable(&args);
  fprintf(out, "Usage: export");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Set environment variables.\n\n");
  fprintf(out, "Each argument should be in the form KEY=VALUE.\n");
  fprintf(out, "The variable is set in the current shell environment.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_export_argtable(&args);
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
 * Executes the export command.
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, 1 on failure
 */
static int export_run(int argc, char **argv) {
  export_args_t args;
  build_export_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    export_print_usage(stdout);
    cleanup_export_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "export");
    fprintf(stderr, "Try 'export --help' for more information.\n");
    cleanup_export_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;
  int result = 0;
  int first = 1;

  if (show_json) {
    printf("[\n");
  }

  for (int i = 0; i < args.vars->count; i++) {
    const char *var = args.vars->sval[i];
    char *eq = strchr(var, '=');

    if (eq == NULL) {
      if (show_json) {
        char escaped_var[512];
        escape_json_string(var, escaped_var, sizeof(escaped_var));
        if (!first) printf(",\n");
        first = 0;
        printf("{\"key\": \"%s\", \"status\": \"error\", \"message\": "
               "\"missing '=' in variable assignment\"}", escaped_var);
      } else {
        fprintf(stderr, "export: '%s': not a valid identifier\n", var);
      }
      result = 1;
      continue;
    }

    size_t key_len = (size_t)(eq - var);
    char key[key_len + 1];
    memcpy(key, var, key_len);
    key[key_len] = '\0';

    char *value = eq + 1;

    if (setenv(key, value, 1) != 0) {
      if (show_json) {
        char escaped_key[256];
        char escaped_msg[256];
        escape_json_string(key, escaped_key, sizeof(escaped_key));
        escape_json_string(strerror(errno), escaped_msg, sizeof(escaped_msg));
        if (!first) printf(",\n");
        first = 0;
        printf("{\"key\": \"%s\", \"status\": \"error\", \"message\": \"%s\"}",
               escaped_key, escaped_msg);
      } else {
        fprintf(stderr, "export: cannot set '%s': %s\n", key, strerror(errno));
      }
      result = 1;
    } else {
      if (show_json) {
        char escaped_key[256];
        char escaped_value[4096];
        escape_json_string(key, escaped_key, sizeof(escaped_key));
        escape_json_string(value, escaped_value, sizeof(escaped_value));
        if (!first) printf(",\n");
        first = 0;
        printf("{\"key\": \"%s\", \"value\": \"%s\", \"status\": \"ok\"}",
               escaped_key, escaped_value);
      }
    }
  }

  if (show_json) {
    printf("\n]\n");
  }

  cleanup_export_argtable(&args);
  return result;
}


/**
 * Command specification for the export builtin.
 */
const jshell_cmd_spec_t cmd_export_spec = {
  .name = "export",
  .summary = "set environment variables",
  .long_help = "Set environment variables in the current shell. "
               "Each argument should be in the form KEY=VALUE.",
  .type = CMD_BUILTIN,
  .run = export_run,
  .print_usage = export_print_usage
};


/**
 * Registers the export command with the shell command registry.
 */
void jshell_register_export_command(void) {
  jshell_register_command(&cmd_export_spec);
}
