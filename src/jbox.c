#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "jshell/jshell.h"
#include "utils/jbox_utils.h"


int main(int argc, char *argv[]) {
  char *cmd = argv[0];

  // get addr after slash
  char *after_slash = strrchr(cmd, '/') + 1;

  // move target substring forward
  memmove(cmd, after_slash, strlen(after_slash) + 1);

  if (strcmp(cmd, "jbox")==0) {
    printf("welcome to jbox!\n");
    jshell_main();
    exit(EXIT_SUCCESS);
  }

  exit(EXIT_FAILURE);
}
