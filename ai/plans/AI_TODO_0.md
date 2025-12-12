# JShell Command Execution Implementation Plan

## Overview
Implement command execution logic for the JShell interpreter. Commands are parsed into a JShellCmdVector, wrapped in a JShellExecJob with I/O redirection info, then executed with proper piping and environment variable assignment support.

## Architecture
- **jshell_ast_interpreter.c**: Walks AST, builds data structures, delegates execution
- **jshell_ast_helpers.c**: Contains execution logic, process management, piping
- **Data flow**: AST → JShellCmdVector → JShellExecJob → Execution

## Tasks

### Phase 1: Data Structure Building in jshell_ast_interpreter.c
- [x] 1.1: Modify `visitCommandPart()` to count commands and allocate JShellCmdParams array
- [x] 1.2: Modify `visitCommandPart()` to populate JShellCmdParams array by calling `visitCommandUnit()`
- [x] 1.3: Modify `visitCommandPart()` to return populated JShellCmdVector
- [x] 1.4: Modify `visitOptionalInputRedirection()` to return file descriptor (or -1 for none)
- [x] 1.5: Modify `visitOptionalOutputRedirection()` to return file descriptor (or -1 for none)
- [x] 1.6: Modify `visitCommandLine()` to build and return complete JShellExecJob structure
- [x] 1.7: Modify `visitJob()` to build JShellExecJob and call execution function from helpers

### Phase 2: Execution Logic in jshell_ast_helpers.c
- [x] 2.1: Implement `jshell_exec_job(JShellExecJob* job)` - main execution dispatcher
- [x] 2.2: Implement `jshell_exec_single_cmd()` - execute single command without pipes
- [x] 2.3: Implement `jshell_exec_pipeline()` - execute multiple commands with pipes
- [x] 2.4: Implement `jshell_setup_input_redir()` - handle input redirection for first command
- [x] 2.5: Implement `jshell_setup_output_redir()` - handle output redirection for last command
- [x] 2.6: Implement `jshell_create_pipes()` - create pipe array for pipeline
- [x] 2.7: Implement `jshell_fork_and_exec()` - fork process and exec command
- [x] 2.8: Implement `jshell_wait_for_jobs()` - wait for foreground/background jobs

### Phase 3: Command Registry Integration
- [x] 3.1: Implement `jshell_find_builtin()` - check if command is builtin using registry
- [x] 3.2: Implement `jshell_exec_builtin()` - execute builtin command directly
- [x] 3.3: Handle builtin vs external command distinction in execution path

### Phase 4: Assignment Job Support
- [x] 4.1: Implement `jshell_capture_and_tee_output()` - capture stdout while teeing to original destination
- [x] 4.2: Implement `jshell_set_env_var()` - set environment variable from captured output with whitespace trimming
- [x] 4.3: Modify `visitJob()` AssigJob case to extract variable name and call assignment logic
- [x] 4.4: Handle assignment job execution with output capture and tee functionality
- [ ] 4.5: **BUG FIX NEEDED**: Current implementation has IPC issue - tee process cannot communicate buffer back to parent. Need to use shared memory (mmap) or additional pipe for buffer communication.

### Phase 5: Error Handling and Cleanup
- [x] 5.1: Implement `jshell_cleanup_job()` - free JShellExecJob resources
- [x] 5.2: Implement `jshell_cleanup_cmd_vector()` - free JShellCmdVector and wordexp results
- [x] 5.3: Add error handling for fork failures
- [x] 5.4: Add error handling for exec failures
- [x] 5.5: Add error handling for pipe creation failures
- [x] 5.6: Add error handling for file descriptor operations
- [x] 5.7: Ensure proper cleanup on all error paths

### Phase 6: Background Job Support
- [ ] 6.1: Implement job tracking structure for background jobs
- [ ] 6.2: Modify execution to not wait for BG_JOB type (already done in `jshell_wait_for_jobs()`)
- [ ] 6.3: Implement background job status checking
- [ ] 6.4: Handle SIGCHLD for background job completion

## Implementation Notes

### File Descriptor Management
- Input redirection: stdin of first command
- Output redirection: stdout of last command  
- Pipes: stdout of cmd[i] → stdin of cmd[i+1]
- Close unused pipe ends in parent and child processes

### Process Management
- Fork for each command in pipeline
- Parent waits for all children (foreground) or none (background)
- Store PIDs for job control

### Assignment Jobs
- Execute pipeline normally with stdout redirected to capture pipe
- Fork a tee process that reads from capture pipe and:
  - Writes to original output destination (stdout or redirected file)
  - Accumulates data in buffer (up to MAX_VAR_SIZE)
- After command completes, retrieve captured buffer from tee process
- Trim whitespace from captured output
- Set environment variable using setenv()
- Output is displayed to user's terminal as side effect of tee process

### Known Issues
- **CRITICAL BUG in `jshell_capture_and_tee_output()`**: The tee child process accumulates the buffer locally but has no mechanism to communicate it back to the parent process. The current code attempts to read from a pipe that was never written to. Need to implement one of:
  1. Shared memory using mmap() with MAP_SHARED
  2. Additional pipe for tee process to write buffer back to parent
  3. Temporary file for buffer communication

### Builtin Commands
- Check command registry first
- Execute builtins in shell process (no fork)
- Handle I/O redirection for builtins differently

## Dependencies
- wordexp() for token expansion (already implemented)
- Command registry from src/hello/ (already implemented)
- File descriptor operations (open, close, dup2, pipe)
- Process operations (fork, execvp, wait, waitpid)
- Environment operations (setenv, getenv)

## Testing Strategy
- Test single command execution
- Test pipeline with 2 commands
- Test pipeline with 3+ commands
- Test input redirection
- Test output redirection
- Test combined I/O redirection
- Test assignment jobs (BLOCKED by bug 4.5)
- Test background jobs
- Test builtin commands
- Test error conditions
