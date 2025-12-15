#include <signal.h>
#include <stdbool.h>
#include <stddef.h>

#include "jbox_signals.h"


/* Global signal flag */
volatile sig_atomic_t jbox_interrupted = 0;


/**
 * Simple SIGINT handler for apps.
 * Just sets the flag - apps should check and handle appropriately.
 */
static void jbox_sigint_handler(int sig) {
  (void)sig;
  jbox_interrupted = 1;
}


void jbox_setup_sigint_handler(void) {
  struct sigaction sa;

  sa.sa_handler = jbox_sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;  /* Don't use SA_RESTART - let syscalls return EINTR */

  sigaction(SIGINT, &sa, NULL);
}


bool jbox_check_interrupted(void) {
  if (jbox_interrupted) {
    jbox_interrupted = 0;
    return true;
  }
  return false;
}


bool jbox_is_interrupted(void) {
  return jbox_interrupted != 0;
}


void jbox_clear_interrupted(void) {
  jbox_interrupted = 0;
}
