/**
 * @file cmd_echo.c
 * @brief Implementation of the echo command for displaying text.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


/**
 * Arguments structure for the echo command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *no_newline;
  struct arg_str *text;
  struct arg_end *end;
  void *argtable[4];
} echo_args_t;


/**
 * Initializes the argtable3 argument definitions for the echo command.
 *
 * @param args Pointer to the echo_args_t structure to initialize.
 */
static void build_echo_argtable(echo_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->no_newline = arg_lit0("n", NULL, "do not output trailing newline");
  args->text = arg_strn(NULL, NULL, "TEXT", 0, 100, "text to print");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->no_newline;
  args->argtable[2] = args->text;
  args->argtable[3] = args->end;
}


/**
 * Frees memory allocated by build_echo_argtable.
 *
 * @param args Pointer to the echo_args_t structure to clean up.
 */
static void cleanup_echo_argtable(echo_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the echo command.
 *
 * @param out File stream to write usage information to.
 */
static void echo_print_usage(FILE *out) {
  echo_args_t args;
  build_echo_argtable(&args);
  fprintf(out, "Usage: echo");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Display a line of text.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_echo_argtable(&args);
}


/**
 * Main entry point for the echo command.
 *
 * Prints arguments separated by spaces, optionally without trailing newline.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, non-zero on error.
 */
static int echo_run(int argc, char **argv) {
  echo_args_t args;
  build_echo_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    echo_print_usage(stdout);
    cleanup_echo_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "echo");
    fprintf(stderr, "Try 'echo --help' for more information.\n");
    cleanup_echo_argtable(&args);
    return 1;
  }

  for (int i = 0; i < args.text->count; i++) {
    if (i > 0) {
      printf(" ");
    }
    printf("%s", args.text->sval[i]);
  }

  if (args.no_newline->count == 0) {
    printf("\n");
  }

  cleanup_echo_argtable(&args);
  return 0;
}


/**
 * Command specification for the echo command.
 */
const jshell_cmd_spec_t cmd_echo_spec = {
  .name = "echo",
  .summary = "display a line of text",
  .long_help = "Display the TEXT arguments separated by spaces, followed by "
               "a newline. Use -n to suppress the trailing newline.",
  .type = CMD_EXTERNAL,
  .run = echo_run,
  .print_usage = echo_print_usage
};


/**
 * Registers the echo command with the shell command registry.
 */
void jshell_register_echo_command(void) {
  jshell_register_command(&cmd_echo_spec);
}
