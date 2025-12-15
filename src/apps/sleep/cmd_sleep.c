#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


typedef struct {
  struct arg_lit *help;
  struct arg_dbl *seconds;
  struct arg_end *end;
  void *argtable[3];
} sleep_args_t;


static void build_sleep_argtable(sleep_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->seconds = arg_dbl1(NULL, NULL, "SECONDS",
                           "pause for SECONDS (can be fractional)");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->seconds;
  args->argtable[2] = args->end;
}


static void cleanup_sleep_argtable(sleep_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void sleep_print_usage(FILE *out) {
  sleep_args_t args;
  build_sleep_argtable(&args);
  fprintf(out, "Usage: sleep");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Pause for SECONDS.\n\n");
  fprintf(out, "SECONDS may be a floating point number for fractional "
               "seconds.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_sleep_argtable(&args);
}


static int sleep_run(int argc, char **argv) {
  sleep_args_t args;
  build_sleep_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    sleep_print_usage(stdout);
    cleanup_sleep_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "sleep");
    fprintf(stderr, "Try 'sleep --help' for more information.\n");
    cleanup_sleep_argtable(&args);
    return 1;
  }

  double secs = args.seconds->dval[0];
  cleanup_sleep_argtable(&args);

  if (secs < 0) {
    fprintf(stderr, "sleep: invalid time interval '%g'\n", secs);
    return 1;
  }

  struct timespec ts;
  ts.tv_sec = (time_t)secs;
  ts.tv_nsec = (long)((secs - ts.tv_sec) * 1e9);

  while (nanosleep(&ts, &ts) == -1) {
    if (errno != EINTR) {
      perror("sleep");
      return 1;
    }
  }

  return 0;
}


const jshell_cmd_spec_t cmd_sleep_spec = {
  .name = "sleep",
  .summary = "delay for a specified amount of time",
  .long_help = "Pause execution for SECONDS. The argument may be a "
               "floating point number to specify fractional seconds.",
  .type = CMD_EXTERNAL,
  .run = sleep_run,
  .print_usage = sleep_print_usage
};


void jshell_register_sleep_command(void) {
  jshell_register_command(&cmd_sleep_spec);
}
