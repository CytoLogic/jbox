#include <stdio.h>

#ifndef JBOX_DEBUG_H
#define JBOX_DEBUG_H

#ifdef DEBUG
  #define DPRINT(fmt, ...) fprintf(stderr, "[DEBUG]: " fmt "\n", ##__VA_ARGS__)
  #define DPRINT_WORDEXP(we) \
    do { \
      DPRINT("wordexp_t: we_wordc=%zu, we_offs=%zu", (we).we_wordc, (we).we_offs); \
      for (size_t i = 0; i < (we).we_wordc; i++) { \
        DPRINT("  we_wordv[%zu] = \"%s\"", i, (we).we_wordv[i]); \
      } \
    } while (0)
#else
  #define DPRINT(fmt, ...) do {} while (0)
  #define DPRINT_WORDEXP(we) do {} while (0)
#endif

#endif
