#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


typedef struct {
  struct arg_lit *help;
  struct arg_str *command;
  struct arg_end *end;
  void *argtable[3];
} help_args_t;


static void build_help_argtable(help_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->command = arg_str0(NULL, NULL, "COMMAND",
                           "command to get help for");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->command;
  args->argtable[2] = args->end;
}


static void cleanup_help_argtable(help_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void help_print_usage(FILE *out) {
  help_args_t args;
  build_help_argtable(&args);
  fprintf(out, "Usage: help");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Display help for shell commands.\n\n");
  fprintf(out, "Without arguments, lists all available commands.\n");
  fprintf(out, "With a COMMAND argument, shows detailed help for that "
               "command.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_help_argtable(&args);
}


static void print_command_summary(const jshell_cmd_spec_t *spec,
                                  void *userdata) {
  (void)userdata;
  const char *type_str = (spec->type == CMD_BUILTIN) ? "builtin" : "external";
  printf("  %-20s %s (%s)\n", spec->name, spec->summary, type_str);
}


static int help_run(int argc, char **argv) {
  help_args_t args;
  build_help_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    help_print_usage(stdout);
    cleanup_help_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "help");
    fprintf(stderr, "Try 'help --help' for more information.\n");
    cleanup_help_argtable(&args);
    return 1;
  }

  if (args.command->count > 0) {
    const char *cmd_name = args.command->sval[0];
    const jshell_cmd_spec_t *spec = jshell_find_command(cmd_name);

    if (spec == NULL) {
      fprintf(stderr, "help: no help for '%s'\n", cmd_name);
      cleanup_help_argtable(&args);
      return 1;
    }

    if (spec->print_usage != NULL) {
      spec->print_usage(stdout);
    } else {
      printf("%s - %s\n", spec->name, spec->summary);
      if (spec->long_help != NULL) {
        printf("\n%s\n", spec->long_help);
      }
    }
  } else {
    printf("Available commands:\n\n");
    jshell_for_each_command(print_command_summary, NULL);
    printf("\nType 'help COMMAND' for more information on a specific "
           "command.\n");
  }

  cleanup_help_argtable(&args);
  return 0;
}


const jshell_cmd_spec_t cmd_help_spec = {
  .name = "help",
  .summary = "display help for shell commands",
  .long_help = "Display a list of all available commands, or detailed help "
               "for a specific command if provided as an argument.",
  .type = CMD_BUILTIN,
  .run = help_run,
  .print_usage = help_print_usage
};


void jshell_register_help_command(void) {
  jshell_register_command(&cmd_help_spec);
}
