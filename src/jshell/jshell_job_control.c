#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#include "jshell_job_control.h"
#include "utils/jbox_utils.h"


static BackgroundJob job_list[MAX_JOBS];
static int next_job_id = 1;
static volatile sig_atomic_t sigchld_received = 0;


static void sigchld_handler(int sig) {
  (void)sig;
  sigchld_received = 1;
}


void jshell_init_job_control(void) {
  DPRINT("jshell_init_job_control called");
  
  for (int i = 0; i < MAX_JOBS; i++) {
    job_list[i].in_use = false;
    job_list[i].job_id = 0;
    job_list[i].pid_count = 0;
    job_list[i].status = JOB_RUNNING;
    job_list[i].cmd_string[0] = '\0';
  }
  
  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
  }
  
  DPRINT("Job control initialized with SIGCHLD handler");
}


int jshell_add_background_job(pid_t* pids, size_t pid_count, 
                               const char* cmd_string) {
  DPRINT("jshell_add_background_job called with %zu pids", pid_count);
  
  if (pids == NULL || pid_count == 0 || pid_count > MAX_PIDS_PER_JOB) {
    return -1;
  }
  
  int slot = -1;
  for (int i = 0; i < MAX_JOBS; i++) {
    if (!job_list[i].in_use) {
      slot = i;
      break;
    }
  }
  
  if (slot == -1) {
    fprintf(stderr, "jshell: job table full\n");
    return -1;
  }
  
  BackgroundJob* job = &job_list[slot];
  job->job_id = next_job_id++;
  job->pid_count = pid_count;
  
  for (size_t i = 0; i < pid_count; i++) {
    job->pids[i] = pids[i];
  }
  
  if (cmd_string != NULL) {
    strncpy(job->cmd_string, cmd_string, MAX_CMD_STRING - 1);
    job->cmd_string[MAX_CMD_STRING - 1] = '\0';
  } else {
    job->cmd_string[0] = '\0';
  }
  
  job->status = JOB_RUNNING;
  job->in_use = true;
  
  printf("[%d] %d\n", job->job_id, job->pids[0]);
  
  DPRINT("Added background job [%d] with %zu processes", 
         job->job_id, pid_count);
  
  return job->job_id;
}


BackgroundJob* jshell_find_job_by_pid(pid_t pid) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (!job_list[i].in_use) {
      continue;
    }
    
    for (size_t j = 0; j < job_list[i].pid_count; j++) {
      if (job_list[i].pids[j] == pid) {
        return &job_list[i];
      }
    }
  }
  
  return NULL;
}


void jshell_update_job_status(pid_t pid, int status) {
  DPRINT("jshell_update_job_status called for pid %d", pid);
  
  BackgroundJob* job = jshell_find_job_by_pid(pid);
  if (job == NULL) {
    DPRINT("No job found for pid %d", pid);
    return;
  }
  
  if (WIFEXITED(status) || WIFSIGNALED(status)) {
    bool all_done = true;
    
    for (size_t i = 0; i < job->pid_count; i++) {
      int child_status;
      pid_t result = waitpid(job->pids[i], &child_status, WNOHANG);
      
      if (result == 0) {
        all_done = false;
        break;
      }
    }
    
    if (all_done) {
      job->status = JOB_DONE;
      DPRINT("Job [%d] marked as DONE", job->job_id);
    }
  } else if (WIFSTOPPED(status)) {
    job->status = JOB_STOPPED;
    DPRINT("Job [%d] marked as STOPPED", job->job_id);
  }
}


void jshell_check_background_jobs(void) {
  if (!sigchld_received) {
    return;
  }
  
  sigchld_received = 0;
  
  DPRINT("jshell_check_background_jobs called");
  
  pid_t pid;
  int status;
  
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    DPRINT("Reaped process %d", pid);
    jshell_update_job_status(pid, status);
  }
  
  for (int i = 0; i < MAX_JOBS; i++) {
    if (!job_list[i].in_use) {
      continue;
    }
    
    if (job_list[i].status == JOB_DONE) {
      printf("[%d]  Done                    %s\n", 
             job_list[i].job_id, 
             job_list[i].cmd_string);
      job_list[i].in_use = false;
    }
  }
}


void jshell_print_jobs(void) {
  DPRINT("jshell_print_jobs called");
  
  bool found_any = false;
  
  for (int i = 0; i < MAX_JOBS; i++) {
    if (!job_list[i].in_use) {
      continue;
    }
    
    found_any = true;
    
    const char* status_str;
    switch (job_list[i].status) {
      case JOB_RUNNING:
        status_str = "Running";
        break;
      case JOB_STOPPED:
        status_str = "Stopped";
        break;
      case JOB_DONE:
        status_str = "Done";
        break;
      default:
        status_str = "Unknown";
        break;
    }
    
    printf("[%d]  %-23s %s\n", 
           job_list[i].job_id, 
           status_str, 
           job_list[i].cmd_string);
  }
  
  if (!found_any) {
    DPRINT("No background jobs");
  }
}


void jshell_cleanup_finished_jobs(void) {
  DPRINT("jshell_cleanup_finished_jobs called");

  for (int i = 0; i < MAX_JOBS; i++) {
    if (!job_list[i].in_use) {
      continue;
    }

    if (job_list[i].status == JOB_DONE) {
      DPRINT("Cleaning up job [%d]", job_list[i].job_id);
      job_list[i].in_use = false;
    }
  }
}


BackgroundJob* jshell_find_job_by_id(int job_id) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (job_list[i].in_use && job_list[i].job_id == job_id) {
      return &job_list[i];
    }
  }
  return NULL;
}


size_t jshell_get_job_count(void) {
  size_t count = 0;
  for (int i = 0; i < MAX_JOBS; i++) {
    if (job_list[i].in_use) {
      count++;
    }
  }
  return count;
}


void jshell_for_each_job(void (*callback)(const BackgroundJob *job,
                                          void *userdata),
                         void *userdata) {
  for (int i = 0; i < MAX_JOBS; i++) {
    if (job_list[i].in_use) {
      callback(&job_list[i], userdata);
    }
  }
}


int jshell_wait_for_job(int job_id) {
  BackgroundJob *job = jshell_find_job_by_id(job_id);
  if (job == NULL) {
    return -1;
  }

  int final_status = 0;
  for (size_t i = 0; i < job->pid_count; i++) {
    int status;
    pid_t result = waitpid(job->pids[i], &status, 0);
    if (result > 0) {
      if (WIFEXITED(status)) {
        final_status = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        final_status = 128 + WTERMSIG(status);
      }
    }
  }

  job->status = JOB_DONE;
  job->in_use = false;
  return final_status;
}
