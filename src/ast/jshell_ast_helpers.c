#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <wordexp.h>
#include <signal.h>
#include <ctype.h>

#include "jshell_ast_interpreter.h"
#include "jshell_ast_helpers.h"
#include "../jshell_cmd_registry.h"
#include "../jbox_debug.h"
#include "../jshell.h"


int jshell_expand_word(char* word, wordexp_t* word_vector_ptr) {
  int flags = WRDE_NOCMD;
  
  if(word_vector_ptr->we_wordc > 0){
    DPRINT("word_vector_ptr->we_wordc = %zu => flags |= WRDE_APPEND", 
           word_vector_ptr->we_wordc);
    flags |= WRDE_APPEND;
  }
  return wordexp(word, word_vector_ptr, flags);
}


static int jshell_setup_input_redir(int input_fd) {
  if (input_fd == -1) {
    return 0;
  }
  
  if (dup2(input_fd, STDIN_FILENO) == -1) {
    perror("dup2 input");
    return -1;
  }
  
  close(input_fd);
  return 0;
}


static int jshell_setup_output_redir(int output_fd) {
  if (output_fd == -1) {
    return 0;
  }
  
  if (dup2(output_fd, STDOUT_FILENO) == -1) {
    perror("dup2 output");
    return -1;
  }
  
  close(output_fd);
  return 0;
}


static int** jshell_create_pipes(size_t pipe_count) {
  if (pipe_count == 0) {
    return NULL;
  }
  
  int** pipes = malloc(sizeof(int*) * pipe_count);
  if (pipes == NULL) {
    perror("malloc pipes");
    return NULL;
  }
  
  for (size_t i = 0; i < pipe_count; i++) {
    pipes[i] = malloc(sizeof(int) * 2);
    if (pipes[i] == NULL) {
      perror("malloc pipe");
      for (size_t j = 0; j < i; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
        free(pipes[j]);
      }
      free(pipes);
      return NULL;
    }
    
    if (pipe(pipes[i]) == -1) {
      perror("pipe");
      free(pipes[i]);
      for (size_t j = 0; j < i; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
        free(pipes[j]);
      }
      free(pipes);
      return NULL;
    }
  }
  
  return pipes;
}


static void jshell_close_pipes(int** pipes, size_t pipe_count) {
  if (pipes == NULL) {
    return;
  }
  
  for (size_t i = 0; i < pipe_count; i++) {
    if (pipes[i] != NULL) {
      close(pipes[i][0]);
      close(pipes[i][1]);
      free(pipes[i]);
    }
  }
  free(pipes);
}


static const jshell_cmd_spec_t* jshell_find_builtin(const char* name) {
  if (name == NULL) {
    return NULL;
  }
  return jshell_find_command(name);
}


static int jshell_exec_builtin(const jshell_cmd_spec_t* spec, 
                                JShellCmdParams* cmd_params,
                                int input_fd,
                                int output_fd) {
  DPRINT("Executing builtin: %s", spec->name);
  
  int saved_stdin = -1;
  int saved_stdout = -1;
  int result = 0;
  
  if (input_fd != -1) {
    saved_stdin = dup(STDIN_FILENO);
    if (saved_stdin == -1) {
      perror("dup stdin");
      return -1;
    }
    if (jshell_setup_input_redir(input_fd) == -1) {
      close(saved_stdin);
      return -1;
    }
  }
  
  if (output_fd != -1) {
    saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout == -1) {
      perror("dup stdout");
      if (saved_stdin != -1) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
      }
      return -1;
    }
    if (jshell_setup_output_redir(output_fd) == -1) {
      close(saved_stdout);
      if (saved_stdin != -1) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
      }
      return -1;
    }
  }
  
  result = spec->run(cmd_params->argc, cmd_params->argv);
  
  if (saved_stdout != -1) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
  }
  
  if (saved_stdin != -1) {
    dup2(saved_stdin, STDIN_FILENO);
    close(saved_stdin);
  }
  
  return result;
}


static int jshell_fork_and_exec(JShellCmdParams* cmd_params, 
                                 int** pipes, 
                                 size_t cmd_index, 
                                 size_t total_cmds,
                                 int input_fd,
                                 int output_fd) {
  DPRINT("Forking for command %zu: %s", cmd_index, cmd_params->argv[0]);
  
  pid_t pid = fork();
  
  if (pid == -1) {
    perror("fork");
    return -1;
  }
  
  if (pid == 0) {
    if (cmd_index == 0 && input_fd != -1) {
      if (jshell_setup_input_redir(input_fd) == -1) {
        exit(EXIT_FAILURE);
      }
    }
    
    if (cmd_index > 0) {
      if (dup2(pipes[cmd_index - 1][0], STDIN_FILENO) == -1) {
        perror("dup2 pipe read");
        exit(EXIT_FAILURE);
      }
    }
    
    if (cmd_index < total_cmds - 1) {
      if (dup2(pipes[cmd_index][1], STDOUT_FILENO) == -1) {
        perror("dup2 pipe write");
        exit(EXIT_FAILURE);
      }
    }
    
    if (cmd_index == total_cmds - 1 && output_fd != -1) {
      if (jshell_setup_output_redir(output_fd) == -1) {
        exit(EXIT_FAILURE);
      }
    }
    
    for (size_t i = 0; i < total_cmds - 1; i++) {
      close(pipes[i][0]);
      close(pipes[i][1]);
    }
    
    execvp(cmd_params->argv[0], cmd_params->argv);
    perror("execvp");
    exit(EXIT_FAILURE);
  }
  
  return pid;
}


static int jshell_wait_for_jobs(pid_t* pids, size_t pid_count, 
                                 ExecJobType job_type) {
  if (job_type == BG_JOB) {
    DPRINT("Background job, not waiting");
    return 0;
  }
  
  DPRINT("Waiting for %zu processes", pid_count);
  int last_status = 0;
  
  for (size_t i = 0; i < pid_count; i++) {
    int status;
    if (waitpid(pids[i], &status, 0) == -1) {
      perror("waitpid");
      return -1;
    }
    
    if (WIFEXITED(status)) {
      last_status = WEXITSTATUS(status);
      DPRINT("Process %d exited with status %d", pids[i], last_status);
    } else if (WIFSIGNALED(status)) {
      DPRINT("Process %d killed by signal %d", pids[i], WTERMSIG(status));
      last_status = 128 + WTERMSIG(status);
    }
  }
  
  return last_status;
}


static int jshell_exec_single_cmd(JShellExecJob* job) {
  DPRINT("jshell_exec_single_cmd called");
  
  JShellCmdParams* cmd_params = 
    &job->jshell_cmd_vector_ptr->jshell_cmd_params_ptr[0];
  
  const jshell_cmd_spec_t* builtin = jshell_find_builtin(cmd_params->argv[0]);
  if (builtin != NULL) {
    DPRINT("Command is builtin: %s", builtin->name);
    return jshell_exec_builtin(builtin, cmd_params, 
                                job->input_fd, job->output_fd);
  }
  
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return -1;
  }
  
  if (pid == 0) {
    if (jshell_setup_input_redir(job->input_fd) == -1) {
      exit(EXIT_FAILURE);
    }
    
    if (jshell_setup_output_redir(job->output_fd) == -1) {
      exit(EXIT_FAILURE);
    }
    
    execvp(cmd_params->argv[0], cmd_params->argv);
    perror("execvp");
    exit(EXIT_FAILURE);
  }
  
  if (job->input_fd != -1) {
    close(job->input_fd);
  }
  if (job->output_fd != -1) {
    close(job->output_fd);
  }
  
  return jshell_wait_for_jobs(&pid, 1, job->exec_job_type);
}


static int jshell_exec_pipeline(JShellExecJob* job) {
  DPRINT("jshell_exec_pipeline called with %zu commands", 
         job->jshell_cmd_vector_ptr->cmd_count);
  
  size_t cmd_count = job->jshell_cmd_vector_ptr->cmd_count;
  size_t pipe_count = cmd_count - 1;
  
  int** pipes = jshell_create_pipes(pipe_count);
  if (pipes == NULL) {
    return -1;
  }
  
  pid_t* pids = malloc(sizeof(pid_t) * cmd_count);
  if (pids == NULL) {
    perror("malloc pids");
    jshell_close_pipes(pipes, pipe_count);
    return -1;
  }
  
  for (size_t i = 0; i < cmd_count; i++) {
    JShellCmdParams* cmd_params = 
      &job->jshell_cmd_vector_ptr->jshell_cmd_params_ptr[i];
    
    const jshell_cmd_spec_t* builtin = 
      jshell_find_builtin(cmd_params->argv[0]);
    
    if (builtin != NULL && cmd_count == 1) {
      DPRINT("Single builtin command in pipeline: %s", builtin->name);
      int result = jshell_exec_builtin(builtin, cmd_params,
                                        job->input_fd, job->output_fd);
      free(pids);
      jshell_close_pipes(pipes, pipe_count);
      return result;
    }
    
    pid_t pid = jshell_fork_and_exec(cmd_params, pipes, i, cmd_count,
                                      job->input_fd, job->output_fd);
    if (pid == -1) {
      for (size_t j = 0; j < i; j++) {
        kill(pids[j], SIGTERM);
      }
      free(pids);
      jshell_close_pipes(pipes, pipe_count);
      return -1;
    }
    
    pids[i] = pid;
  }
  
  if (job->input_fd != -1) {
    close(job->input_fd);
  }
  if (job->output_fd != -1) {
    close(job->output_fd);
  }
  
  jshell_close_pipes(pipes, pipe_count);
  
  int result = jshell_wait_for_jobs(pids, cmd_count, job->exec_job_type);
  free(pids);
  
  return result;
}


void jshell_exec_job(JShellExecJob* job) {
  DPRINT("jshell_exec_job called");
  
  if (job == NULL || job->jshell_cmd_vector_ptr == NULL) {
    return;
  }
  
  DPRINT("Job type: %d, cmd_count: %zu", 
         job->exec_job_type, 
         job->jshell_cmd_vector_ptr->cmd_count);
  
  int result;
  if (job->jshell_cmd_vector_ptr->cmd_count == 1) {
    result = jshell_exec_single_cmd(job);
  } else {
    result = jshell_exec_pipeline(job);
  }
  
  if (result != 0) {
    DPRINT("Command execution failed with status %d", result);
  }
}


char* jshell_capture_and_tee_output(JShellExecJob* job) {
  DPRINT("jshell_capture_and_t ee_output called");
  
  if (job == NULL || job->jshell_cmd_vector_ptr == NULL) {
    return NULL;
  }
  
  int capture_pipe[2];
  if (pipe(capture_pipe) == -1) {
    perror("pipe for capture");
    return NULL;
  }
  
  char* buffer = malloc(MAX_VAR_SIZE);
  if (buffer == NULL) {
    perror("malloc capture buffer");
    close(capture_pipe[0]);
    close(capture_pipe[1]);
    return NULL;
  }
  memset(buffer, 0, MAX_VAR_SIZE);
  
  int original_output_fd = job->output_fd;
  if (original_output_fd == -1) {
    original_output_fd = dup(STDOUT_FILENO);
    if (original_output_fd == -1) {
      perror("dup stdout");
      free(buffer);
      close(capture_pipe[0]);
      close(capture_pipe[1]);
      return NULL;
    }
  }
  
  pid_t tee_pid = fork();
  if (tee_pid == -1) {
    perror("fork tee process");
    free(buffer);
    close(capture_pipe[0]);
    close(capture_pipe[1]);
    if (job->output_fd == -1) {
      close(original_output_fd);
    }
    return NULL;
  }
  
  if (tee_pid == 0) {
    close(capture_pipe[1]);
    
    char tee_buffer[4096];
    ssize_t bytes_read;
    size_t total_captured = 0;
    
    while ((bytes_read = read(capture_pipe[0], tee_buffer, 
                               sizeof(tee_buffer))) > 0) {
      if (write(original_output_fd, tee_buffer, bytes_read) == -1) {
        perror("write to output");
      }
      
      size_t to_copy = (size_t)bytes_read;
      if (total_captured + to_copy >= MAX_VAR_SIZE) {
        to_copy = MAX_VAR_SIZE - total_captured - 1;
      }
      
      if (to_copy > 0) {
        memcpy(buffer + total_captured, tee_buffer, to_copy);
        total_captured += to_copy;
      }
      
      if (total_captured >= MAX_VAR_SIZE - 1) {
        while (read(capture_pipe[0], tee_buffer, sizeof(tee_buffer)) > 0) {
          if (write(original_output_fd, tee_buffer, bytes_read) == -1) {
            perror("write to output");
          }
        }
        break;
      }
    }
    
    buffer[total_captured] = '\0';
    
    close(capture_pipe[0]);
    if (job->output_fd == -1) {
      close(original_output_fd);
    }
    
    exit(EXIT_SUCCESS);
  }
  
  close(capture_pipe[0]);
  
  int saved_output_fd = job->output_fd;
  job->output_fd = capture_pipe[1];
  
  int result;
  if (job->jshell_cmd_vector_ptr->cmd_count == 1) {
    result = jshell_exec_single_cmd(job);
  } else {
    result = jshell_exec_pipeline(job);
  }
  
  close(capture_pipe[1]);
  
  int tee_status;
  if (waitpid(tee_pid, &tee_status, 0) == -1) {
    perror("waitpid tee process");
    free(buffer);
    job->output_fd = saved_output_fd;
    if (saved_output_fd == -1) {
      close(original_output_fd);
    }
    return NULL;
  }
  
  job->output_fd = saved_output_fd;
  if (saved_output_fd == -1) {
    close(original_output_fd);
  }
  
  if (result != 0) {
    DPRINT("Command execution failed with status %d", result);
  }
  
  DPRINT("Captured output in buffer");
  return buffer;
}


int jshell_set_env_var(const char* name, const char* value) {
  DPRINT("jshell_set_env_var: %s = %s", name, value);
  
  if (name == NULL || value == NULL) {
    return -1;
  }
  
  char* trimmed_value = strdup(value);
  if (trimmed_value == NULL) {
    perror("strdup");
    return -1;
  }
  
  char* start = trimmed_value;
  while (isspace((unsigned char)*start)) {
    start++;
  }
  
  char* end = start + strlen(start) - 1;
  while (end > start && isspace((unsigned char)*end)) {
    end--;
  }
  *(end + 1) = '\0';
  
  int result = setenv(name, start, 1);
  if (result == -1) {
    perror("setenv");
  } else {
    DPRINT("Set environment variable: %s=%s", name, start);
  }
  
  free(trimmed_value);
  return result;
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
