#include <stdio.h>
#include <stdlib.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "jshell/jshell_history.h"


typedef struct {
  struct arg_lit *help;
  struct arg_end *end;
  void *argtable[2];
} history_args_t;


static void build_history_argtable(history_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->end;
}


static void cleanup_history_argtable(history_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void history_print_usage(FILE *out) {
  history_args_t args;
  build_history_argtable(&args);
  fprintf(out, "Usage: history");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Display command history.\n\n");
  fprintf(out, "Shows a numbered list of commands that have been entered\n");
  fprintf(out, "in the current shell session.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_history_argtable(&args);
}


static int history_run(int argc, char **argv) {
  history_args_t args;
  build_history_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    history_print_usage(stdout);
    cleanup_history_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "history");
    fprintf(stderr, "Try 'history --help' for more information.\n");
    cleanup_history_argtable(&args);
    return 1;
  }

  cleanup_history_argtable(&args);

  size_t count = jshell_history_count();
  for (size_t i = 0; i < count; i++) {
    const char *entry = jshell_history_get(i);
    if (entry != NULL) {
      printf("%5zu  %s\n", i + 1, entry);
    }
  }

  return 0;
}


const jshell_cmd_spec_t cmd_history_spec = {
  .name = "history",
  .summary = "display command history",
  .long_help = "Display a numbered list of commands that have been entered "
               "in the current shell session.",
  .type = CMD_BUILTIN,
  .run = history_run,
  .print_usage = history_print_usage
};


void jshell_register_history_command(void) {
  jshell_register_command(&cmd_history_spec);
}
