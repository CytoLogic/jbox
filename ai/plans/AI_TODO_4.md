# Signal Handling Implementation Plan

## Overview

Implement comprehensive signal handling throughout the shell and all commands to ensure:
- Graceful handling of Ctrl+C (SIGINT) without killing the shell
- Proper cleanup on termination signals (SIGTERM, SIGHUP)
- Prevention of crashes from broken pipes (SIGPIPE)
- Job control signals (SIGTSTP, SIGTTIN, SIGTTOU)
- Signal reset in child processes after fork()
- Interruptible operations in external apps

### Current State

The shell has:
- SIGCHLD handler in `src/jshell/jshell_job_control.c` (reaps background jobs)
- SIGWINCH handler in `src/apps/vi/cmd_vi.c` and `src/apps/less/cmd_less.c` (terminal resize)
- Signal table in `src/jshell/builtins/cmd_kill.c` (for `kill` command)
- `sleep` command handles EINTR from `nanosleep()` correctly

### Missing Signal Handling

**Shell Level:**
- SIGINT (Ctrl+C) - currently kills shell process
- SIGTERM - no graceful shutdown
- SIGHUP - no cleanup of jobs or history save
- SIGPIPE - can crash shell on broken pipe
- SIGTSTP (Ctrl+Z) - stops shell instead of foreground job

**Child Processes:**
- Signal handlers inherited from parent (should reset to SIG_DFL)

**External Apps:**
- Most apps have no signal handling
- Long-running operations not interruptible

---

## Implementation Progress

| Phase | Status | Commit | Tests |
|-------|--------|--------|-------|
| Phase 1: Core Signal Infrastructure | ✅ COMPLETED | 5b49766 | - |
| Phase 2: Shell Signal Handlers | ✅ COMPLETED | 32ae8df | - |
| Phase 3: Child Process Signal Reset | ✅ COMPLETED | fa8e60c | - |
| Phase 4: External App Signal Handling | ✅ COMPLETED | - | ✓ |
| Phase 5: Interactive App Signals | PENDING | - | - |
| Phase 6: Builtin Command Signals | PENDING | - | - |
| Phase 7: Test Suite | PENDING | - | - |

---

## Phase 1: Core Signal Infrastructure ✅ COMPLETED

### 1.1 Create Signal Utility Module ✅
**Files**: `src/jshell/jshell_signals.h`, `src/jshell/jshell_signals.c`

- [x] Create `src/jshell/jshell_signals.h`:
  - [x] Declare `void jshell_init_signals(void);`
  - [x] Declare `void jshell_reset_signals_for_child(void);`
  - [x] Declare `void jshell_block_signals(sigset_t *oldmask);`
  - [x] Declare `void jshell_unblock_signals(const sigset_t *oldmask);`
  - [x] Declare `volatile sig_atomic_t jshell_interrupted;` (global flag)
  - [x] Declare `volatile sig_atomic_t jshell_received_sigterm;`
  - [x] Declare `volatile sig_atomic_t jshell_received_sighup;`

- [x] Create `src/jshell/jshell_signals.c`:
  - [x] Implement SIGINT handler:
    ```c
    static void sigint_handler(int sig) {
      (void)sig;
      jshell_interrupted = 1;
    }
    ```
  - [x] Implement SIGTERM handler:
    ```c
    static void sigterm_handler(int sig) {
      (void)sig;
      jshell_received_sigterm = 1;
    }
    ```
  - [x] Implement SIGHUP handler:
    ```c
    static void sighup_handler(int sig) {
      (void)sig;
      jshell_received_sighup = 1;
    }
    ```
  - [x] Implement `jshell_init_signals()`:
    - [x] Set up SIGINT handler with `sigaction()`
    - [x] Set up SIGTERM handler with `sigaction()`
    - [x] Set up SIGHUP handler with `sigaction()`
    - [x] Ignore SIGPIPE: `sigaction(SIGPIPE, SIG_IGN)`
    - [x] Set `SA_RESTART` flag for all handlers
  - [x] Implement `jshell_reset_signals_for_child()`:
    - [x] Reset SIGINT to SIG_DFL
    - [x] Reset SIGTERM to SIG_DFL
    - [x] Reset SIGHUP to SIG_DFL
    - [x] Reset SIGPIPE to SIG_DFL
    - [x] Reset SIGCHLD to SIG_DFL
    - [x] Reset SIGTSTP to SIG_DFL
  - [x] Implement `jshell_block_signals()`:
    - [x] Block SIGINT, SIGCHLD during critical sections
  - [x] Implement `jshell_unblock_signals()`:
    - [x] Restore previous signal mask

### 1.2 Update Makefile ✅
**File**: `Makefile`

- [x] Add `jshell_signals.c` to `JSHELL_SRCS`
- [x] Ensure proper dependencies

---

## Phase 2: Shell Signal Handlers ✅ COMPLETED

### 2.1 Update Shell Initialization ✅
**File**: `src/jshell/jshell.c`

- [x] Add `#include "jshell_signals.h"`
- [x] Call `jshell_init_signals()` early in `jshell_exec_string()` and `jshell_interactive()`
- [x] In `jshell_interactive()`:
  - [x] Check `jshell_interrupted` after `fgets()` returns
  - [x] Clear `jshell_interrupted` flag after checking
  - [x] Print newline if interrupted to reset prompt position
- [x] Add graceful shutdown:
  - [x] Check `jshell_should_terminate()` and `jshell_should_hangup()` in main loop
  - [x] Break out of loop on termination signal
  - Note: History save deferred (no persistent history yet)

### 2.2 Create Shell Cleanup Function
**File**: `src/jshell/jshell.c`

- [x] N/A - Deferred until persistent history is implemented
  - Note: Currently history is in-memory only, no `jshell_history_save()` exists

### 2.3 Update Interactive Loop for SIGINT ✅
**File**: `src/jshell/jshell.c`

- [x] Modify `jshell_interactive()`:
  - [x] Handle SIGINT during command input:
    ```c
    if (fgets(line, sizeof(line), stdin) == NULL) {
      if (jshell_check_interrupted()) {
        printf("\n");  // Move to new line
        full_line[0] = '\0';
        clearerr(stdin);  // Clear EOF flag
        continue;  // Re-prompt
      }
      break;  // EOF
    }
    ```
  - [x] Clear interrupted flag before prompting
  - [x] Skip empty lines
  - [x] Flush stdout after printing prompt

---

## Phase 3: Child Process Signal Reset ✅ COMPLETED

### 3.1 Update Fork/Exec in AST Helpers ✅
**File**: `src/ast/jshell_ast_helpers.c`

- [x] Add `#include "jshell/jshell_signals.h"`
- [x] In `jshell_fork_and_exec()`:
  - [x] Call `jshell_reset_signals_for_child()` immediately after `fork()` in child
  - [x] This ensures children have default signal disposition
- [x] In `jshell_exec_single_cmd()`:
  - [x] Same: call `jshell_reset_signals_for_child()` after fork in child
- [x] In `jshell_capture_and_tee_output()`:
  - [x] Reset signals in the tee child process

### 3.2 Update Thread Execution
**File**: `src/jshell/jshell_thread_exec.c`

- [x] N/A - Threads share signal handlers with main process
  - Note: SIGINT is handled by main thread only
  - Worker threads don't need special signal handling

---

## Phase 4: External App Signal Handling ✅ COMPLETED

External apps should handle SIGINT gracefully where appropriate. Apps that perform long-running operations or have loops should check for interruption.

### 4.1 Create Signal Utility for Apps ✅
**Files**: `src/utils/jbox_signals.h`, `src/utils/jbox_signals.c`

- [x] Create utility header for apps:
  ```c
  #ifndef JBOX_SIGNALS_H
  #define JBOX_SIGNALS_H

  #include <signal.h>
  #include <stdbool.h>

  extern volatile sig_atomic_t jbox_interrupted;

  void jbox_setup_sigint_handler(void);
  bool jbox_check_interrupted(void);
  bool jbox_is_interrupted(void);
  void jbox_clear_interrupted(void);

  #endif
  ```

- [x] Create `src/utils/jbox_signals.c`:
  - [x] Implement simple SIGINT handler for apps
  - [x] Allow apps to check and respond to interruption

### 4.2 Update File Operations Apps ✅
These apps read/write files and could hang on large files:

**cat** (`src/apps/cat/cmd_cat.c`):
- [x] Check `jbox_interrupted` in main file loop
- [x] Exit gracefully with code 130 on interrupt

**head** (`src/apps/head/cmd_head.c`):
- [x] Check `jbox_interrupted` in getline loop
- [x] Exit gracefully with code 130 on interrupt

**tail** (`src/apps/tail/cmd_tail.c`):
- [x] Check `jbox_interrupted` in read_all_lines loop
- [x] Exit gracefully with code 130 on interrupt

**cp** (`src/apps/cp/cmd_cp.c`):
- [x] Check `jbox_interrupted` during recursive copy
- [x] Check during large file copy loop (fread)
- [x] Propagate interruption through nested functions
- [x] Exit gracefully with code 130 on interrupt

**rg** (`src/apps/rg/cmd_rg.c`):
- [x] Check `jbox_interrupted` between files
- [x] Check during file read (getline loop)
- [x] Check during stdin search
- [x] Exit gracefully with code 130 on interrupt

### 4.3 Update Long-Running Apps

**sleep** (`src/apps/sleep/cmd_sleep.c`):
- [x] Already handles EINTR correctly via `nanosleep()` loop
- [x] N/A - Consider exiting on SIGINT instead of resuming sleep (deferred)

**ls** (`src/apps/ls/cmd_ls.c`):
- [x] N/A - Check interrupt between directory entries (deferred - ls is typically fast)

### 4.4 Update Makefile for Signal Utilities ✅
**File**: `Makefile`

- [x] Add `jbox_signals.c` to JSHELL_SRCS in main Makefile
- [x] Update individual app Makefiles to link SIGNALS_SRC:
  - [x] cat/Makefile
  - [x] head/Makefile
  - [x] tail/Makefile
  - [x] cp/Makefile
  - [x] rg/Makefile

---

## Phase 5: Interactive App Signals

### 5.1 Update vi Editor
**File**: `src/apps/vi/cmd_vi.c`

- [x] Already has SIGWINCH handler for resize
- [ ] Add SIGINT handling:
  - [ ] In command mode: cancel current operation, return to normal mode
  - [ ] In insert mode: exit insert mode
  - [ ] Never exit vi on SIGINT (standard vi behavior)
- [ ] Add SIGTERM handling:
  - [ ] Save file to backup location if modified
  - [ ] Exit gracefully
- [ ] Add SIGTSTP handling:
  - [ ] Restore terminal settings
  - [ ] Send SIGSTOP to self
  - [ ] On resume (SIGCONT): restore raw mode, redraw screen

### 5.2 Update less Pager
**File**: `src/apps/less/cmd_less.c`

- [x] Already has SIGWINCH handler for resize
- [ ] Add SIGINT handling:
  - [ ] Cancel current search if in progress
  - [ ] Return to normal viewing mode
- [ ] Add SIGTSTP handling:
  - [ ] Restore terminal settings
  - [ ] Send SIGSTOP to self
  - [ ] On SIGCONT: restore raw mode, redraw screen

---

## Phase 6: Builtin Command Signals

Builtins run in the shell process or in threads, requiring careful signal handling.

### 6.1 Network Builtins
**Files**: `src/jshell/builtins/cmd_http_get.c`, `src/jshell/builtins/cmd_http_post.c`

- [ ] Use libcurl's progress callback to check for interruption:
  ```c
  static int progress_callback(void *clientp, ...) {
    if (jshell_interrupted) {
      return 1;  // Abort transfer
    }
    return 0;
  }
  ```
- [ ] Set `CURLOPT_PROGRESSFUNCTION` and `CURLOPT_NOPROGRESS` to 0
- [ ] Handle `CURLE_ABORTED_BY_CALLBACK` return code

### 6.2 Wait Builtin
**File**: `src/jshell/builtins/cmd_wait.c`

- [ ] Handle SIGINT during `waitpid()`:
  - [ ] `waitpid()` will return -1 with `errno == EINTR` on signal
  - [ ] Check `jshell_interrupted` and exit wait if set
  - [ ] Print message about interrupted wait

### 6.3 Edit Commands
**Files**: `src/jshell/builtins/cmd_edit_*.c`

- [ ] Edit commands are typically fast, but for large files:
  - [ ] Check `jshell_interrupted` during file read
  - [ ] Check during search/replace operations in `edit-replace`

---

## Phase 7: Test Suite

### 7.1 Create Signal Test Infrastructure
**File**: `tests/helpers/signals.py`

- [ ] Create helper class for signal testing:
  ```python
  class SignalTestHelper:
      @staticmethod
      def send_signal_after_delay(pid, signal, delay_ms):
          """Send signal to process after delay."""

      @staticmethod
      def run_with_signal(cmd, signal, delay_ms, timeout=5):
          """Run command and send signal after delay."""
  ```

### 7.2 Shell Signal Tests
**File**: `tests/jshell/test_signals.py`

- [ ] Test SIGINT during interactive input:
  - [ ] Send SIGINT to shell
  - [ ] Verify shell continues running
  - [ ] Verify prompt is re-displayed

- [ ] Test SIGINT during command execution:
  - [ ] Run `sleep 10` in foreground
  - [ ] Send SIGINT
  - [ ] Verify sleep terminates
  - [ ] Verify shell continues

- [ ] Test SIGTERM graceful shutdown:
  - [ ] Start interactive shell
  - [ ] Send SIGTERM
  - [ ] Verify clean exit

- [ ] Test SIGPIPE handling:
  - [ ] Run `echo test | head -1`
  - [ ] Verify no crash from SIGPIPE

- [ ] Test background job signals:
  - [ ] Start background job
  - [ ] Send SIGINT to shell
  - [ ] Verify background job unaffected

### 7.3 External App Signal Tests
**File**: `tests/apps/test_signals.py`

- [ ] Test cat with large file and SIGINT:
  ```python
  def test_cat_sigint(self):
      # Create large file
      # Start cat reading it
      # Send SIGINT
      # Verify clean exit with code 130 (128 + SIGINT)
  ```

- [ ] Test cp with SIGINT during copy:
  - [ ] Verify partial copy is handled
  - [ ] Verify no corruption

- [ ] Test rg with SIGINT:
  - [ ] Search large directory
  - [ ] Send SIGINT
  - [ ] Verify clean exit

- [ ] Test sleep with SIGINT:
  - [ ] Run `sleep 10`
  - [ ] Send SIGINT
  - [ ] Verify exits promptly with code 130

### 7.4 Interactive App Signal Tests
**File**: `tests/apps/vi/test_vi_signals.py`

- [ ] Test vi SIGINT in command mode:
  - [ ] Open file
  - [ ] Send SIGINT
  - [ ] Verify vi still running
  - [ ] Verify no exit

- [ ] Test vi SIGTSTP:
  - [ ] Open file
  - [ ] Send SIGTSTP
  - [ ] Verify terminal restored
  - [ ] Send SIGCONT
  - [ ] Verify vi resumes

**File**: `tests/apps/less/test_less_signals.py`

- [ ] Test less SIGINT:
  - [ ] Open file
  - [ ] Send SIGINT
  - [ ] Verify less still running

- [ ] Test less SIGTSTP:
  - [ ] Open file
  - [ ] Send SIGTSTP
  - [ ] Verify terminal restored
  - [ ] Send SIGCONT
  - [ ] Verify less resumes

### 7.5 Update tests/Makefile
**File**: `tests/Makefile`

- [ ] Add `signals` test target
- [ ] Add to `all` target
- [ ] Add individual app signal test targets

---

## Implementation Order

### Sprint 1: Core Infrastructure
1. Phase 1.1: Create signal utility module
2. Phase 1.2: Update Makefile
3. Phase 2.1: Update shell initialization
4. Phase 2.2: Create cleanup function
5. Phase 2.3: Update interactive loop

### Sprint 2: Child Process Handling
1. Phase 3.1: Update fork/exec signal reset
2. Phase 3.2: Update thread execution
3. Phase 7.2: Create shell signal tests (basic)

### Sprint 3: External Apps
1. Phase 4.1: Create app signal utility
2. Phase 4.2: Update file operation apps
3. Phase 4.3: Update long-running apps
4. Phase 7.3: External app signal tests

### Sprint 4: Interactive Apps and Builtins
1. Phase 5.1: Update vi editor
2. Phase 5.2: Update less pager
3. Phase 6.1: Update network builtins
4. Phase 6.2: Update wait builtin
5. Phase 7.4: Interactive app signal tests

### Sprint 5: Polish and Complete Testing
1. Phase 6.3: Update edit commands
2. Phase 7.1: Complete test infrastructure
3. Phase 7.5: Update test Makefile
4. Full integration testing

---

## Signal Reference

### Standard Signal Numbers (Linux)

| Signal | Number | Default Action | Description |
|--------|--------|----------------|-------------|
| SIGHUP | 1 | Terminate | Hangup detected |
| SIGINT | 2 | Terminate | Interrupt (Ctrl+C) |
| SIGQUIT | 3 | Core dump | Quit (Ctrl+\\) |
| SIGPIPE | 13 | Terminate | Broken pipe |
| SIGTERM | 15 | Terminate | Termination signal |
| SIGCHLD | 17 | Ignore | Child status change |
| SIGCONT | 18 | Continue | Continue if stopped |
| SIGSTOP | 19 | Stop | Stop process |
| SIGTSTP | 20 | Stop | Stop (Ctrl+Z) |
| SIGWINCH | 28 | Ignore | Window size change |

### Exit Codes for Signals

When a process is terminated by a signal, the exit code is `128 + signal_number`:

| Signal | Exit Code |
|--------|-----------|
| SIGHUP | 129 |
| SIGINT | 130 |
| SIGPIPE | 141 |
| SIGTERM | 143 |

### Signal Handler Best Practices

1. **Keep handlers simple**: Only set flags, no complex logic
2. **Use `volatile sig_atomic_t`**: For flags modified by handlers
3. **Use `sigaction()` over `signal()`**: More portable and predictable
4. **Set `SA_RESTART`**: To automatically restart interrupted syscalls
5. **Reset in children**: Call `jshell_reset_signals_for_child()` after fork
6. **Block during critical sections**: Use `sigprocmask()` to prevent races

---

## Notes

### Thread Safety

- Signal handlers are process-wide, not per-thread
- Use `pthread_sigmask()` to block signals in worker threads
- Only main thread should handle SIGINT for interactive use
- Avoid calling async-signal-unsafe functions in handlers

### Terminal Signals

- SIGTSTP requires saving/restoring terminal state
- Must handle SIGCONT to redraw screen after resume
- Interactive apps should ignore SIGINT in most cases

### SIGPIPE Handling

- Default action terminates process (bad for shells)
- Should be ignored (SIG_IGN) at shell level
- Apps should handle `EPIPE` error from `write()` instead

### Testing Challenges

- Signal timing is inherently racy
- Use delays and retries in tests
- Test with both fast and slow operations
- Verify cleanup occurs (no resource leaks)

---

## Dependencies

### Required Headers

- `<signal.h>` - Signal handling functions
- `<unistd.h>` - pause(), getpid()
- `<sys/types.h>` - pid_t
- `<errno.h>` - EINTR handling

### Required Functions

- `sigaction()` - Install signal handlers
- `sigprocmask()` - Block/unblock signals
- `sigemptyset()` / `sigfillset()` - Initialize signal sets
- `pthread_sigmask()` - Thread-specific signal blocking

---

## Checklist Summary

- [x] Phase 1: Core Signal Infrastructure (2 tasks)
- [x] Phase 2: Shell Signal Handlers (3 tasks)
- [x] Phase 3: Child Process Signal Reset (2 tasks)
- [x] Phase 4: External App Signal Handling (4 tasks)
- [ ] Phase 5: Interactive App Signals (2 tasks)
- [ ] Phase 6: Builtin Command Signals (3 tasks)
- [ ] Phase 7: Test Suite (5 tasks)

**Total: 21 major tasks (13 completed)**
