#include <stdio.h>

#include "Parser.h"
#include "Absyn.h"
#include "Printer.h"
#include "ast/jshell_ast_interpreter.h"


int jshell_main(void) {
  char line[1024];
  char full_line[4096] = "";

  Input parse_tree;

  while (true) {
    printf("(jsh)>");

    if (fgets(line, sizeof(line), stdin) == NULL) break;

    size_t len = strlen(line); 

    // Remove trailing newline
    if (len > 0 && line[len - 1] == '\n') {
      line[--len] = '\0';
    } 

    // handle backslash continuation
    if (len > 0 && line[len - 1] == '\\') {
      line[len - 1] = '\0'; // Remove the backslash
      strcat(full_line, line); // Append to full_line
      strcat(full_line, " ");  // Add space
      continue;  // Read next line
    }

    strcat(full_line, line); // append final or sole line

    parse_tree = psInput(full_line);
    
    printf("%s\n", showInput(parse_tree));

    interpretInput(parse_tree);

    free_Input(parse_tree);

    full_line[0] = '\0';
  }

  return 0;
}

