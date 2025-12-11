#include <stdio.h>

#ifndef JBOX_DEBUG_H
#define JBOX_DEBUG_H

#ifdef DEBUG
  #define DPRINT(fmt, ...) fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__)
#else
  #define DPRINT(fmt, ...) do {} while (0)
#endif

#endif
