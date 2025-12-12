#ifndef JSHELL_AST_HELPERS_H
#define JSHELL_AST_HELPERS_H

#include <wordexp.h>
#include "jshell_ast_interpreter.h"


int jshell_expand_word(char* word, wordexp_t* word_vector_ptr);

void jshell_exec_job(JShellExecJob* job);

void jshell_cleanup_job(JShellExecJob* job);

void jshell_cleanup_cmd_vector(JShellCmdVector* cmd_vector);


#endif
