#ifndef JBOX_SIGNALS_H
#define JBOX_SIGNALS_H

#include <signal.h>
#include <stdbool.h>

/**
 * Global flag set by SIGINT handler.
 * Apps can check this to detect if they should abort.
 */
extern volatile sig_atomic_t jbox_interrupted;

/**
 * Set up a simple SIGINT handler for standalone apps.
 * The handler sets jbox_interrupted = 1 when SIGINT is received.
 * Should be called early in main() for apps that need interruptibility.
 */
void jbox_setup_sigint_handler(void);

/**
 * Check if the app was interrupted and clear the flag.
 * Returns true if SIGINT was received since last check.
 *
 * @return true if interrupted, false otherwise
 */
bool jbox_check_interrupted(void);

/**
 * Check if the app was interrupted without clearing the flag.
 * Useful for checking in loops where you want to preserve the state.
 *
 * @return true if interrupted, false otherwise
 */
bool jbox_is_interrupted(void);

/**
 * Clear the interrupted flag.
 */
void jbox_clear_interrupted(void);

#endif /* JBOX_SIGNALS_H */
