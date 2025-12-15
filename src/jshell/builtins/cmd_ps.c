#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "jshell/jshell_job_control.h"


typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_end *end;
  void *argtable[3];
} ps_args_t;


static void build_ps_argtable(ps_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->end;
}


static void cleanup_ps_argtable(ps_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void ps_print_usage(FILE *out) {
  ps_args_t args;
  build_ps_argtable(&args);
  fprintf(out, "Usage: ps");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "List processes known to the shell.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_ps_argtable(&args);
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


static const char* job_status_string(JobStatus status) {
  switch (status) {
    case JOB_RUNNING: return "Running";
    case JOB_STOPPED: return "Stopped";
    case JOB_DONE:    return "Done";
    default:          return "Unknown";
  }
}


typedef struct {
  int show_json;
  int first_proc;
} ps_print_ctx_t;


static void print_process_text(const BackgroundJob *job, void *userdata) {
  (void)userdata;
  for (size_t i = 0; i < job->pid_count; i++) {
    printf("%6d  [%d]  %-10s  %s\n",
           job->pids[i],
           job->job_id,
           job_status_string(job->status),
           job->cmd_string);
  }
}


static void print_process_json(const BackgroundJob *job, void *userdata) {
  ps_print_ctx_t *ctx = (ps_print_ctx_t *)userdata;

  char escaped_cmd[2048];
  escape_json_string(job->cmd_string, escaped_cmd, sizeof(escaped_cmd));

  for (size_t i = 0; i < job->pid_count; i++) {
    if (!ctx->first_proc) {
      printf(",\n");
    }
    ctx->first_proc = 0;

    printf("    {\"pid\": %d, \"job_id\": %d, \"status\": \"%s\", "
           "\"command\": \"%s\"}",
           job->pids[i],
           job->job_id,
           job_status_string(job->status),
           escaped_cmd);
  }
}


static int ps_run(int argc, char **argv) {
  ps_args_t args;
  build_ps_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    ps_print_usage(stdout);
    cleanup_ps_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "ps");
    fprintf(stderr, "Try 'ps --help' for more information.\n");
    cleanup_ps_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;

  if (show_json) {
    ps_print_ctx_t ctx = { .show_json = 1, .first_proc = 1 };
    printf("{\"processes\": [\n");
    jshell_for_each_job(print_process_json, &ctx);
    printf("\n  ]\n}\n");
  } else {
    printf("   PID  JOB   STATUS      COMMAND\n");
    jshell_for_each_job(print_process_text, NULL);
  }

  cleanup_ps_argtable(&args);
  return 0;
}


const jshell_cmd_spec_t cmd_ps_spec = {
  .name = "ps",
  .summary = "list processes known to the shell",
  .long_help = "Display a list of processes associated with background jobs\n"
               "in the current shell session.",
  .type = CMD_BUILTIN,
  .run = ps_run,
  .print_usage = ps_print_usage
};


void jshell_register_ps_command(void) {
  jshell_register_command(&cmd_ps_spec);
}
