#ifndef JSHELL_AI_CONTEXT_H
#define JSHELL_AI_CONTEXT_H

/**
 * Embedded AI context constants.
 * These are compiled into the binary using C23 #embed directive.
 */

/* Instructions for AI exec queries */
extern const char EXEC_INSTRUCTIONS[];

/* Command help text for all available commands */
extern const char CMD_CONTEXT[];

/* Shell grammar specification */
extern const char GRAMMAR_CONTEXT[];

#endif /* JSHELL_AI_CONTEXT_H */
