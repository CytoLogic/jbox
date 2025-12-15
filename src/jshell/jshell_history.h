#ifndef JSHELL_HISTORY_H
#define JSHELL_HISTORY_H

#include <stddef.h>

#define JSHELL_HISTORY_MAX 1000

void jshell_history_init(void);
void jshell_history_add(const char *line);
size_t jshell_history_count(void);
const char *jshell_history_get(size_t index);

#endif
