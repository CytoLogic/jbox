#include "../jbox.h"
#include "../jshell_cmd_registry.h"
#include "../jshell_job_control.h"
#include <stdio.h>
#include <getopt.h>


static void print_usage(FILE* out) {
  fprintf(out, "Usage: jobs\n");
  fprintf(out, "List background jobs\n");
}


static int jobs_run(int argc, char** argv) {
  char opt;
  while ((opt = getopt(argc, argv, "")) != -1) {
    switch (opt) {
      case '?':
        print_usage(stderr);
        return 1;
      default:
        print_usage(stderr);
        return 1;
    }
  }
  
  jshell_print_jobs();
  
  return 0;
}


static const jshell_cmd_spec_t jobs_spec = {
  .name = "jobs",
  .summary = "List background jobs",
  .long_help = "Display status of jobs in the current shell session.\n"
               "Shows job number, status, and command for each background job.",
  .run = jobs_run,
  .print_usage = print_usage
};


__attribute__((constructor))
static void register_jobs_cmd(void) {
  jshell_register_command(&jobs_spec);
}
