#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "jshell/jshell_job_control.h"


typedef struct {
  struct arg_lit *help;
  struct arg_str *signal;
  struct arg_lit *json;
  struct arg_str *pid;
  struct arg_end *end;
  void *argtable[5];
} kill_args_t;


static void build_kill_argtable(kill_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->signal = arg_str0("s", NULL, "SIGNAL",
                          "signal to send (name or number, default: TERM)");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->pid = arg_str1(NULL, NULL, "PID",
                       "process ID or job ID (use %N for job N)");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->signal;
  args->argtable[2] = args->json;
  args->argtable[3] = args->pid;
  args->argtable[4] = args->end;
}


static void cleanup_kill_argtable(kill_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


static void kill_print_usage(FILE *out) {
  kill_args_t args;
  build_kill_argtable(&args);
  fprintf(out, "Usage: kill");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "Send a signal to a process or job.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-20s %s\n");
  fprintf(out, "\nSignals:\n");
  fprintf(out, "  TERM (15)  Terminate (default)\n");
  fprintf(out, "  KILL (9)   Kill (cannot be caught)\n");
  fprintf(out, "  INT (2)    Interrupt\n");
  fprintf(out, "  HUP (1)    Hangup\n");
  fprintf(out, "  STOP (19)  Stop process\n");
  fprintf(out, "  CONT (18)  Continue stopped process\n");
  cleanup_kill_argtable(&args);
}


typedef struct {
  const char *name;
  int signum;
} signal_entry_t;


static const signal_entry_t signal_table[] = {
  {"HUP",  SIGHUP},
  {"INT",  SIGINT},
  {"QUIT", SIGQUIT},
  {"ILL",  SIGILL},
  {"TRAP", SIGTRAP},
  {"ABRT", SIGABRT},
  {"FPE",  SIGFPE},
  {"KILL", SIGKILL},
  {"SEGV", SIGSEGV},
  {"PIPE", SIGPIPE},
  {"ALRM", SIGALRM},
  {"TERM", SIGTERM},
  {"USR1", SIGUSR1},
  {"USR2", SIGUSR2},
  {"CHLD", SIGCHLD},
  {"CONT", SIGCONT},
  {"STOP", SIGSTOP},
  {"TSTP", SIGTSTP},
  {"TTIN", SIGTTIN},
  {"TTOU", SIGTTOU},
  {NULL, 0}
};


static int parse_signal(const char *sig_str) {
  if (sig_str == NULL || sig_str[0] == '\0') {
    return SIGTERM;
  }

  if (isdigit((unsigned char)sig_str[0])) {
    char *endptr;
    long signum = strtol(sig_str, &endptr, 10);
    if (*endptr == '\0' && signum > 0 && signum < 64) {
      return (int)signum;
    }
    return -1;
  }

  const char *name = sig_str;
  if (strncasecmp(name, "SIG", 3) == 0) {
    name += 3;
  }

  for (const signal_entry_t *entry = signal_table; entry->name; entry++) {
    if (strcasecmp(name, entry->name) == 0) {
      return entry->signum;
    }
  }

  return -1;
}


static const char* signal_name(int signum) {
  for (const signal_entry_t *entry = signal_table; entry->name; entry++) {
    if (entry->signum == signum) {
      return entry->name;
    }
  }
  return "UNKNOWN";
}


static void print_json_result(pid_t pid, int signum, const char *status,
                              const char *message) {
  printf("{\"pid\": %d, \"signal\": %d, \"signal_name\": \"%s\", "
         "\"status\": \"%s\"",
         pid, signum, signal_name(signum), status);
  if (message) {
    printf(", \"message\": \"%s\"", message);
  }
  printf("}\n");
}


static int kill_run(int argc, char **argv) {
  kill_args_t args;
  build_kill_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  if (args.help->count > 0) {
    kill_print_usage(stdout);
    cleanup_kill_argtable(&args);
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "kill");
    fprintf(stderr, "Try 'kill --help' for more information.\n");
    cleanup_kill_argtable(&args);
    return 1;
  }

  int show_json = args.json->count > 0;
  const char *pid_str = args.pid->sval[0];
  const char *sig_str = args.signal->count > 0 ? args.signal->sval[0] : NULL;

  int signum = parse_signal(sig_str);
  if (signum < 0) {
    if (show_json) {
      printf("{\"status\": \"error\", \"message\": \"invalid signal: %s\"}\n",
             sig_str);
    } else {
      fprintf(stderr, "kill: invalid signal: %s\n", sig_str);
    }
    cleanup_kill_argtable(&args);
    return 1;
  }

  pid_t target_pid = 0;

  if (pid_str[0] == '%') {
    char *endptr;
    long job_id = strtol(pid_str + 1, &endptr, 10);
    if (*endptr != '\0' || job_id <= 0) {
      if (show_json) {
        printf("{\"status\": \"error\", "
               "\"message\": \"invalid job specification: %s\"}\n", pid_str);
      } else {
        fprintf(stderr, "kill: invalid job specification: %s\n", pid_str);
      }
      cleanup_kill_argtable(&args);
      return 1;
    }

    BackgroundJob *job = jshell_find_job_by_id((int)job_id);
    if (job == NULL) {
      if (show_json) {
        printf("{\"status\": \"error\", \"message\": \"no such job: %ld\"}\n",
               job_id);
      } else {
        fprintf(stderr, "kill: no such job: %ld\n", job_id);
      }
      cleanup_kill_argtable(&args);
      return 1;
    }

    int success_count = 0;
    int error_count = 0;

    if (show_json) {
      printf("{\"results\": [\n");
    }

    for (size_t i = 0; i < job->pid_count; i++) {
      pid_t pid = job->pids[i];
      int result = kill(pid, signum);
      if (result == 0) {
        success_count++;
        if (show_json) {
          if (i > 0) printf(",\n");
          print_json_result(pid, signum, "ok", NULL);
        }
      } else {
        error_count++;
        if (show_json) {
          if (i > 0) printf(",\n");
          print_json_result(pid, signum, "error", strerror(errno));
        } else {
          fprintf(stderr, "kill: (%d) - %s\n", pid, strerror(errno));
        }
      }
    }

    if (show_json) {
      printf("]}\n");
    }

    cleanup_kill_argtable(&args);
    return error_count > 0 ? 1 : 0;
  }

  char *endptr;
  long pid_val = strtol(pid_str, &endptr, 10);
  if (*endptr != '\0') {
    if (show_json) {
      printf("{\"status\": \"error\", "
             "\"message\": \"invalid process ID: %s\"}\n", pid_str);
    } else {
      fprintf(stderr, "kill: invalid process ID: %s\n", pid_str);
    }
    cleanup_kill_argtable(&args);
    return 1;
  }
  target_pid = (pid_t)pid_val;

  int result = kill(target_pid, signum);
  if (result == 0) {
    if (show_json) {
      print_json_result(target_pid, signum, "ok", NULL);
    }
    cleanup_kill_argtable(&args);
    return 0;
  } else {
    if (show_json) {
      print_json_result(target_pid, signum, "error", strerror(errno));
    } else {
      fprintf(stderr, "kill: (%d) - %s\n", target_pid, strerror(errno));
    }
    cleanup_kill_argtable(&args);
    return 1;
  }
}


const jshell_cmd_spec_t cmd_kill_spec = {
  .name = "kill",
  .summary = "send a signal to a process or job",
  .long_help = "Send a signal to a process specified by PID or to all\n"
               "processes in a job specified by %job_id.\n"
               "Default signal is TERM (15).",
  .type = CMD_BUILTIN,
  .run = kill_run,
  .print_usage = kill_print_usage
};


void jshell_register_kill_command(void) {
  jshell_register_command(&cmd_kill_spec);
}
