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
- [ ] 2.1: Implement `jshell_exec_job(JShellExecJob* job)` - main execution dispatcher
- [ ] 2.2: Implement `jshell_exec_single_cmd()` - execute single command without pipes
- [ ] 2.3: Implement `jshell_exec_pipeline()` - execute multiple commands with pipes
- [ ] 2.4: Implement `jshell_setup_input_redir()` - handle input redirection for first command
- [ ] 2.5: Implement `jshell_setup_output_redir()` - handle output redirection for last command
- [ ] 2.6: Implement `jshell_create_pipes()` - create pipe array for pipeline
- [ ] 2.7: Implement `jshell_fork_and_exec()` - fork process and exec command
- [ ] 2.8: Implement `jshell_wait_for_jobs()` - wait for foreground/background jobs

### Phase 3: Command Registry Integration
- [ ] 3.1: Implement `jshell_find_builtin()` - check if command is builtin using registry
- [ ] 3.2: Implement `jshell_exec_builtin()` - execute builtin command directly
- [ ] 3.3: Handle builtin vs external command distinction in execution path

### Phase 4: Assignment Job Support
- [ ] 4.1: Implement `jshell_capture_output()` - capture stdout to buffer
- [ ] 4.2: Implement `jshell_set_env_var()` - set environment variable from captured output
- [ ] 4.3: Modify `visitJob()` AssigJob case to extract variable name and call assignment logic
- [ ] 4.4: Handle assignment job execution with output capture

### Phase 5: Error Handling and Cleanup
- [x] 5.1: Implement `jshell_cleanup_job()` - free JShellExecJob resources
- [x] 5.2: Implement `jshell_cleanup_cmd_vector()` - free JShellCmdVector and wordexp results
- [ ] 5.3: Add error handling for fork failures
- [ ] 5.4: Add error handling for exec failures
- [ ] 5.5: Add error handling for pipe creation failures
- [ ] 5.6: Add error handling for file descriptor operations
- [ ] 5.7: Ensure proper cleanup on all error paths

### Phase 6: Background Job Support
- [ ] 6.1: Implement job tracking structure for background jobs
- [ ] 6.2: Modify execution to not wait for BG_JOB type
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
- Execute pipeline normally
- Capture final stdout to buffer
- Trim whitespace from captured output
- Set environment variable using setenv()
- Also display output to user's terminal

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
- Test assignment jobs
- Test background jobs
- Test builtin commands
- Test error conditions
