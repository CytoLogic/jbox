#ifndef JSHELL_SIGNALS_H
#define JSHELL_SIGNALS_H

#include <signal.h>
#include <stdbool.h>

/**
 * Global flags set by signal handlers.
 * These are volatile sig_atomic_t for safe access from signal handlers.
 */
extern volatile sig_atomic_t jshell_interrupted;
extern volatile sig_atomic_t jshell_received_sigterm;
extern volatile sig_atomic_t jshell_received_sighup;

/**
 * Initialize all signal handlers for the shell.
 * Sets up handlers for SIGINT, SIGTERM, SIGHUP, and ignores SIGPIPE.
 * Should be called early in shell initialization.
 */
void jshell_init_signals(void);

/**
 * Reset all signal handlers to their default disposition.
 * Should be called in child processes immediately after fork(),
 * before exec(), to ensure children have standard signal behavior.
 */
void jshell_reset_signals_for_child(void);

/**
 * Block signals that could interfere with critical sections.
 * Blocks SIGINT and SIGCHLD. Saves previous mask in oldmask.
 *
 * @param oldmask Pointer to store the previous signal mask
 */
void jshell_block_signals(sigset_t *oldmask);

/**
 * Restore signal mask after critical section.
 *
 * @param oldmask Pointer to the previous signal mask to restore
 */
void jshell_unblock_signals(const sigset_t *oldmask);

/**
 * Check if shell was interrupted and clear the flag.
 * Returns true if SIGINT was received since last check.
 *
 * @return true if interrupted, false otherwise
 */
bool jshell_check_interrupted(void);

/**
 * Check if shell was interrupted without clearing the flag.
 * Useful for polling during long operations.
 *
 * @return true if interrupted, false otherwise
 */
bool jshell_is_interrupted(void);

/**
 * Clear the interrupted flag without checking it.
 */
void jshell_clear_interrupted(void);

/**
 * Check if shell received SIGTERM.
 *
 * @return true if SIGTERM received, false otherwise
 */
bool jshell_should_terminate(void);

/**
 * Check if shell received SIGHUP.
 *
 * @return true if SIGHUP received, false otherwise
 */
bool jshell_should_hangup(void);

#endif /* JSHELL_SIGNALS_H */
