/**
 * @file jshell_socketpair.c
 * @brief Pipe and socketpair creation utilities for inter-process communication.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>

#include "jshell_socketpair.h"
#include "utils/jbox_utils.h"


/**
 * Create a pipe or socketpair for inter-process communication.
 * @param p Pointer to JShellPipe structure to initialize.
 * @param use_socketpair If true, create socketpair; otherwise create pipe.
 * @return 0 on success, -1 on failure.
 */
int jshell_create_pipe(JShellPipe* p, bool use_socketpair) {
  if (p == NULL) {
    return -1;
  }

  p->read_fd = -1;
  p->write_fd = -1;
  p->is_socketpair = use_socketpair;

  int fds[2];

  if (use_socketpair) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
      perror("socketpair");
      return -1;
    }
    DPRINT("Created socketpair: read_fd=%d, write_fd=%d", fds[0], fds[1]);
  } else {
    if (pipe(fds) == -1) {
      perror("pipe");
      return -1;
    }
    DPRINT("Created pipe: read_fd=%d, write_fd=%d", fds[0], fds[1]);
  }

  p->read_fd = fds[0];
  p->write_fd = fds[1];

  return 0;
}


/**
 * Close both ends of a pipe.
 * @param p Pointer to JShellPipe to close.
 */
void jshell_close_pipe(JShellPipe* p) {
  if (p == NULL) {
    return;
  }

  jshell_close_pipe_read(p);
  jshell_close_pipe_write(p);
}


/**
 * Close the read end of a pipe.
 * @param p Pointer to JShellPipe.
 */
void jshell_close_pipe_read(JShellPipe* p) {
  if (p == NULL) {
    return;
  }

  if (p->read_fd != -1) {
    close(p->read_fd);
    p->read_fd = -1;
  }
}


/**
 * Close the write end of a pipe.
 * @param p Pointer to JShellPipe.
 */
void jshell_close_pipe_write(JShellPipe* p) {
  if (p == NULL) {
    return;
  }

  if (p->write_fd != -1) {
    close(p->write_fd);
    p->write_fd = -1;
  }
}
