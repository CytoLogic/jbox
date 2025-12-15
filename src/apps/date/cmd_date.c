#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


typedef struct {
  struct arg_lit *help;
  struct arg_end *end;
  void *argtable[2];
} date_args_t;


static void build_date_argtable(date_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->end;
}


static void cleanup_date_argtable(date_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void date_print_usage(FILE *out) {
  date_args_t args;
  build_date_argtable(&args);
  fprintf(out, "Usage: date");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Display the current date and time.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_date_argtable(&args);
}


static int date_run(int argc, char **argv) {
  date_args_t args;
  build_date_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    date_print_usage(stdout);
    cleanup_date_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "date");
    fprintf(stderr, "Try 'date --help' for more information.\n");
    cleanup_date_argtable(&args);
    return 1;
  }

  cleanup_date_argtable(&args);

  time_t now = time(NULL);
  if (now == (time_t)-1) {
    perror("date: cannot get current time");
    return 1;
  }

  struct tm *tm_info = localtime(&now);
  if (tm_info == NULL) {
    perror("date: cannot convert time");
    return 1;
  }

  char buffer[128];
  if (strftime(buffer, sizeof(buffer), "%a %b %e %H:%M:%S %Z %Y", tm_info)
      == 0) {
    fprintf(stderr, "date: cannot format time\n");
    return 1;
  }

  printf("%s\n", buffer);
  return 0;
}


const jshell_cmd_spec_t cmd_date_spec = {
  .name = "date",
  .summary = "display the current date and time",
  .long_help = "Display the current date and time in the default format.",
  .type = CMD_EXTERNAL,
  .run = date_run,
  .print_usage = date_print_usage
};


void jshell_register_date_command(void) {
  jshell_register_command(&cmd_date_spec);
}
