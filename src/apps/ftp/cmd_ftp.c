/**
 * @file cmd_ftp.c
 * @brief FTP client command implementation.
 *
 * Implements the ftp command for connecting to FTP servers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "argtable3.h"
#include "jshell/jshell_cmd_registry.h"
#include "cmd_ftp.h"
#include "ftp_client.h"
#include "ftp_interactive.h"


/** Default FTP server port. */
#define FTP_DEFAULT_PORT 21021


/**
 * @brief Arguments structure for the ftp command.
 */
typedef struct {
  struct arg_lit *help;
  struct arg_str *host;
  struct arg_int *port;
  struct arg_str *user;
  struct arg_lit *json;
  struct arg_end *end;
  void *argtable[6];
} ftp_args_t;


/**
 * @brief Initialize the argtable3 argument definitions.
 *
 * @param args Pointer to arguments structure to initialize.
 */
static void build_ftp_argtable(ftp_args_t *args) {
  args->help = arg_lit0("h", "help", "display this help and exit");
  args->host = arg_str0("H", "host", "<host>",
                        "server hostname (default: localhost)");
  args->port = arg_int0("p", "port", "<port>",
                        "server port (default: 21021)");
  args->user = arg_str0("u", "user", "<user>",
                        "username for login (default: anonymous)");
  args->json = arg_lit0(NULL, "json", "output in JSON format");
  args->end = arg_end(20);

  args->argtable[0] = args->help;
  args->argtable[1] = args->host;
  args->argtable[2] = args->port;
  args->argtable[3] = args->user;
  args->argtable[4] = args->json;
  args->argtable[5] = args->end;
}


/**
 * @brief Free argtable resources.
 *
 * @param args Pointer to arguments structure.
 */
static void cleanup_ftp_argtable(ftp_args_t *args) {
  arg_freetable(args->argtable,
                sizeof(args->argtable) / sizeof(args->argtable[0]));
}


/**
 * @brief Print usage information.
 *
 * @param out Output stream.
 */
static void ftp_print_usage(FILE *out) {
  ftp_args_t args;
  build_ftp_argtable(&args);

  fprintf(out, "Usage: ftp");
  arg_print_syntax(out, args.argtable, "\n");
  fprintf(out, "FTP client for file transfer.\n\n");
  fprintf(out, "Options:\n");
  arg_print_glossary(out, args.argtable, "  %-25s %s\n");
  fprintf(out, "\nInteractive commands:\n");
  fprintf(out, "  ls [path]           List directory contents\n");
  fprintf(out, "  cd <path>           Change directory\n");
  fprintf(out, "  pwd                 Print working directory\n");
  fprintf(out, "  get <remote> [local] Download file\n");
  fprintf(out, "  put <local> [remote] Upload file\n");
  fprintf(out, "  mkdir <dir>         Create directory\n");
  fprintf(out, "  help                Show commands\n");
  fprintf(out, "  quit                Disconnect and exit\n");

  cleanup_ftp_argtable(&args);
}


/**
 * @brief Main entry point for the ftp command.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status.
 */
static int ftp_run(int argc, char **argv) {
  ftp_args_t args;
  build_ftp_argtable(&args);

  int nerrors = arg_parse(argc, argv, args.argtable);

  /* Handle help */
  if (args.help->count > 0) {
    ftp_print_usage(stdout);
    cleanup_ftp_argtable(&args);
    return 0;
  }

  /* Handle parse errors */
  if (nerrors > 0) {
    arg_print_errors(stderr, args.end, "ftp");
    fprintf(stderr, "Try 'ftp --help' for more information.\n");
    cleanup_ftp_argtable(&args);
    return 1;
  }

  /* Get connection parameters */
  const char *host = "localhost";
  if (args.host->count > 0) {
    host = args.host->sval[0];
  }

  int port = FTP_DEFAULT_PORT;
  if (args.port->count > 0) {
    port = args.port->ival[0];
    if (port <= 0 || port > 65535) {
      fprintf(stderr, "ftp: invalid port: %d\n", port);
      cleanup_ftp_argtable(&args);
      return 1;
    }
  }

  const char *user = "anonymous";
  if (args.user->count > 0) {
    user = args.user->sval[0];
  }

  bool json_output = args.json->count > 0;

  cleanup_ftp_argtable(&args);

  /* Initialize session */
  ftp_session_t session;
  ftp_session_init(&session);

  /* Connect to server */
  if (json_output) {
    printf("{\"action\":\"connect\",\"host\":\"%s\",\"port\":%d,", host, port);
  } else {
    printf("Connecting to %s:%d...\n", host, port);
  }

  if (ftp_connect(&session, host, (uint16_t)port) < 0) {
    if (json_output) {
      printf("\"status\":\"error\",\"message\":\"connection failed\"}\n");
    } else {
      fprintf(stderr, "ftp: failed to connect to %s:%d\n", host, port);
    }
    return 1;
  }

  if (json_output) {
    printf("\"status\":\"ok\",\"response\":\"%s\"}\n",
           ftp_last_response(&session));
  } else {
    printf("Connected: %s\n", ftp_last_response(&session));
  }

  /* Login */
  if (json_output) {
    printf("{\"action\":\"login\",\"user\":\"%s\",", user);
  } else {
    printf("Logging in as %s...\n", user);
  }

  if (ftp_login(&session, user) < 0) {
    if (json_output) {
      printf("\"status\":\"error\",\"message\":\"login failed\",");
      printf("\"response\":\"%s\"}\n", ftp_last_response(&session));
    } else {
      fprintf(stderr, "ftp: login failed: %s\n", ftp_last_response(&session));
    }
    ftp_close(&session);
    return 1;
  }

  if (json_output) {
    printf("\"status\":\"ok\",\"response\":\"%s\"}\n",
           ftp_last_response(&session));
  } else {
    printf("Logged in: %s\n", ftp_last_response(&session));
  }

  /* Enter interactive mode */
  int result = ftp_interactive(&session, json_output);

  /* Disconnect */
  ftp_quit(&session);

  return result;
}


/** FTP command specification. */
const jshell_cmd_spec_t cmd_ftp_spec = {
  .name = "ftp",
  .summary = "FTP client for file transfer",
  .long_help = "Connect to an FTP server for file upload and download.",
  .type = CMD_EXTERNAL,
  .run = ftp_run,
  .print_usage = ftp_print_usage,
};


void jshell_register_ftp_command(void) {
  jshell_register_command(&cmd_ftp_spec);
}
