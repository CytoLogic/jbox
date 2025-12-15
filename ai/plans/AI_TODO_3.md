# Shell Execution Implementation Plan

## Overview

Complete the shell's AST execution system with proper command dispatch, threading for builtins, and socketpair-based pipes. This plan focuses on the execution engine in `src/ast/` and `src/jshell/`.

### Current State

The shell has:
- BNFC-generated parser producing AST nodes (`gen/bnfc/Absyn.h`)
- AST interpreter that visits nodes (`src/ast/jshell_ast_interpreter.c`)
- Basic execution helpers (`src/ast/jshell_ast_helpers.c`)
- Command registry with 18 builtins and 18 external commands
- Job control with background job tracking

### Goals

1. **External Command Path Priority**: Execute from `~/.jshell/bin` first, fallback to system PATH
2. **Threaded Builtin Execution**: Run builtins in pthreads instead of main process
3. **Socketpair Pipes**: Use socketpairs for builtin-to-builtin pipes
4. **Comprehensive Tests**: Python unittest suite under `tests/jshell/`

### Out of Scope

- `@<prompt>` AI chat integration (AIQueryToken)
- `@!<command>` AI execution integration (AIExecToken)

---

## Phase 1: Command Path Resolution

### 1.1 Create Path Resolution Module
**Files**: `src/jshell/jshell_path.h`, `src/jshell/jshell_path.c`

- [ ] Create `src/jshell/jshell_path.h`:
  - [ ] Declare `char* jshell_resolve_command(const char* cmd_name);`
  - [ ] Declare `void jshell_init_path(void);`
  - [ ] Declare `void jshell_cleanup_path(void);`
  - [ ] Declare `const char* jshell_get_bin_dir(void);`

- [ ] Create `src/jshell/jshell_path.c`:
  - [ ] Implement `jshell_init_path()`:
    - [ ] Get user home directory from `$HOME` or `getpwuid(getuid())`
    - [ ] Construct `~/.jshell/bin` path
    - [ ] Create directory if it doesn't exist (`mkdir -p`)
    - [ ] Prepend to `$PATH` environment variable
  - [ ] Implement `jshell_get_bin_dir()`:
    - [ ] Return the `~/.jshell/bin` path
  - [ ] Implement `jshell_resolve_command(cmd_name)`:
    - [ ] First check if command is registered as external in registry
    - [ ] If registered external, check `~/.jshell/bin/<cmd_name>`
    - [ ] If found and executable, return full path
    - [ ] Otherwise, search system PATH using `access()` with `X_OK`
    - [ ] Return NULL if not found
  - [ ] Implement `jshell_cleanup_path()`:
    - [ ] Free any allocated path strings

### 1.2 Update Shell Initialization
**File**: `src/jshell/jshell.c`

- [ ] Add `#include "jshell_path.h"`
- [ ] Call `jshell_init_path()` early in `jshell_main()` (before command registration)
- [ ] Call `jshell_cleanup_path()` in shell cleanup/exit

### 1.3 Update External Command Execution
**File**: `src/ast/jshell_ast_helpers.c`

- [ ] Update `jshell_fork_and_exec()`:
  - [ ] Before `execvp()`, call `jshell_resolve_command()` to get full path
  - [ ] If resolved path exists, use `execv()` with full path
  - [ ] If not resolved, fall back to `execvp()` for system PATH lookup
  - [ ] Handle case where command is not found anywhere

### 1.4 Create Path Tests
**File**: `tests/jshell/test_path.py`

- [ ] Test `~/.jshell/bin` directory creation
- [ ] Test command resolution priority (local bin first)
- [ ] Test fallback to system PATH
- [ ] Test non-existent command handling

---

## Phase 2: Threaded Builtin Execution

### 2.1 Create Thread Execution Module
**Files**: `src/jshell/jshell_thread_exec.h`, `src/jshell/jshell_thread_exec.c`

- [ ] Create `src/jshell/jshell_thread_exec.h`:
  - [ ] Include `<pthread.h>`
  - [ ] Define `JShellBuiltinThread` struct:
    ```c
    typedef struct {
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
    ```
  - [ ] Declare `JShellBuiltinThread* jshell_spawn_builtin_thread(...);`
  - [ ] Declare `int jshell_wait_builtin_thread(JShellBuiltinThread* bt);`
  - [ ] Declare `void jshell_free_builtin_thread(JShellBuiltinThread* bt);`

- [ ] Create `src/jshell/jshell_thread_exec.c`:
  - [ ] Implement thread entry function `builtin_thread_entry(void* arg)`:
    - [ ] Cast arg to `JShellBuiltinThread*`
    - [ ] Set up stdin/stdout redirection using thread-local dup2
    - [ ] Call `spec->run(argc, argv)`
    - [ ] Store exit code
    - [ ] Signal completion via condition variable
    - [ ] Restore original fds if needed
  - [ ] Implement `jshell_spawn_builtin_thread()`:
    - [ ] Allocate `JShellBuiltinThread` struct
    - [ ] Copy argv (deep copy for thread safety)
    - [ ] Initialize mutex and condition variable
    - [ ] Create thread with `pthread_create()`
    - [ ] Return thread handle
  - [ ] Implement `jshell_wait_builtin_thread()`:
    - [ ] Use `pthread_join()` to wait for completion
    - [ ] Return exit code
  - [ ] Implement `jshell_free_builtin_thread()`:
    - [ ] Free copied argv
    - [ ] Destroy mutex and condition variable
    - [ ] Free struct

### 2.2 Update Builtin Execution in AST Helpers
**File**: `src/ast/jshell_ast_helpers.c`

- [ ] Add `#include "jshell_thread_exec.h"`
- [ ] Refactor `jshell_exec_builtin()` to use threads:
  - [ ] Create `JShellBuiltinThread` via `jshell_spawn_builtin_thread()`
  - [ ] If foreground job, wait immediately with `jshell_wait_builtin_thread()`
  - [ ] If background job, add to job tracking (new mechanism needed)
  - [ ] Clean up thread resources after completion

### 2.3 Thread-Safe Builtin Considerations
**File**: `src/jshell/builtins/*.c`

- [ ] Audit builtins for thread safety:
  - [ ] `cd`: Must NOT run in thread (affects main process CWD)
  - [ ] `export`/`unset`: Must NOT run in thread (affects main environment)
  - [ ] `jobs`: Needs mutex protection for job table access
  - [ ] `history`: Needs mutex protection for history access
  - [ ] Others: Generally safe for threading

- [ ] Create list of "main-thread-only" builtins:
  - [ ] cd, export, unset, wait (these modify shell state)

- [ ] Update `jshell_exec_builtin()` to check if builtin requires main thread:
  - [ ] If main-thread-only, execute directly (current behavior)
  - [ ] Otherwise, spawn thread

### 2.4 Create Thread Execution Tests
**File**: `tests/jshell/test_thread_exec.py`

- [ ] Test single builtin execution in thread
- [ ] Test multiple concurrent builtin threads
- [ ] Test thread cleanup after completion
- [ ] Test main-thread-only builtins execute correctly
- [ ] Test exit code propagation from threads

---

## Phase 3: Socketpair Pipes for Builtins

### 3.1 Create Socketpair Pipe Module
**Files**: `src/jshell/jshell_socketpair.h`, `src/jshell/jshell_socketpair.c`

- [ ] Create `src/jshell/jshell_socketpair.h`:
  - [ ] Define `JShellPipe` struct:
    ```c
    typedef struct {
      int read_fd;
      int write_fd;
      bool is_socketpair;  // true for builtin-builtin, false for regular pipe
    } JShellPipe;
    ```
  - [ ] Declare `int jshell_create_pipe(JShellPipe* p, bool use_socketpair);`
  - [ ] Declare `void jshell_close_pipe(JShellPipe* p);`

- [ ] Create `src/jshell/jshell_socketpair.c`:
  - [ ] Implement `jshell_create_pipe()`:
    - [ ] If `use_socketpair`:
      - [ ] Call `socketpair(AF_UNIX, SOCK_STREAM, 0, fds)`
      - [ ] Set read_fd and write_fd from socketpair
    - [ ] Else:
      - [ ] Call `pipe(fds)`
      - [ ] Set read_fd and write_fd from pipe
    - [ ] Set `is_socketpair` flag
  - [ ] Implement `jshell_close_pipe()`:
    - [ ] Close both fds if open

### 3.2 Update Pipeline Execution
**File**: `src/ast/jshell_ast_helpers.c`

- [ ] Refactor `jshell_exec_pipeline()`:
  - [ ] Analyze pipeline to determine which commands are builtins
  - [ ] For builtin-to-builtin connections, use socketpair
  - [ ] For external-to-external or mixed, use regular pipe
  - [ ] Create array of `JShellPipe` structs for pipeline
  - [ ] For each command in pipeline:
    - [ ] If builtin: spawn thread with appropriate fds
    - [ ] If external: fork with appropriate fds
  - [ ] Wait for all threads/processes in order
  - [ ] Close all pipe fds in parent
  - [ ] Return exit code of last command

### 3.3 Pipeline Command Type Detection
**File**: `src/ast/jshell_ast_helpers.c`

- [ ] Add helper function `jshell_is_builtin_command(const char* name)`:
  - [ ] Look up in registry
  - [ ] Return true if found and type == CMD_BUILTIN

- [ ] Add helper function `jshell_pipeline_needs_socketpair(JShellCmdVector* vec, int idx)`:
  - [ ] Check if command at idx is builtin
  - [ ] Check if command at idx+1 is builtin
  - [ ] Return true if both are builtins

### 3.4 Create Socketpair Pipe Tests
**File**: `tests/jshell/test_pipes.py`

- [ ] Test simple two-command builtin pipeline (e.g., `env | rg PATH`)
- [ ] Test builtin-to-external pipeline
- [ ] Test external-to-builtin pipeline
- [ ] Test multi-command mixed pipeline
- [ ] Test pipe data integrity
- [ ] Test large data through socketpair

---

## Phase 4: Complete AST Execution

### 4.1 Review and Complete Job Types
**File**: `src/ast/jshell_ast_interpreter.c`

- [ ] Verify `visitJob()` handles all job types:
  - [ ] `AssigJob`: Variable assignment with command (VAR=$(cmd))
  - [ ] `FGJob`: Foreground job execution
  - [ ] `BGJob`: Background job execution
  - [ ] `AIChatJob`: Skip for now (return placeholder message)
  - [ ] `AIExecJob`: Skip for now (return placeholder message)

- [ ] Verify `visitCommandLine()` handles:
  - [ ] Input redirection (`< file`)
  - [ ] Output redirection (`> file`, `>> file`)
  - [ ] Here documents (`<< EOF`) - if supported by grammar

### 4.2 Variable Expansion and Assignment
**File**: `src/ast/jshell_ast_helpers.c`

- [ ] Review `jshell_expand_word()`:
  - [ ] Ensure `$VAR` expansion works
  - [ ] Ensure `${VAR}` expansion works
  - [ ] Ensure tilde expansion works (`~` to home)
  - [ ] Ensure glob expansion works (`*.c`, `?`, `[a-z]`)

- [ ] Review `jshell_set_env_var()`:
  - [ ] Handle `VAR=value` (simple assignment)
  - [ ] Handle `VAR=$(command)` (command substitution capture)

### 4.3 Error Handling Improvements
**File**: `src/ast/jshell_ast_helpers.c`

- [ ] Add proper error messages for:
  - [ ] Command not found
  - [ ] Permission denied (non-executable file)
  - [ ] Redirection file errors (can't open, can't create)
  - [ ] Pipe creation failures
  - [ ] Thread creation failures
  - [ ] Fork failures

- [ ] Ensure exit codes are correct:
  - [ ] 0 for success
  - [ ] 1 for general errors
  - [ ] 126 for permission denied
  - [ ] 127 for command not found
  - [ ] 128+N for signal N termination

### 4.4 Create AST Execution Tests
**File**: `tests/jshell/test_ast_exec.py`

- [ ] Test simple command execution
- [ ] Test command with arguments
- [ ] Test quoted arguments (single and double)
- [ ] Test variable expansion
- [ ] Test glob expansion
- [ ] Test tilde expansion
- [ ] Test input redirection
- [ ] Test output redirection (overwrite and append)
- [ ] Test pipeline execution
- [ ] Test background jobs
- [ ] Test variable assignment
- [ ] Test command substitution (VAR=$(cmd))

---

## Phase 5: Shell Session Integration

### 5.1 Exit Code Tracking
**File**: `src/jshell/jshell.c`

- [ ] Add `int jshell_last_exit_code` global variable
- [ ] Update after each command execution
- [ ] Make accessible via `$?` variable (requires interpreter update)

### 5.2 Signal Handling Improvements
**File**: `src/jshell/jshell.c`

- [ ] Ensure SIGINT (Ctrl+C) interrupts current command but not shell
- [ ] Ensure SIGTSTP (Ctrl+Z) stops current foreground job
- [ ] Ensure SIGCHLD properly reaps background jobs
- [ ] Ensure SIGPIPE is ignored in main shell process

### 5.3 Interactive Shell Improvements
**File**: `src/jshell/jshell.c`

- [ ] Update `jshell_interactive()`:
  - [ ] Call `jshell_check_background_jobs()` before each prompt
  - [ ] Print job completion notifications
  - [ ] Handle empty input lines gracefully
  - [ ] Handle EOF (Ctrl+D) to exit shell

### 5.4 Create Shell Session Tests
**File**: `tests/jshell/test_session.py`

- [ ] Test shell startup and initialization
- [ ] Test exit code tracking (`$?`)
- [ ] Test background job notifications
- [ ] Test shell exit on EOF
- [ ] Test command history integration

---

## Phase 6: Comprehensive Test Suite

### 6.1 Test Infrastructure
**Files**: `tests/jshell/__init__.py`, `tests/jshell/conftest.py`

- [ ] Create `tests/jshell/__init__.py` (empty, for Python package)
- [ ] Create `tests/jshell/conftest.py`:
  - [ ] Add fixtures for shell binary path
  - [ ] Add fixtures for temporary directories
  - [ ] Add helper functions for running shell commands
  - [ ] Add helper class `JShellRunner` for test isolation

### 6.2 JShellRunner Helper Class
**File**: `tests/jshell/jshell_runner.py`

- [ ] Create `JShellRunner` class:
  ```python
  class JShellRunner:
      def __init__(self, jbox_bin_path):
          self.jbox = jbox_bin_path

      def run(self, command, input_text=None, timeout=5):
          """Run a single command and return result"""

      def run_script(self, script, timeout=10):
          """Run multiple commands from a script string"""

      def start_interactive(self):
          """Start interactive session for complex tests"""
  ```

### 6.3 Test Files Organization

Create the following test files:

- [ ] `tests/jshell/test_path.py` - Command path resolution (Phase 1)
- [ ] `tests/jshell/test_thread_exec.py` - Threaded builtin execution (Phase 2)
- [ ] `tests/jshell/test_pipes.py` - Pipe and socketpair tests (Phase 3)
- [ ] `tests/jshell/test_ast_exec.py` - AST execution tests (Phase 4)
- [ ] `tests/jshell/test_session.py` - Shell session tests (Phase 5)
- [ ] `tests/jshell/test_builtins.py` - Builtin command integration
- [ ] `tests/jshell/test_externals.py` - External command integration
- [ ] `tests/jshell/test_redirection.py` - I/O redirection tests
- [ ] `tests/jshell/test_jobs.py` - Job control tests
- [ ] `tests/jshell/test_variables.py` - Variable expansion tests
- [ ] `tests/jshell/test_quoting.py` - Quoting and escaping tests

### 6.4 Test Coverage Requirements

Each test file should cover:

- [ ] Normal operation cases
- [ ] Edge cases (empty input, special characters, large data)
- [ ] Error cases (invalid input, missing files, permissions)
- [ ] Exit code verification
- [ ] JSON output verification (for --json commands)

---

## Phase 7: Makefile and Build Updates

### 7.1 Update Main Makefile
**File**: `Makefile`

- [ ] Add new source files to `JSHELL_SRCS`:
  - [ ] `jshell_path.c`
  - [ ] `jshell_thread_exec.c`
  - [ ] `jshell_socketpair.c`

- [ ] Add pthread linking:
  - [ ] Add `-lpthread` to linker flags

- [ ] Add test targets:
  - [ ] `test-jshell`: Run all jshell tests
  - [ ] `test-jshell-path`: Run path resolution tests
  - [ ] `test-jshell-exec`: Run execution tests
  - [ ] `test-jshell-pipes`: Run pipe tests

### 7.2 Update tests/Makefile
**File**: `tests/Makefile`

- [ ] Add `jshell` test targets
- [ ] Add `test-all` target that includes jshell tests

---

## Implementation Order

### Sprint 1: Foundation (Path Resolution)
1. Phase 1.1: Create path resolution module
2. Phase 1.2: Update shell initialization
3. Phase 1.3: Update external command execution
4. Phase 1.4: Create path tests
5. Phase 6.1-6.2: Create test infrastructure

### Sprint 2: Threading (Builtin Execution)
1. Phase 2.1: Create thread execution module
2. Phase 2.2: Update builtin execution
3. Phase 2.3: Handle thread-safe builtins
4. Phase 2.4: Create thread execution tests

### Sprint 3: Pipes (Socketpair Integration)
1. Phase 3.1: Create socketpair pipe module
2. Phase 3.2: Update pipeline execution
3. Phase 3.3: Add pipeline type detection
4. Phase 3.4: Create pipe tests

### Sprint 4: Completion (AST and Session)
1. Phase 4.1: Complete job type handling
2. Phase 4.2: Variable expansion review
3. Phase 4.3: Error handling improvements
4. Phase 4.4: Create AST execution tests
5. Phase 5.1-5.4: Shell session improvements

### Sprint 5: Polish (Tests and Build)
1. Phase 6.3-6.4: Complete test suite
2. Phase 7.1-7.2: Build system updates
3. Final integration testing

---

## Notes

### Threading Considerations

- Main-thread-only builtins: `cd`, `export`, `unset`, `wait`
- Thread-local file descriptors: Each thread manages its own stdin/stdout
- Mutex protection needed for: job table, history, command registry (if dynamic)

### Socketpair vs Pipe

- Use socketpair for builtin-to-builtin: bidirectional, better for threads
- Use regular pipe for external processes: standard Unix behavior
- Mixed pipelines: use appropriate type for each connection

### Exit Code Conventions

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | General error |
| 2 | Misuse of command |
| 126 | Permission denied |
| 127 | Command not found |
| 128 | Invalid exit argument |
| 128+N | Signal N received |

### Test Running

```bash
# Run all jshell tests
python -m unittest discover -s tests/jshell -v

# Run specific test file
python -m unittest tests.jshell.test_path -v

# Run specific test case
python -m unittest tests.jshell.test_path.TestPathResolution.test_local_bin_priority -v
```

---

## Dependencies

### Required Libraries

- `pthread`: POSIX threads for builtin execution
- Standard C library: `wordexp`, `socketpair`, `pipe`, `fork`, `exec*`
- Existing: `argtable3` (already in project)

### Required Files (Existing)

- `src/ast/jshell_ast_interpreter.c` - AST visitor
- `src/ast/jshell_ast_helpers.c` - Execution helpers
- `src/jshell/jshell_cmd_registry.c` - Command lookup
- `src/jshell/jshell_job_control.c` - Job tracking

---

## Checklist Summary

- [ ] Phase 1: Command Path Resolution (4 tasks)
- [ ] Phase 2: Threaded Builtin Execution (4 tasks)
- [ ] Phase 3: Socketpair Pipes for Builtins (4 tasks)
- [ ] Phase 4: Complete AST Execution (4 tasks)
- [ ] Phase 5: Shell Session Integration (4 tasks)
- [ ] Phase 6: Comprehensive Test Suite (4 tasks)
- [ ] Phase 7: Makefile and Build Updates (2 tasks)

Total: 26 major tasks across 7 phases
