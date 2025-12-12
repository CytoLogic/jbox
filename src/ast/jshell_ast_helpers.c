#include <wordexp.h> 
#include "jbox_debug.h"


int jshell_expand_word(char* word, wordexp_t* word_vector_ptr) {
  // copy global flags to the stack
  int flags = WRDE_NOCMD;
  // if the word_vector_t has more than 0 elements it is ready to append to
  if(word_vector_ptr->we_wordc > 0){
    DPRINT("word_vector_ptr->we_wordc = %zu => flags |= WRDE_APPEND", word_vector_ptr->we_wordc);
    flags |= WRDE_APPEND;
  }
  return wordexp(word, word_vector_ptr, flags);
}


void jshell_exec_cmdline(void) {
  return NULL;
}


void jshell_exec_ai(void) {
  return NULL;
}
