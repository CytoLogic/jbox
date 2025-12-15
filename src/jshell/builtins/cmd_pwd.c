#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_end *end;
  void *argtable[3];
} pwd_args_t;


static void build_pwd_argtable(pwd_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->end  = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->end;
}


static void cleanup_pwd_argtable(pwd_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


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


const jshell_cmd_spec_t cmd_pwd_spec = {
  .name = "pwd",
  .summary = "print working directory",
  .long_help = "Print the full filename of the current working directory.",
  .type = CMD_BUILTIN,
  .run = pwd_run,
  .print_usage = pwd_print_usage
};


void jshell_register_pwd_command(void) {
  jshell_register_command(&cmd_pwd_spec);
}
