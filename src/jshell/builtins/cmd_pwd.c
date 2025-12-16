/**
 * @file cmd_pwd.c
 * @brief Print working directory builtin command implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


/**
 * Arguments structure for the pwd command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_end *end;
  void *argtable[3];
} pwd_args_t;


/**
 * Builds the argtable3 structure for the pwd command.
 *
 * @param args Pointer to pwd_args_t structure to populate
 */
static void build_pwd_argtable(pwd_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->end  = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->end;
}


/**
 * Frees memory allocated for the pwd argtable.
 *
 * @param args Pointer to pwd_args_t structure to cleanup
 */
static void cleanup_pwd_argtable(pwd_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the pwd command.
 *
 * @param out Output stream to write usage information to
 */
static void pwd_print_usage(FILE *out) {
  pwd_args_t args;
  build_pwd_argtable(&args);
  fprintf(out, "Usage: pwd");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Print the current working directory.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_pwd_argtable(&args);
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
 * Executes the pwd command.
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, 1 on failure
 */
static int pwd_run(int argc, char **argv) {
  pwd_args_t args;
  build_pwd_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    pwd_print_usage(stdout);
    cleanup_pwd_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "pwd");
    fprintf(stderr, "Try 'pwd --help' for more information.\n");
    cleanup_pwd_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;
  char cwd[PATH_MAX];

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    if (show_json) {
      printf("{\"status\": \"error\", \"message\": \"%s\"}\n", strerror(errno));
    } else {
      fprintf(stderr, "pwd: error getting current directory: %s\n",
              strerror(errno));
    }
    cleanup_pwd_argtable(&args);
    return 1;
  }

  if (show_json) {
    char escaped_cwd[PATH_MAX * 2];
    escape_json_string(cwd, escaped_cwd, sizeof(escaped_cwd));
    printf("{\"cwd\": \"%s\"}\n", escaped_cwd);
  } else {
    printf("%s\n", cwd);
  }

  cleanup_pwd_argtable(&args);
  return 0;
}


/**
 * Command specification for the pwd builtin.
 */
const jshell_cmd_spec_t cmd_pwd_spec = {
  .name = "pwd",
  .summary = "print working directory",
  .long_help = "Print the full filename of the current working directory.",
  .type = CMD_BUILTIN,
  .run = pwd_run,
  .print_usage = pwd_print_usage
};


/**
 * Registers the pwd command with the shell command registry.
 */
void jshell_register_pwd_command(void) {
  jshell_register_command(&cmd_pwd_spec);
}
