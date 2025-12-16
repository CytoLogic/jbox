/**
 * @file jshell_ai_context.c
 * @brief Embedded AI context data compiled into the binary
 *
 * Uses C23 #embed directive to compile text files directly into the binary
 * as null-terminated string constants. This eliminates the need to read
 * external files at runtime for AI context.
 *
 * Context files:
 * - exec_context.txt: Instructions for AI command generation
 * - cmd_context.txt: Documentation for all available commands
 * - grammar_context.txt: Shell grammar and syntax specification
 */

#include "jshell_ai_context.h"

/* Use C23 #embed to compile file contents into constants */

/** Execution instructions for AI command generation */
const char EXEC_INSTRUCTIONS[] = {
  #embed "ai_context/exec_context.txt"
  , '\0'
};

/** Command documentation and usage information */
const char CMD_CONTEXT[] = {
  #embed "ai_context/cmd_context.txt"
  , '\0'
};

/** Shell grammar and syntax specification */
const char GRAMMAR_CONTEXT[] = {
  #embed "ai_context/grammar_context.txt"
  , '\0'
};
