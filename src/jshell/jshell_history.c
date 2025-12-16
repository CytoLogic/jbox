/**
 * @file jshell_history.c
 * @brief Command history management for jshell.
 *
 * Implements a circular buffer for storing command history with automatic
 * deduplication of consecutive identical commands.
 */

#include <stdlib.h>
#include <string.h>

#include "jshell_history.h"


/** Circular buffer of command history entries */
static char *history[JSHELL_HISTORY_MAX];

/** Number of history entries currently stored */
static size_t history_count = 0;

/** Index of the oldest history entry in the circular buffer */
static size_t history_start = 0;


/**
 * Initializes the history system.
 *
 * Clears all history entries and resets the circular buffer state.
 * This should be called once during shell initialization.
 */
void jshell_history_init(void) {
  history_count = 0;
  history_start = 0;
  for (size_t i = 0; i < JSHELL_HISTORY_MAX; i++) {
    history[i] = NULL;
  }
}


/**
 * Adds a command to the history.
 *
 * If the line is identical to the most recent history entry, it is not added
 * (consecutive duplicate prevention). When the history is full, the oldest
 * entry is removed to make room for the new one.
 *
 * @param line Command line to add to history (will be duplicated internally).
 */
void jshell_history_add(const char *line) {
  if (line == NULL || line[0] == '\0') {
    return;
  }

  if (history_count > 0) {
    size_t last_idx = (history_start + history_count - 1) % JSHELL_HISTORY_MAX;
    if (history[last_idx] != NULL && strcmp(history[last_idx], line) == 0) {
      return;
    }
  }

  if (history_count < JSHELL_HISTORY_MAX) {
    size_t idx = (history_start + history_count) % JSHELL_HISTORY_MAX;
    history[idx] = strdup(line);
    history_count++;
  } else {
    free(history[history_start]);
    history[history_start] = strdup(line);
    history_start = (history_start + 1) % JSHELL_HISTORY_MAX;
  }
}


/**
 * Gets the number of commands currently in history.
 *
 * @return Number of history entries (0 to JSHELL_HISTORY_MAX).
 */
size_t jshell_history_count(void) {
  return history_count;
}


/**
 * Retrieves a history entry by index.
 *
 * Index 0 is the oldest entry, index (count-1) is the most recent.
 *
 * @param index Index of the history entry to retrieve.
 * @return Pointer to the command string, or NULL if index is out of range.
 *         The returned string is owned by the history system and should
 *         not be modified or freed by the caller.
 */
const char *jshell_history_get(size_t index) {
  if (index >= history_count) {
    return NULL;
  }
  size_t actual_idx = (history_start + index) % JSHELL_HISTORY_MAX;
  return history[actual_idx];
}
