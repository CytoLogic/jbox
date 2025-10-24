#ifndef JBOX_COMMON
#define JBOX_COMMON

// project wide includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>


static inline void jbox_chk_nonzero_ret_err(int r) { if (r != 0) { perror(NULL); exit(EXIT_FAILURE); } } 

static inline void jbox_chk_neg1_ret_err(int r) { if (r == -1) { perror(NULL); exit(EXIT_FAILURE); } } 

static inline void jbox_chk_null_ptr_err(void *ptr) { if (ptr == NULL) { perror(NULL); exit(EXIT_FAILURE); } } 

static inline void jbox_chk_map_fail_err(void *ptr) { if (ptr == MAP_FAILED) { perror(NULL); exit(EXIT_FAILURE); } } 

// utils

int cp_main(int argc, char *argv[]);

int cat_main(int argc, char *argv[]);

int echo_main(int argc, char *argv[]);

int mv_main(int argc, char *argv[]);

int rm_main(int argc, char *argv[]);

int touch_main(int argc, char *argv[]);

int ls_main(int argc, char *argv[]);

int mkdir_main(int argc, char *argv[]);

int pwd_main(int argc, char *argv[]);

int stat_main(int argc, char *argv[]);

#endif
