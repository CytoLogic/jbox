#ifndef JBOX_UTILS_H
#define JBOX_UTILS_H

#include <sys/mman.h>
#include <stdlib.h>

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

static inline void jbox_chk_nonzero_ret_err(int r) { if (r != 0) { perror(NULL); exit(EXIT_FAILURE); } } 

static inline void jbox_chk_neg1_ret_err(int r) { if (r == -1) { perror(NULL); exit(EXIT_FAILURE); } } 

static inline void jbox_chk_null_ptr_err(void *ptr) { if (ptr == NULL) { perror(NULL); exit(EXIT_FAILURE); } } 

static inline void jbox_chk_map_fail_err(void *ptr) { if (ptr == MAP_FAILED) { perror(NULL); exit(EXIT_FAILURE); } } 


#endif
