/**
 * @file jbox_signals.c
 * @brief Signal handling utilities for jbox applications.
 *
 * Provides a simple SIGINT handler and interrupt checking utilities
 * for graceful handling of Ctrl-C in command-line applications.
 */

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>

#include "jbox_signals.h"


/** Global flag indicating whether SIGINT was received. */
volatile sig_atomic_t jbox_interrupted = 0;


/**
 * @brief SIGINT signal handler.
 *
 * Sets the global interrupted flag when Ctrl-C is pressed.
 * Applications should check this flag periodically.
 *
 * @param sig Signal number (unused).
 */
static void jbox_sigint_handler(int sig) {
  (void)sig;
  jbox_interrupted = 1;
}


/**
 * @brief Installs the SIGINT signal handler.
 *
 * Sets up the signal handler to catch Ctrl-C interrupts.
 * Does not use SA_RESTART, so system calls will return EINTR.
 */
void jbox_setup_sigint_handler(void) {
  struct sigaction sa;

  sa.sa_handler = jbox_sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;  /* Don't use SA_RESTART - let syscalls return EINTR */

  sigaction(SIGINT, &sa, NULL);
}


/**
 * @brief Checks and clears the interrupt flag.
 *
 * Atomically checks if SIGINT was received and clears the flag.
 *
 * @return true if interrupt was pending, false otherwise.
 */
bool jbox_check_interrupted(void) {
  if (jbox_interrupted) {
    jbox_interrupted = 0;
    return true;
  }
  return false;
}


/**
 * @brief Checks the interrupt flag without clearing it.
 *
 * @return true if SIGINT was received, false otherwise.
 */
bool jbox_is_interrupted(void) {
  return jbox_interrupted != 0;
}


/**
 * @brief Clears the interrupt flag.
 *
 * Resets the SIGINT flag to allow detecting future interrupts.
 */
void jbox_clear_interrupted(void) {
  jbox_interrupted = 0;
}
