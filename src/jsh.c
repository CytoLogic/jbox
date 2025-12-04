#include <stdio.h>

#include "Parser.h"
#include "Absyn.h"
#include "Printer.h"


int jsh_main(void) {
  char line[1024];
  Input parse_tree;

  while (true) {
    printf("(jsh)>");
    if (fgets(line, sizeof(line), stdin) == NULL) break;

    parse_tree = psInput(line);
    
    printf("%s\n", showInput(parse_tree));

    free_Input(parse_tree);
  }

  return 0;
}

