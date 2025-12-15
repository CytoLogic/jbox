#ifndef JSHELL_SOCKETPAIR_H
#define JSHELL_SOCKETPAIR_H

#include <stdbool.h>


// Pipe structure for inter-process/thread communication
typedef struct {
  int read_fd;
  int write_fd;
  bool is_socketpair;  // true for builtin-builtin, false for regular pipe
} JShellPipe;


// Create a pipe for command communication
// use_socketpair: true for builtin-to-builtin (bidirectional)
//                 false for regular pipe (unidirectional)
// Returns 0 on success, -1 on failure
int jshell_create_pipe(JShellPipe* p, bool use_socketpair);

// Close both ends of a pipe
void jshell_close_pipe(JShellPipe* p);

// Close only the read end of a pipe
void jshell_close_pipe_read(JShellPipe* p);

// Close only the write end of a pipe
void jshell_close_pipe_write(JShellPipe* p);


#endif
