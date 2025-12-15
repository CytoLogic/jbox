#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "jshell/jshell.h"
#include "utils/jbox_utils.h"


int main(int argc, char *argv[]) {
  char *cmd = argv[0];

  char *after_slash = strrchr(cmd, '/');
  if (after_slash != NULL) {
    after_slash++;
    memmove(cmd, after_slash, strlen(after_slash) + 1);
  }

  if (strcmp(cmd, "jshell") == 0) {
    return jshell_main(argc, argv);
  }

  if (strcmp(cmd, "jbox") == 0) {
    if (argc > 1 && strcmp(argv[1], "jshell") == 0) {
      return jshell_main(argc - 1, argv + 1);
    }

    int has_c_flag = 0;
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "-h") == 0
          || strcmp(argv[i], "--help") == 0) {
        has_c_flag = 1;
        break;
      }
    }

    if (!has_c_flag) {
      printf("welcome to jbox!\n");
    }

    return jshell_main(argc, argv);
  }

  fprintf(stderr, "jbox: unknown command: %s\n", cmd);
  return EXIT_FAILURE;
}
