#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "jshell_signals.h"
#include "utils/jbox_utils.h"


/* Global signal flags - volatile for safe access from signal handlers */
volatile sig_atomic_t jshell_interrupted = 0;
volatile sig_atomic_t jshell_received_sigterm = 0;
volatile sig_atomic_t jshell_received_sighup = 0;


/**
 * SIGINT handler - sets interrupted flag.
 * In interactive mode, this allows the shell to cancel current input
 * and return to the prompt without exiting.
 */
static void sigint_handler(int sig) {
  (void)sig;
  jshell_interrupted = 1;
}


/**
 * SIGTERM handler - sets termination flag.
 * Allows graceful shutdown with cleanup.
 */
static void sigterm_handler(int sig) {
  (void)sig;
  jshell_received_sigterm = 1;
}


/**
 * SIGHUP handler - sets hangup flag.
 * Allows cleanup (save history, etc.) before exit.
 */
static void sighup_handler(int sig) {
  (void)sig;
  jshell_received_sighup = 1;
}


void jshell_init_signals(void) {
  struct sigaction sa;

  DPRINT("Initializing shell signal handlers");

  /* Clear flags */
  jshell_interrupted = 0;
  jshell_received_sigterm = 0;
  jshell_received_sighup = 0;

  /* Set up SIGINT handler */
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;  /* Restart interrupted syscalls */

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("sigaction SIGINT");
  }

  /* Set up SIGTERM handler */
  sa.sa_handler = sigterm_handler;
  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("sigaction SIGTERM");
  }

  /* Set up SIGHUP handler */
  sa.sa_handler = sighup_handler;
  if (sigaction(SIGHUP, &sa, NULL) == -1) {
    perror("sigaction SIGHUP");
  }

  /* Ignore SIGPIPE - prevents crash when writing to broken pipe */
  sa.sa_handler = SIG_IGN;
  if (sigaction(SIGPIPE, &sa, NULL) == -1) {
    perror("sigaction SIGPIPE");
  }

  DPRINT("Signal handlers initialized: SIGINT, SIGTERM, SIGHUP, SIGPIPE(ignored)");
}


void jshell_reset_signals_for_child(void) {
  struct sigaction sa;

  DPRINT("Resetting signals to default for child process");

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  /* Reset all signals we've modified to default */
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGPIPE, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);
  sigaction(SIGTSTP, &sa, NULL);
  sigaction(SIGTTIN, &sa, NULL);
  sigaction(SIGTTOU, &sa, NULL);

  /* Clear any pending signal masks */
  sigset_t empty_mask;
  sigemptyset(&empty_mask);
  sigprocmask(SIG_SETMASK, &empty_mask, NULL);
}


void jshell_block_signals(sigset_t *oldmask) {
  sigset_t block_mask;

  sigemptyset(&block_mask);
  sigaddset(&block_mask, SIGINT);
  sigaddset(&block_mask, SIGCHLD);

  if (sigprocmask(SIG_BLOCK, &block_mask, oldmask) == -1) {
    perror("sigprocmask block");
  }
}


void jshell_unblock_signals(const sigset_t *oldmask) {
  if (sigprocmask(SIG_SETMASK, oldmask, NULL) == -1) {
    perror("sigprocmask unblock");
  }
}


bool jshell_check_interrupted(void) {
  if (jshell_interrupted) {
    jshell_interrupted = 0;
    return true;
  }
  return false;
}


void jshell_clear_interrupted(void) {
  jshell_interrupted = 0;
}


bool jshell_should_terminate(void) {
  return jshell_received_sigterm != 0;
}


bool jshell_should_hangup(void) {
  return jshell_received_sighup != 0;
}
