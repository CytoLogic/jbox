/**
 * @file cmd_cd.c
 * @brief Change directory builtin command implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"


/**
 * Arguments structure for the cd command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_file *dir;
  struct arg_end *end;
  void *argtable[3];
} cd_args_t;


/**
 * Builds the argtable3 structure for the cd command.
 *
 * @param args Pointer to cd_args_t structure to populate
 */
static void build_cd_argtable(cd_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->dir  = arg_file0(NULL, NULL, "DIR", "directory to change to");
  args->end  = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->dir;
  args->argtable[2] = args->end;
}


/**
 * Frees memory allocated for the cd argtable.
 *
 * @param args Pointer to cd_args_t structure to cleanup
 */
static void cleanup_cd_argtable(cd_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the cd command.
 *
 * @param out Output stream to write usage information to
 */
static void cd_print_usage(FILE *out) {
  cd_args_t args;
  build_cd_argtable(&args);
  fprintf(out, "Usage: cd");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Change the shell working directory.\n\n");
  fprintf(out, "Change the current directory to DIR. If DIR is not supplied,\n");
  fprintf(out, "the value of the HOME environment variable is used.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_cd_argtable(&args);
}


/**
 * Executes the cd command.
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, 1 on failure
 */
static int cd_run(int argc, char **argv) {
  cd_args_t args;
  build_cd_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    cd_print_usage(stdout);
    cleanup_cd_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "cd");
    fprintf(stderr, "Try 'cd --help' for more information.\n");
    cleanup_cd_argtable(&args);
    return 1;
  }

  const char *target_dir;

  if (args.dir->count > 0) {
    target_dir = args.dir->filename[0];
  } else {
    target_dir = getenv("HOME");
    if (target_dir == NULL) {
      fprintf(stderr, "cd: HOME not set\n");
      cleanup_cd_argtable(&args);
      return 1;
    }
  }

  if (chdir(target_dir) != 0) {
    fprintf(stderr, "cd: %s: %s\n", target_dir, strerror(errno));
    cleanup_cd_argtable(&args);
    return 1;
  }

  cleanup_cd_argtable(&args);
  return 0;
}


/**
 * Command specification for the cd builtin.
 */
const jshell_cmd_spec_t cmd_cd_spec = {
  .name = "cd",
  .summary = "change the shell working directory",
  .long_help = "Change the current directory to DIR. If DIR is not supplied, "
               "the value of the HOME environment variable is used.",
  .type = CMD_BUILTIN,
  .run = cd_run,
  .print_usage = cd_print_usage
};


/**
 * Registers the cd command with the shell command registry.
 */
void jshell_register_cd_command(void) {
  jshell_register_command(&cmd_cd_spec);
}
