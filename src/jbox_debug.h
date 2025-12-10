#include <stdio.h>

#ifndef JBOX_DEBUG_H
#define JBOX_DEBUG_H

#ifdef DEBUG
#else
  #define DPRINT(fmt, ...) fprintf(sterr, "DEBUG: " fmt, ##__VAR_ARGS__)
#else
  #define DPRINT(fmt, ...) do {} while (0)
#endif

#endif
