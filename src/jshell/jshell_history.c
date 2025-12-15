#include <stdlib.h>
#include <string.h>

#include "jshell_history.h"


static char *history[JSHELL_HISTORY_MAX];
static size_t history_count = 0;
static size_t history_start = 0;


void jshell_history_init(void) {
  history_count = 0;
  history_start = 0;
  for (size_t i = 0; i < JSHELL_HISTORY_MAX; i++) {
    history[i] = NULL;
  }
}


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


size_t jshell_history_count(void) {
  return history_count;
}


const char *jshell_history_get(size_t index) {
  if (index >= history_count) {
    return NULL;
  }
  size_t actual_idx = (history_start + index) % JSHELL_HISTORY_MAX;
  return history[actual_idx];
}
