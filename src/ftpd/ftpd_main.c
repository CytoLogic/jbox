/**
 * @file ftpd_main.c
 * @brief FTP server main entry point.
 *
 * This file contains the main() function for the standalone FTP server
 * daemon. It parses command-line arguments and starts the server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "argtable3.h"
#include "ftpd.h"


/** Global server instance for signal handler access. */
static ftpd_server_t g_server;


/**
 * @brief Signal handler for graceful shutdown.
 *
 * @param sig Signal number (unused).
 */
static void handle_signal(int sig) {
  (void)sig;
  printf("\nftpd: shutting down...\n");
  ftpd_stop(&g_server);
}


/**
 * @brief Print usage information.
 *
 * @param progname Program name.
 * @param argtable Argument table.
 */
static void print_usage(const char *progname, void **argtable) {
  printf("Usage: %s", progname);
  arg_print_syntax(stdout, argtable, "\n");
  printf("\njbox FTP server daemon.\n\n");
  printf("Options:\n");
  arg_print_glossary(stdout, argtable, "  %-25s %s\n");
}


/**
 * @brief Main entry point for the FTP server.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status (0 on success).
 */
int main(int argc, char **argv) {
  /* Define command-line arguments */
  struct arg_lit *help = arg_lit0("h", "help", "display this help and exit");
  struct arg_int *port = arg_int0("p", "port", "<port>",
                                  "port to listen on (default: 21021)");
  struct arg_str *root = arg_str0("r", "root", "<dir>",
                                  "root directory (default: srv/ftp)");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {help, port, root, end};

  /* Set defaults */
  port->ival[0] = FTPD_DEFAULT_PORT;

  /* Parse arguments */
  int nerrors = arg_parse(argc, argv, argtable);

  if (help->count > 0) {
    print_usage(argv[0], argtable);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 0;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }

  /* Determine root directory */
  const char *root_dir = "srv/ftp";
  if (root->count > 0) {
    root_dir = root->sval[0];
  }

  /* Get port */
  int listen_port = port->ival[0];
  if (listen_port <= 0 || listen_port > 65535) {
    fprintf(stderr, "ftpd: invalid port number: %d\n", listen_port);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }

  /* Configure server */
  ftpd_config_t config = {
    .port = (uint16_t)listen_port,
    .root_dir = root_dir,
    .max_clients = FTPD_MAX_CLIENTS
  };

  /* Initialize server */
  if (ftpd_init(&g_server, &config) < 0) {
    fprintf(stderr, "ftpd: failed to initialize server\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }

  /* Set up signal handlers */
  struct sigaction sa = {
    .sa_handler = handle_signal,
    .sa_flags = 0
  };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  /* Ignore SIGPIPE to avoid crashes on broken connections */
  signal(SIGPIPE, SIG_IGN);

  /* Start server (blocks until stopped) */
  int result = ftpd_start(&g_server);

  /* Cleanup */
  ftpd_cleanup(&g_server);
  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

  return (result < 0) ? 1 : 0;
}
