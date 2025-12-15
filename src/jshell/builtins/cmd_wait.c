#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "jshell/jshell_job_control.h"


typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_str *job_id;
  struct arg_end *end;
  void *argtable[4];
} wait_args_t;


static void build_wait_argtable(wait_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->job_id = arg_str0(NULL, NULL, "JOB_ID",
                          "job ID to wait for (use %N or just N)");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->job_id;
  args->argtable[3] = args->end;
}


static void cleanup_wait_argtable(wait_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void wait_print_usage(FILE *out) {
  wait_args_t args;
  build_wait_argtable(&args);
  fprintf(out, "Usage: wait");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Wait for a job to finish.\n\n");
  fprintf(out, "If no JOB_ID is specified, waits for all background jobs.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_wait_argtable(&args);
}


typedef struct {
  int show_json;
  int first;
  int total_status;
} wait_all_ctx_t;


static void wait_for_job_callback(const BackgroundJob *job, void *userdata) {
  wait_all_ctx_t *ctx = (wait_all_ctx_t *)userdata;

  int status = jshell_wait_for_job(job->job_id);

  if (ctx->show_json) {
    if (!ctx->first) {
      printf(",\n");
    }
    ctx->first = 0;
    printf("    {\"job\": %d, \"status\": \"exited\", \"code\": %d}",
           job->job_id, status);
  }

  if (status != 0) {
    ctx->total_status = status;
  }
}


static int wait_run(int argc, char **argv) {
  wait_args_t args;
  build_wait_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    wait_print_usage(stdout);
    cleanup_wait_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "wait");
    fprintf(stderr, "Try 'wait --help' for more information.\n");
    cleanup_wait_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;

  if (args.job_id->count == 0) {
    if (jshell_get_job_count() == 0) {
      if (show_json) {
        printf("{\"jobs\": [], \"status\": \"ok\"}\n");
      }
      cleanup_wait_argtable(&args);
      return 0;
    }

    wait_all_ctx_t ctx = {
      .show_json = show_json,
      .first = 1,
      .total_status = 0
    };

    if (show_json) {
      printf("{\"jobs\": [\n");
    }

    jshell_for_each_job(wait_for_job_callback, &ctx);

    if (show_json) {
      printf("\n  ],\n  \"status\": \"ok\"\n}\n");
    }

    cleanup_wait_argtable(&args);
    return ctx.total_status;
  }

  const char *job_str = args.job_id->sval[0];
  const char *num_start = job_str;
  if (job_str[0] == '%') {
    num_start = job_str + 1;
  }

  char *endptr;
  long job_id = strtol(num_start, &endptr, 10);
  if (*endptr != '\0' || job_id <= 0) {
    if (show_json) {
      printf("{\"status\": \"error\", "
             "\"message\": \"invalid job specification: %s\"}\n", job_str);
    } else {
      fprintf(stderr, "wait: invalid job specification: %s\n", job_str);
    }
    cleanup_wait_argtable(&args);
    return 1;
  }

  BackgroundJob *job = jshell_find_job_by_id((int)job_id);
  if (job == NULL) {
    if (show_json) {
      printf("{\"status\": \"error\", \"message\": \"no such job: %ld\"}\n",
             job_id);
    } else {
      fprintf(stderr, "wait: no such job: %ld\n", job_id);
    }
    cleanup_wait_argtable(&args);
    return 127;
  }

  int status = jshell_wait_for_job((int)job_id);

  if (show_json) {
    printf("{\"job\": %ld, \"status\": \"exited\", \"code\": %d}\n",
           job_id, status);
  }

  cleanup_wait_argtable(&args);
  return status;
}


const jshell_cmd_spec_t cmd_wait_spec = {
  .name = "wait",
  .summary = "wait for a job to finish",
  .long_help = "Wait for a background job to finish and return its exit status.\n"
               "If no job ID is specified, waits for all background jobs.",
  .type = CMD_BUILTIN,
  .run = wait_run,
  .print_usage = wait_print_usage
};


void jshell_register_wait_command(void) {
  jshell_register_command(&cmd_wait_spec);
}
