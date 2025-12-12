#ifndef JSHELL_JOB_CONTROL_H
#define JSHELL_JOB_CONTROL_H

#include <sys/types.h>
#include <stdbool.h>

#define MAX_JOBS 100
#define MAX_PIDS_PER_JOB 50
#define MAX_CMD_STRING 1024


typedef enum {
  JOB_RUNNING,
  JOB_STOPPED,
  JOB_DONE
} JobStatus;


typedef struct {
  int job_id;
  pid_t pids[MAX_PIDS_PER_JOB];
  size_t pid_count;
  char cmd_string[MAX_CMD_STRING];
  JobStatus status;
  bool in_use;
} BackgroundJob;


void jshell_init_job_control(void);

int jshell_add_background_job(pid_t* pids, size_t pid_count, 
                               const char* cmd_string);

void jshell_update_job_status(pid_t pid, int status);

void jshell_check_background_jobs(void);

void jshell_print_jobs(void);

void jshell_cleanup_finished_jobs(void);

BackgroundJob* jshell_find_job_by_pid(pid_t pid);


#endif
