#include "jshell_ai_context.h"

/* Use C23 #embed to compile file contents into constants */

const char EXEC_INSTRUCTIONS[] = {
  #embed "ai_context/exec_context.txt"
  , '\0'
};

const char CMD_CONTEXT[] = {
  #embed "ai_context/cmd_context.txt"
  , '\0'
};

const char GRAMMAR_CONTEXT[] = {
  #embed "ai_context/grammar_context.txt"
  , '\0'
};
