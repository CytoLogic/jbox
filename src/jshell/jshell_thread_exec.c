/**
 * @file jshell_thread_exec.c
 * @brief Threaded execution of builtin commands with I/O redirection.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jshell_thread_exec.h"
#include "utils/jbox_utils.h"


/**
 * List of builtin commands that must run in the main thread.
 * These commands either modify shell state or are fast enough that
 * threading overhead isn't worth it.
 */
static const char* MAIN_THREAD_BUILTINS[] = {
  "cd",
  "export",
  "unset",
  "wait",
  "type",     // Fast lookup, avoids ASan thread inspection race
  "help",     // Fast lookup
  "pwd",      // Fast syscall
  "env",      // Fast read
  "history",  // Fast read
  NULL
};


/**
 * Create a deep copy of an argv array.
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 * @return Newly allocated argv copy, or NULL on failure.
 */
static char** copy_argv(int argc, char** argv) {
  if (argc <= 0 || argv == NULL) {
    return NULL;
  }

  char** new_argv = malloc((argc + 1) * sizeof(char*));
  if (new_argv == NULL) {
    return NULL;
  }

  for (int i = 0; i < argc; i++) {
    if (argv[i] != NULL) {
      new_argv[i] = strdup(argv[i]);
      if (new_argv[i] == NULL) {
        for (int j = 0; j < i; j++) {
          free(new_argv[j]);
        }
        free(new_argv);
        return NULL;
      }
    } else {
      new_argv[i] = NULL;
    }
  }
  new_argv[argc] = NULL;

  return new_argv;
}


/**
 * Free a copied argv array.
 * @param argc Number of arguments.
 * @param argv Array of argument strings to free.
 */
static void free_argv(int argc, char** argv) {
  if (argv == NULL) {
    return;
  }

  for (int i = 0; i < argc; i++) {
    if (argv[i] != NULL) {
      free(argv[i]);
    }
  }
  free(argv);
}


/**
 * Thread entry point for executing a builtin command.
 * Handles I/O redirection, runs the command, and signals completion.
 * @param arg Pointer to JShellBuiltinThread structure.
 * @return NULL (pthread return value).
 */
static void* builtin_thread_entry(void* arg) {
  JShellBuiltinThread* bt = (JShellBuiltinThread*)arg;

  DPRINT("Thread entry for builtin: %s", bt->spec->name);

  int saved_stdin = -1;
  int saved_stdout = -1;

  if (bt->input_fd != -1) {
    saved_stdin = dup(STDIN_FILENO);
    if (saved_stdin == -1) {
      perror("dup stdin in thread");
      bt->exit_code = 1;
      goto cleanup;
    }
    if (dup2(bt->input_fd, STDIN_FILENO) == -1) {
      perror("dup2 input in thread");
      close(saved_stdin);
      bt->exit_code = 1;
      goto cleanup;
    }
    close(bt->input_fd);
    bt->input_fd = -1;
  }

  if (bt->output_fd != -1) {
    saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout == -1) {
      perror("dup stdout in thread");
      if (saved_stdin != -1) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
      }
      bt->exit_code = 1;
      goto cleanup;
    }
    if (dup2(bt->output_fd, STDOUT_FILENO) == -1) {
      perror("dup2 output in thread");
      close(saved_stdout);
      if (saved_stdin != -1) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
      }
      bt->exit_code = 1;
      goto cleanup;
    }
    close(bt->output_fd);
    bt->output_fd = -1;
  }

  bt->exit_code = bt->spec->run(bt->argc, bt->argv);

  // Flush stdout to ensure all output goes to the redirected fd
  fflush(stdout);

  DPRINT("Thread builtin %s completed with exit code %d",
         bt->spec->name, bt->exit_code);

  if (saved_stdout != -1) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
  }

  if (saved_stdin != -1) {
    dup2(saved_stdin, STDIN_FILENO);
    close(saved_stdin);
  }

cleanup:
  pthread_mutex_lock(&bt->mutex);
  bt->completed = true;
  pthread_cond_signal(&bt->cond);
  pthread_mutex_unlock(&bt->mutex);

  return NULL;
}


/**
 * Check if a builtin command must run on the main thread.
 * @param cmd_name Name of the builtin command.
 * @return true if command requires main thread, false otherwise.
 */
bool jshell_builtin_requires_main_thread(const char* cmd_name) {
  if (cmd_name == NULL) {
    return false;
  }

  for (int i = 0; MAIN_THREAD_BUILTINS[i] != NULL; i++) {
    if (strcmp(cmd_name, MAIN_THREAD_BUILTINS[i]) == 0) {
      return true;
    }
  }

  return false;
}


/**
 * Spawn a new thread to execute a builtin command.
 * @param spec Command specification for the builtin.
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 * @param input_fd File descriptor for stdin redirection (-1 for none).
 * @param output_fd File descriptor for stdout redirection (-1 for none).
 * @return Pointer to JShellBuiltinThread on success, NULL on failure.
 */
JShellBuiltinThread* jshell_spawn_builtin_thread(
    const jshell_cmd_spec_t* spec,
    int argc,
    char** argv,
    int input_fd,
    int output_fd) {

  if (spec == NULL || argc <= 0 || argv == NULL) {
    return NULL;
  }

  JShellBuiltinThread* bt = calloc(1, sizeof(JShellBuiltinThread));
  if (bt == NULL) {
    perror("calloc JShellBuiltinThread");
    return NULL;
  }

  bt->spec = spec;
  bt->argc = argc;
  bt->argv = copy_argv(argc, argv);
  if (bt->argv == NULL) {
    free(bt);
    return NULL;
  }

  bt->input_fd = input_fd;
  bt->output_fd = output_fd;
  bt->exit_code = 0;
  bt->completed = false;

  if (pthread_mutex_init(&bt->mutex, NULL) != 0) {
    perror("pthread_mutex_init");
    free_argv(bt->argc, bt->argv);
    free(bt);
    return NULL;
  }

  if (pthread_cond_init(&bt->cond, NULL) != 0) {
    perror("pthread_cond_init");
    pthread_mutex_destroy(&bt->mutex);
    free_argv(bt->argc, bt->argv);
    free(bt);
    return NULL;
  }

  if (pthread_create(&bt->thread, NULL, builtin_thread_entry, bt) != 0) {
    perror("pthread_create");
    pthread_cond_destroy(&bt->cond);
    pthread_mutex_destroy(&bt->mutex);
    free_argv(bt->argc, bt->argv);
    free(bt);
    return NULL;
  }

  DPRINT("Spawned thread for builtin: %s", spec->name);
  return bt;
}


/**
 * Wait for a builtin thread to complete execution.
 * Blocks until the thread finishes.
 * @param bt Pointer to JShellBuiltinThread to wait for.
 * @return Exit code from the builtin command, or -1 if bt is NULL.
 */
int jshell_wait_builtin_thread(JShellBuiltinThread* bt) {
  if (bt == NULL) {
    return -1;
  }

  DPRINT("Waiting for builtin thread: %s", bt->spec->name);

  pthread_join(bt->thread, NULL);

  return bt->exit_code;
}


/**
 * Free resources associated with a builtin thread.
 * Thread must have been joined before calling this function.
 * @param bt Pointer to JShellBuiltinThread to free.
 */
void jshell_free_builtin_thread(JShellBuiltinThread* bt) {
  if (bt == NULL) {
    return;
  }

  DPRINT("Freeing builtin thread resources: %s", bt->spec->name);

  pthread_cond_destroy(&bt->cond);
  pthread_mutex_destroy(&bt->mutex);

  if (bt->argv != NULL) {
    free_argv(bt->argc, bt->argv);
  }

  if (bt->input_fd != -1) {
    close(bt->input_fd);
  }
  if (bt->output_fd != -1) {
    close(bt->output_fd);
  }

  free(bt);
}
