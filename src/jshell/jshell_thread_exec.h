#ifndef JSHELL_THREAD_EXEC_H
#define JSHELL_THREAD_EXEC_H

#include <pthread.h>
#include <stdbool.h>

#include "jshell_cmd_registry.h"


// Thread state for a builtin command execution
typedef struct JShellBuiltinThread {
  pthread_t thread;
  const jshell_cmd_spec_t* spec;
  int argc;
  char** argv;
  int input_fd;
  int output_fd;
  int exit_code;
  bool completed;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} JShellBuiltinThread;

// Spawn a builtin command in a new thread
// Returns a thread handle, or NULL on failure
// The caller is responsible for calling jshell_wait_builtin_thread()
// and jshell_free_builtin_thread() after use
JShellBuiltinThread* jshell_spawn_builtin_thread(
  const jshell_cmd_spec_t* spec,
  int argc,
  char** argv,
  int input_fd,
  int output_fd
);

// Wait for a builtin thread to complete
// Returns the exit code of the builtin command
int jshell_wait_builtin_thread(JShellBuiltinThread* bt);

// Free resources associated with a builtin thread
// Must be called after jshell_wait_builtin_thread()
void jshell_free_builtin_thread(JShellBuiltinThread* bt);

// Check if a builtin command must run in the main thread
// Returns true if the command modifies shell state (cd, export, unset, etc.)
bool jshell_builtin_requires_main_thread(const char* cmd_name);


#endif
