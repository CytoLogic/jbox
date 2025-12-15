#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_str *keys;
  struct arg_end *end;
  void *argtable[4];
} unset_args_t;


static void build_unset_argtable(unset_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->keys = arg_strn(NULL, NULL, "KEY", 1, 100,
                        "environment variable names to unset");
  args->end  = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->keys;
  args->argtable[3] = args->end;
}


static void cleanup_unset_argtable(unset_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void unset_print_usage(FILE *out) {
  unset_args_t args;
  build_unset_argtable(&args);
  fprintf(out, "Usage: unset");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Unset environment variables.\n\n");
  fprintf(out, "Remove the specified environment variables from the shell.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_unset_argtable(&args);
}


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


static int unset_run(int argc, char **argv) {
  unset_args_t args;
  build_unset_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    unset_print_usage(stdout);
    cleanup_unset_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "unset");
    fprintf(stderr, "Try 'unset --help' for more information.\n");
    cleanup_unset_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;
  int result = 0;
  int first = 1;

  if (show_json) {
    printf("[\n");
  }

  for (int i = 0; i < args.keys->count; i++) {
    const char *key = args.keys->sval[i];

    if (unsetenv(key) != 0) {
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
        fprintf(stderr, "unset: cannot unset '%s': %s\n", key, strerror(errno));
      }
      result = 1;
    } else {
      if (show_json) {
        char escaped_key[256];
        escape_json_string(key, escaped_key, sizeof(escaped_key));
        if (!first) printf(",\n");
        first = 0;
        printf("{\"key\": \"%s\", \"status\": \"ok\"}", escaped_key);
      }
    }
  }

  if (show_json) {
    printf("\n]\n");
  }

  cleanup_unset_argtable(&args);
  return result;
}


const jshell_cmd_spec_t cmd_unset_spec = {
  .name = "unset",
  .summary = "unset environment variables",
  .long_help = "Remove the specified environment variables from the shell.",
  .type = CMD_BUILTIN,
  .run = unset_run,
  .print_usage = unset_print_usage
};


void jshell_register_unset_command(void) {
  jshell_register_command(&cmd_unset_spec);
}
