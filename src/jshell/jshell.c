#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3.h"

#include "Parser.h"
#include "Absyn.h"
#include "Printer.h"

#include "ast/jshell_ast_interpreter.h"
#include "jshell_job_control.h"
#include "jshell_history.h"
#include "jshell_register_builtins.h"
#include "jshell_register_externals.h"
#include "jshell_pkg_loader.h"
#include "jshell_path.h"
#include "jshell_signals.h"
#include "jshell_env_loader.h"
#include "jshell_ai.h"
#include "utils/jbox_utils.h"
#include "jshell.h"


static int g_last_exit_status = 0;


int jshell_get_last_exit_status(void) {
  return g_last_exit_status;
}


void jshell_set_last_exit_status(int status) {
  g_last_exit_status = status;
}


typedef struct {
  struct arg_lit *help;
  struct arg_str *cmd;
  struct arg_end *end;
  void *argtable[4];
} jshell_args_t;


static void build_jshell_argtable(jshell_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->cmd  = arg_str0("c", NULL, "COMMAND", "execute command and exit");
  args->end  = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->cmd;
  args->argtable[2] = args->end;
  args->argtable[3] = NULL;
}


static void cleanup_jshell_argtable(jshell_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


void jshell_print_usage(FILE *out) {
  jshell_args_t args;
  build_jshell_argtable(&args);
  fprintf(out, "Usage: jshell");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "\njshell - the jbox shell\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  fprintf(out, "\nWhen invoked without -c, runs in interactive mode.\n");
  cleanup_jshell_argtable(&args);
}


int jshell_exec_string(const char *cmd_string) {
  Input parse_tree;

  jshell_init_signals();
  jshell_init_path();
  jshell_load_env_file();
  jshell_ai_init();  /* Initialize AI after env is loaded */
  jshell_init_job_control();
  jshell_register_all_builtin_commands();
  jshell_register_all_external_commands();
  jshell_load_installed_packages();

  g_last_exit_status = 0;

  parse_tree = psInput(cmd_string);

  if (parse_tree == NULL) {
    fprintf(stderr, "jshell: parse error\n");
    return 1;
  }

  DPRINT("%s", showInput(parse_tree));

  interpretInput(parse_tree);

  free_Input(parse_tree);

  return g_last_exit_status;
}


static int jshell_interactive(void) {
  char line[1024];
  char full_line[4096] = "";

  Input parse_tree;

  jshell_init_signals();
  jshell_init_path();
  jshell_load_env_file();
  jshell_ai_init();  /* Initialize AI after env is loaded */
  jshell_init_job_control();
  jshell_history_init();
  jshell_register_all_builtin_commands();
  jshell_register_all_external_commands();
  jshell_load_installed_packages();

  while (true) {
    /* Check for termination signals */
    if (jshell_should_terminate() || jshell_should_hangup()) {
      DPRINT("Received termination signal, exiting");
      break;
    }

    jshell_check_background_jobs();

    /* Clear any pending interrupt before prompting */
    jshell_clear_interrupted();

    printf("(jsh)>");
    fflush(stdout);

    if (fgets(line, sizeof(line), stdin) == NULL) {
      /* Check if fgets was interrupted by SIGINT */
      if (jshell_check_interrupted()) {
        printf("\n");  /* Move to new line after ^C */
        full_line[0] = '\0';  /* Clear any partial input */
        clearerr(stdin);  /* Clear EOF flag set by interrupted read */
        continue;
      }
      /* Actual EOF (Ctrl+D) */
      printf("\n");
      break;
    }

    /* Check if we were interrupted during input */
    if (jshell_check_interrupted()) {
      printf("\n");
      full_line[0] = '\0';
      continue;
    }

    size_t len = strlen(line);

    if (len > 0 && line[len - 1] == '\n') {
      line[--len] = '\0';
    }

    if (len > 0 && line[len - 1] == '\\') {
      line[len - 1] = '\0';
      strcat(full_line, line);
      strcat(full_line, " ");
      continue;
    }

    strcat(full_line, line);

    /* Skip empty lines */
    if (strlen(full_line) == 0) {
      continue;
    }

    jshell_history_add(full_line);

    /* Handle exit command */
    if (strcmp(full_line, "exit") == 0) {
      exit(0);
    }

    parse_tree = psInput(full_line);

    if (parse_tree == NULL) {
      fprintf(stderr, "\033[31mParse Error: Invalid Input!\033[0m\n");
      full_line[0] = '\0';
      continue;
    }

    DPRINT("%s", showInput(parse_tree));

    interpretInput(parse_tree);

    free_Input(parse_tree);

    full_line[0] = '\0';
  }

  return 0;
}


int jshell_main(int argc, char **argv) {
  jshell_args_t args;
  build_jshell_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    jshell_print_usage(stdout);
    cleanup_jshell_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "jshell");
    fprintf(stderr, "Try 'jshell --help' for more information.\n");
    cleanup_jshell_argtable(&args);
    return 1;
  }

  if (args.cmd->count > 0) {
    int status = jshell_exec_string(args.cmd->sval[0]);
    cleanup_jshell_argtable(&args);
    return status;
  }

  cleanup_jshell_argtable(&args);

  return jshell_interactive();
}
