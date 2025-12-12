#include <stdlib.h>
#include <unistd.h>
#include <wordexp.h>
#include "../jbox_debug.h"
#include "jshell_ast_helpers.h"


int jshell_expand_word(char* word, wordexp_t* word_vector_ptr) {
  int flags = WRDE_NOCMD;
  
  if(word_vector_ptr->we_wordc > 0){
    DPRINT("word_vector_ptr->we_wordc = %zu => flags |= WRDE_APPEND", 
           word_vector_ptr->we_wordc);
    flags |= WRDE_APPEND;
  }
  return wordexp(word, word_vector_ptr, flags);
}


void jshell_exec_job(JShellExecJob* job) {
  DPRINT("jshell_exec_job called (stub)");
  
  if (job == NULL || job->jshell_cmd_vector_ptr == NULL) {
    return;
  }
  
  DPRINT("Job type: %d, cmd_count: %zu", 
         job->exec_job_type, 
         job->jshell_cmd_vector_ptr->cmd_count);
}


void jshell_cleanup_cmd_vector(JShellCmdVector* cmd_vector) {
  DPRINT("jshell_cleanup_cmd_vector called");
  
  if (cmd_vector == NULL) {
    return;
  }
  
  if (cmd_vector->jshell_cmd_params_ptr != NULL) {
    for (size_t i = 0; i < cmd_vector->cmd_count; i++) {
      wordfree(&cmd_vector->jshell_cmd_params_ptr[i].word_expansion);
    }
    free(cmd_vector->jshell_cmd_params_ptr);
  }
}


void jshell_cleanup_job(JShellExecJob* job) {
  DPRINT("jshell_cleanup_job called");
  
  if (job == NULL) {
    return;
  }
  
  if (job->jshell_cmd_vector_ptr != NULL) {
    jshell_cleanup_cmd_vector(job->jshell_cmd_vector_ptr);
    free(job->jshell_cmd_vector_ptr);
  }
  
  if (job->input_fd != -1) {
    close(job->input_fd);
  }
  
  if (job->output_fd != -1) {
    close(job->output_fd);
  }
}
