/**
 * @file cmd_jobs.c
 * @brief Implementation of the jobs builtin command for listing background jobs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "jshell/jshell_job_control.h"


/**
 * Argument table structure for the jobs command
 */
typedef struct {
  struct arg_lit *help;
  struct arg_lit *json;
  struct arg_end *end;
  void *argtable[3];
} jobs_args_t;


/**
 * Builds the argtable3 argument table for the jobs command.
 *
 * @param args Pointer to jobs_args_t structure to populate
 */
static void build_jobs_argtable(jobs_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->json;
  args->argtable[2] = args->end;
}


/**
 * Cleans up the argtable3 argument table for the jobs command.
 *
 * @param args Pointer to jobs_args_t structure to free
 */
static void cleanup_jobs_argtable(jobs_args_t *args) {
  arg_freetable(args->argtable, sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * Prints usage information for the jobs command.
 *
 * @param out Output stream to write to
 */
static void jobs_print_usage(FILE *out) {
  jobs_args_t args;
  build_jobs_argtable(&args);
  fprintf(out, "Usage: jobs");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "List background jobs.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  cleanup_jobs_argtable(&args);
}


/**
 * Escapes special characters in a string for JSON output.
 *
 * @param str Input string to escape
 * @param out Output buffer for escaped string
 * @param out_size Size of output buffer
 */
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


/**
 * Context structure for printing jobs
 */
typedef struct {
  int show_json;
  int first;
} jobs_print_ctx_t;


/**
 * Converts job status enum to string representation.
 *
 * @param status Job status enum value
 * @return String representation of status
 */
static const char* job_status_string(JobStatus status) {
  switch (status) {
    case JOB_RUNNING: return "Running";
    case JOB_STOPPED: return "Stopped";
    case JOB_DONE:    return "Done";
    default:          return "Unknown";
  }
}


/**
 * Prints a single job in text format.
 *
 * @param job Background job to print
 * @param userdata Unused user data pointer
 */
static void print_job_text(const BackgroundJob *job, void *userdata) {
  (void)userdata;
  printf("[%d]  %-23s %s\n",
         job->job_id,
         job_status_string(job->status),
         job->cmd_string);
}


/**
 * Prints a single job in JSON format.
 *
 * @param job Background job to print
 * @param userdata Pointer to jobs_print_ctx_t context
 */
static void print_job_json(const BackgroundJob *job, void *userdata) {
  jobs_print_ctx_t *ctx = (jobs_print_ctx_t *)userdata;

  if (!ctx->first) {
    printf(",\n");
  }
  ctx->first = 0;

  char escaped_cmd[2048];
  escape_json_string(job->cmd_string, escaped_cmd, sizeof(escaped_cmd));

  printf("    {\"id\": %d, \"status\": \"%s\", \"command\": \"%s\", \"pids\": [",
         job->job_id,
         job_status_string(job->status),
         escaped_cmd);

  for (size_t i = 0; i < job->pid_count; i++) {
    if (i > 0) printf(", ");
    printf("%d", job->pids[i]);
  }
  printf("]}");
}


/**
 * Executes the jobs command.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status (0 for success, non-zero for error)
 */
static int jobs_run(int argc, char **argv) {
  jobs_args_t args;
  build_jobs_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    jobs_print_usage(stdout);
    cleanup_jobs_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "jobs");
    fprintf(stderr, "Try 'jobs --help' for more information.\n");
    cleanup_jobs_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;

  if (show_json) {
    jobs_print_ctx_t ctx = { .show_json = 1, .first = 1 };
    printf("{\"jobs\": [\n");
    jshell_for_each_job(print_job_json, &ctx);
    printf("\n  ]\n}\n");
  } else {
    jshell_for_each_job(print_job_text, NULL);
  }

  cleanup_jobs_argtable(&args);
  return 0;
}


/**
 * Command specification for the jobs builtin
 */
const jshell_cmd_spec_t cmd_jobs_spec = {
  .name = "jobs",
  .summary = "list background jobs",
  .long_help = "Display status of jobs in the current shell session.\n"
               "Shows job number, status, and command for each background job.",
  .type = CMD_BUILTIN,
  .run = jobs_run,
  .print_usage = jobs_print_usage
};


/**
 * Registers the jobs command with the shell command registry.
 */
void jshell_register_jobs_command(void) {
  jshell_register_command(&cmd_jobs_spec);
}
