#include <stdio.h>


int jsh_main(void) {
  char line[1024];

  while (true) {
    printf("(jsh)>");
    if (fgets(line, sizeof(line), stdin) == NULL) break;
    printf(line, "\n");
  }

  return 0;
}

