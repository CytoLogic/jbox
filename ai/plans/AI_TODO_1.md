# CLI Tools and Shell Builtins Implementation Plan

## Overview
Implement CLI tools and shell builtins conforming to `ai/CLItools.md` and `ai/APPANATOMY.md`.

**Directory Structure:**
- Builtins: `src/jshell/builtins/`
- Standalone apps: `src/apps/<command_name>/`

**Standards:**
- All commands use `argtable3` for CLI parsing
- All commands follow `cmd_spec_t` anatomy
- Agent-facing commands support `--json` output
- All commands support `-h` / `--help`

---

## Phase 1: Core Infrastructure

### 1.1 Update Command Registry
- [ ] Update `src/jshell/jshell_cmd_registry.h`:
  - [ ] Add `typedef enum { CMD_BUILTIN, CMD_EXTERNAL } jshell_cmd_type_t;`
  - [ ] Add `jshell_cmd_type_t type;` field to `jshell_cmd_spec_t`
- [ ] Update existing command specs to include `.type = CMD_BUILTIN`

### 1.2 Create Builtin Registration System
- [ ] Create `src/jshell/jshell_register_builtins.h`:
  - [ ] Declare `void jshell_register_all_builtin_commands(void);`
  - [ ] Declare individual registration functions (e.g., `void jshell_register_ls_command(void);`)
- [ ] Create `src/jshell/jshell_register_builtins.c`:
  - [ ] Implement `jshell_register_all_builtin_commands()` that calls all individual registration functions
- [ ] Update `src/jshell/jshell.c`:
  - [ ] Call `jshell_register_all_builtin_commands()` in `jshell_main()` initialization

### 1.3 Update AST Helpers for Builtin Dispatch
- [ ] Update `src/ast/jshell_ast_helpers.c`:
  - [ ] Ensure `jshell_find_builtin()` uses `jshell_find_command()`
  - [ ] Ensure `jshell_exec_builtin()` respects `spec->type`

---

## Phase 2: Filesystem Tools (Agent-Facing, HIGH PRIORITY)

### 2.1 ls - List Directory Contents
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_ls.h`:
  - [ ] Declare `extern jshell_cmd_spec_t cmd_ls_spec;`
  - [ ] Declare `void jshell_register_ls_command(void);`
- [ ] Create `src/jshell/builtins/cmd_ls.c`:
  - [ ] Implement `build_ls_argtable()` with:
    - [ ] `-h, --help` (help)
    - [ ] `-a` (include hidden files)
    - [ ] `-l` (long format)
    - [ ] `--json` (JSON output)
    - [ ] `[PATH...]` (positional file/directory arguments)
  - [ ] Implement `ls_run(int argc, char **argv)`:
    - [ ] Parse args with `argtable3`
    - [ ] Handle `--help`
    - [ ] Handle errors
    - [ ] Implement directory listing logic
    - [ ] Support `-a` flag
    - [ ] Support `-l` flag (permissions, owner, size, timestamps)
    - [ ] Support `--json` output format
    - [ ] Default to current directory if no paths given
  - [ ] Implement `ls_print_usage(FILE *out)` using `argtable3`
  - [ ] Define `cmd_ls_spec` with name, summary, long_help, type, run, print_usage
  - [ ] Implement `jshell_register_ls_command()`
- [ ] Create `src/apps/ls/`:
  - [ ] Create `src/apps/ls/ls_main.c`:
    - [ ] Implement `main()` that calls `cmd_ls_spec.run(argc, argv)`
  - [ ] Update Makefile to build standalone `ls` binary

### 2.2 stat - File Metadata
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_stat.h`
- [ ] Create `src/jshell/builtins/cmd_stat.c`:
  - [ ] Implement `build_stat_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
    - [ ] `<FILE>` (required positional argument)
  - [ ] Implement `stat_run()`:
    - [ ] Use `stat()` system call
    - [ ] Display: path, type, size, mode, mtime, atime, ctime
    - [ ] Support `--json` output
  - [ ] Implement `stat_print_usage()`
  - [ ] Define `cmd_stat_spec`
  - [ ] Implement `jshell_register_stat_command()`
- [ ] Create `src/apps/stat/stat_main.c`
- [ ] Update Makefile

### 2.3 cat - Print File Contents
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_cat.h`
- [ ] Create `src/jshell/builtins/cmd_cat.c`:
  - [ ] Implement `build_cat_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json` (optional, wraps output)
    - [ ] `<FILE...>` (one or more files)
  - [ ] Implement `cat_run()`:
    - [ ] Read and print file contents
    - [ ] Handle multiple files
    - [ ] Support `--json` output (wrap as `{"path": "...", "content": "..."}`)
  - [ ] Implement `cat_print_usage()`
  - [ ] Define `cmd_cat_spec`
  - [ ] Implement `jshell_register_cat_command()`
- [ ] Create `src/apps/cat/cat_main.c`
- [ ] Update Makefile

### 2.4 head - View Start of File
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_head.h`
- [ ] Create `src/jshell/builtins/cmd_head.c`:
  - [ ] Implement `build_head_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-n N` (number of lines, default 10)
    - [ ] `--json`
    - [ ] `<FILE>`
  - [ ] Implement `head_run()`:
    - [ ] Read first N lines
    - [ ] Support `--json` output: `{"path": "...", "lines": [...]}`
  - [ ] Implement `head_print_usage()`
  - [ ] Define `cmd_head_spec`
  - [ ] Implement `jshell_register_head_command()`
- [ ] Create `src/apps/head/head_main.c`
- [ ] Update Makefile

### 2.5 tail - View End of File
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_tail.h`
- [ ] Create `src/jshell/builtins/cmd_tail.c`:
  - [ ] Implement `build_tail_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-n N` (number of lines, default 10)
    - [ ] `--json`
    - [ ] `<FILE>`
  - [ ] Implement `tail_run()`:
    - [ ] Read last N lines
    - [ ] Support `--json` output
  - [ ] Implement `tail_print_usage()`
  - [ ] Define `cmd_tail_spec`
  - [ ] Implement `jshell_register_tail_command()`
- [ ] Create `src/apps/tail/tail_main.c`
- [ ] Update Makefile

### 2.6 cp - Copy Files/Directories
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_cp.h`
- [ ] Create `src/jshell/builtins/cmd_cp.c`:
  - [ ] Implement `build_cp_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-r` (recursive)
    - [ ] `-f` (force overwrite)
    - [ ] `--json`
    - [ ] `<SOURCE>` (required)
    - [ ] `<DEST>` (required)
  - [ ] Implement `cp_run()`:
    - [ ] Copy file logic
    - [ ] Recursive directory copy with `-r`
    - [ ] Force overwrite with `-f`
    - [ ] Support `--json` output (success/error summary)
  - [ ] Implement `cp_print_usage()`
  - [ ] Define `cmd_cp_spec`
  - [ ] Implement `jshell_register_cp_command()`
- [ ] Create `src/apps/cp/cp_main.c`
- [ ] Update Makefile

### 2.7 mv - Move/Rename Files
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_mv.h`
- [ ] Create `src/jshell/builtins/cmd_mv.c`:
  - [ ] Implement `build_mv_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-f` (force)
    - [ ] `--json`
    - [ ] `<SOURCE>`
    - [ ] `<DEST>`
  - [ ] Implement `mv_run()`:
    - [ ] Use `rename()` system call
    - [ ] Handle cross-filesystem moves
    - [ ] Support `--json` output
  - [ ] Implement `mv_print_usage()`
  - [ ] Define `cmd_mv_spec`
  - [ ] Implement `jshell_register_mv_command()`
- [ ] Create `src/apps/mv/mv_main.c`
- [ ] Update Makefile

### 2.8 rm - Remove Files/Directories
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_rm.h`
- [ ] Create `src/jshell/builtins/cmd_rm.c`:
  - [ ] Implement `build_rm_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-r` (recursive)
    - [ ] `-f` (force, no prompt)
    - [ ] `--json`
    - [ ] `<FILE...>` (one or more files)
  - [ ] Implement `rm_run()`:
    - [ ] Remove files with `unlink()`
    - [ ] Recursive directory removal with `-r`
    - [ ] Support `--json` output
  - [ ] Implement `rm_print_usage()`
  - [ ] Define `cmd_rm_spec`
  - [ ] Implement `jshell_register_rm_command()`
- [ ] Create `src/apps/rm/rm_main.c`
- [ ] Update Makefile

### 2.9 mkdir - Create Directories
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_mkdir.h`
- [ ] Create `src/jshell/builtins/cmd_mkdir.c`:
  - [ ] Implement `build_mkdir_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-p` (create parents)
    - [ ] `--json`
    - [ ] `<DIR...>` (one or more directories)
  - [ ] Implement `mkdir_run()`:
    - [ ] Use `mkdir()` system call
    - [ ] Create parent directories with `-p`
    - [ ] Support `--json` output
  - [ ] Implement `mkdir_print_usage()`
  - [ ] Define `cmd_mkdir_spec`
  - [ ] Implement `jshell_register_mkdir_command()`
- [ ] Create `src/apps/mkdir/mkdir_main.c`
- [ ] Update Makefile

### 2.10 rmdir - Remove Empty Directories
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_rmdir.h`
- [ ] Create `src/jshell/builtins/cmd_rmdir.c`:
  - [ ] Implement `build_rmdir_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
    - [ ] `<DIR...>`
  - [ ] Implement `rmdir_run()`:
    - [ ] Use `rmdir()` system call
    - [ ] Only remove empty directories
    - [ ] Support `--json` output
  - [ ] Implement `rmdir_print_usage()`
  - [ ] Define `cmd_rmdir_spec`
  - [ ] Implement `jshell_register_rmdir_command()`
- [ ] Create `src/apps/rmdir/rmdir_main.c`
- [ ] Update Makefile

### 2.11 touch - Create Empty File or Update Timestamps
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_touch.h`
- [ ] Create `src/jshell/builtins/cmd_touch.c`:
  - [ ] Implement `build_touch_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
    - [ ] `<FILE...>`
  - [ ] Implement `touch_run()`:
    - [ ] Create file if it doesn't exist
    - [ ] Update timestamps with `utime()` if it exists
    - [ ] Support `--json` output
  - [ ] Implement `touch_print_usage()`
  - [ ] Define `cmd_touch_spec`
  - [ ] Implement `jshell_register_touch_command()`
- [ ] Create `src/apps/touch/touch_main.c`
- [ ] Update Makefile

---

## Phase 3: Search and Text Tools (Agent-Facing, HIGH PRIORITY)

### 3.1 rg - Regex Search
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_rg.h`
- [ ] Create `src/jshell/builtins/cmd_rg.c`:
  - [ ] Implement `build_rg_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-n` (show line numbers)
    - [ ] `-i` (case-insensitive)
    - [ ] `-w` (match whole words)
    - [ ] `-C N` (context lines)
    - [ ] `--fixed-strings` (literal, not regex)
    - [ ] `--json` (emit JSON per match)
    - [ ] `<PATTERN>` (required)
    - [ ] `<FILE...>` (files to search)
  - [ ] Implement `rg_run()`:
    - [ ] Use POSIX regex (`regcomp`, `regexec`)
    - [ ] Search files for pattern
    - [ ] Support all flags
    - [ ] Support `--json` output: `{"file": "...", "line": N, "column": N, "text": "..."}`
  - [ ] Implement `rg_print_usage()`
  - [ ] Define `cmd_rg_spec`
  - [ ] Implement `jshell_register_rg_command()`
- [ ] Create `src/apps/rg/rg_main.c`
- [ ] Update Makefile

---

## Phase 4: Structured Editing Commands (Agent-Facing, HIGH PRIORITY)

### 4.1 edit-replace-line - Replace Single Line
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_edit_replace_line.h`
- [ ] Create `src/jshell/builtins/cmd_edit_replace_line.c`:
  - [ ] Implement `build_edit_replace_line_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
    - [ ] `<FILE>` (required)
    - [ ] `<LINE_NUM>` (required, integer)
    - [ ] `<TEXT>` (required, replacement text)
  - [ ] Implement `edit_replace_line_run()`:
    - [ ] Read entire file into memory
    - [ ] Replace line at LINE_NUM with TEXT
    - [ ] Write file back
    - [ ] Support `--json` output: `{"path": "...", "line": N, "status": "ok"|"error", "message": "..."}`
  - [ ] Implement `edit_replace_line_print_usage()`
  - [ ] Define `cmd_edit_replace_line_spec`
  - [ ] Implement `jshell_register_edit_replace_line_command()`
- [ ] Create `src/apps/edit-replace-line/edit_replace_line_main.c`
- [ ] Update Makefile

### 4.2 edit-insert-line - Insert Line Before Given Line
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_edit_insert_line.h`
- [ ] Create `src/jshell/builtins/cmd_edit_insert_line.c`:
  - [ ] Implement `build_edit_insert_line_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
    - [ ] `<FILE>`
    - [ ] `<LINE_NUM>`
    - [ ] `<TEXT>`
  - [ ] Implement `edit_insert_line_run()`:
    - [ ] Read file
    - [ ] Insert TEXT before line LINE_NUM
    - [ ] Write file back
    - [ ] Support `--json` output
  - [ ] Implement `edit_insert_line_print_usage()`
  - [ ] Define `cmd_edit_insert_line_spec`
  - [ ] Implement `jshell_register_edit_insert_line_command()`
- [ ] Create `src/apps/edit-insert-line/edit_insert_line_main.c`
- [ ] Update Makefile

### 4.3 edit-delete-line - Delete Single Line
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_edit_delete_line.h`
- [ ] Create `src/jshell/builtins/cmd_edit_delete_line.c`:
  - [ ] Implement `build_edit_delete_line_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
    - [ ] `<FILE>`
    - [ ] `<LINE_NUM>`
  - [ ] Implement `edit_delete_line_run()`:
    - [ ] Read file
    - [ ] Delete line at LINE_NUM
    - [ ] Write file back
    - [ ] Support `--json` output
  - [ ] Implement `edit_delete_line_print_usage()`
  - [ ] Define `cmd_edit_delete_line_spec`
  - [ ] Implement `jshell_register_edit_delete_line_command()`
- [ ] Create `src/apps/edit-delete-line/edit_delete_line_main.c`
- [ ] Update Makefile

### 4.4 edit-replace - Global Find/Replace with Regex
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_edit_replace.h`
- [ ] Create `src/jshell/builtins/cmd_edit_replace.c`:
  - [ ] Implement `build_edit_replace_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-i` (case-insensitive)
    - [ ] `--fixed-strings` (literal, not regex)
    - [ ] `--json`
    - [ ] `<FILE>`
    - [ ] `<PATTERN>` (regex or literal)
    - [ ] `<REPLACEMENT>`
  - [ ] Implement `edit_replace_run()`:
    - [ ] Read file
    - [ ] Use POSIX regex to find/replace all matches
    - [ ] Write file back
    - [ ] Support `--json` output: `{"path": "...", "matches": M, "replacements": R}`
  - [ ] Implement `edit_replace_print_usage()`
  - [ ] Define `cmd_edit_replace_spec`
  - [ ] Implement `jshell_register_edit_replace_command()`
- [ ] Create `src/apps/edit-replace/edit_replace_main.c`
- [ ] Update Makefile

---

## Phase 5: Process and Job Control (MEDIUM PRIORITY)

### 5.1 jobs - List Background Jobs (Refactor Existing)
**Builtin Only**

- [ ] Refactor `src/jshell/builtins/jobs.c`:
  - [ ] Rename to `cmd_jobs.c`
  - [ ] Implement `build_jobs_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
  - [ ] Refactor `jobs_run()` to use `argtable3`
  - [ ] Add `--json` output support
  - [ ] Implement `jobs_print_usage()` using `argtable3`
  - [ ] Define `cmd_jobs_spec`
  - [ ] Rename `jshell_register_jobs_cmd()` to `jshell_register_jobs_command()`
- [ ] Update `src/jshell/builtins/cmd_jobs.h` (create if needed)

### 5.2 ps - List Processes/Threads
**Builtin Only**

- [ ] Create `src/jshell/builtins/cmd_ps.h`
- [ ] Create `src/jshell/builtins/cmd_ps.c`:
  - [ ] Implement `build_ps_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
  - [ ] Implement `ps_run()`:
    - [ ] List processes/threads known to shell
    - [ ] Support `--json` output
  - [ ] Implement `ps_print_usage()`
  - [ ] Define `cmd_ps_spec`
  - [ ] Implement `jshell_register_ps_command()`

### 5.3 kill - Send Signal to Job/Process
**Builtin Only**

- [ ] Create `src/jshell/builtins/cmd_kill.h`
- [ ] Create `src/jshell/builtins/cmd_kill.c`:
  - [ ] Implement `build_kill_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-s SIGNAL` (signal name or number)
    - [ ] `--json`
    - [ ] `<PID>` (required)
  - [ ] Implement `kill_run()`:
    - [ ] Use `kill()` system call
    - [ ] Support signal names (TERM, KILL, etc.)
    - [ ] Support `--json` output
  - [ ] Implement `kill_print_usage()`
  - [ ] Define `cmd_kill_spec`
  - [ ] Implement `jshell_register_kill_command()`

### 5.4 wait - Wait for Job to Finish
**Builtin Only**

- [ ] Create `src/jshell/builtins/cmd_wait.h`
- [ ] Create `src/jshell/builtins/cmd_wait.c`:
  - [ ] Implement `build_wait_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
    - [ ] `<JOB_ID>` (optional)
  - [ ] Implement `wait_run()`:
    - [ ] Wait for specific job or all jobs
    - [ ] Support `--json` output: `{"job": ID, "status": "exited", "code": N}`
  - [ ] Implement `wait_print_usage()`
  - [ ] Define `cmd_wait_spec`
  - [ ] Implement `jshell_register_wait_command()`

---

## Phase 6: Shell and Environment (MEDIUM PRIORITY)

### 6.1 cd - Change Directory
**Builtin Only**

- [ ] Create `src/jshell/builtins/cmd_cd.h`
- [ ] Create `src/jshell/builtins/cmd_cd.c`:
  - [ ] Implement `build_cd_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `[DIR]` (optional, defaults to HOME)
  - [ ] Implement `cd_run()`:
    - [ ] Use `chdir()` system call
    - [ ] Handle `~` expansion
    - [ ] Update PWD environment variable
  - [ ] Implement `cd_print_usage()`
  - [ ] Define `cmd_cd_spec`
  - [ ] Implement `jshell_register_cd_command()`

### 6.2 pwd - Print Working Directory
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_pwd.h`
- [ ] Create `src/jshell/builtins/cmd_pwd.c`:
  - [ ] Implement `build_pwd_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
  - [ ] Implement `pwd_run()`:
    - [ ] Use `getcwd()`
    - [ ] Support `--json` output: `{"cwd": "/path"}`
  - [ ] Implement `pwd_print_usage()`
  - [ ] Define `cmd_pwd_spec`
  - [ ] Implement `jshell_register_pwd_command()`
- [ ] Create `src/apps/pwd/pwd_main.c`
- [ ] Update Makefile

### 6.3 env - List Environment Variables
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_env.h`
- [ ] Create `src/jshell/builtins/cmd_env.c`:
  - [ ] Implement `build_env_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
  - [ ] Implement `env_run()`:
    - [ ] List all environment variables
    - [ ] Support `--json` output: `{"env": {"KEY": "VALUE", ...}}`
  - [ ] Implement `env_print_usage()`
  - [ ] Define `cmd_env_spec`
  - [ ] Implement `jshell_register_env_command()`
- [ ] Create `src/apps/env/env_main.c`
- [ ] Update Makefile

### 6.4 export - Set Environment Variable
**Builtin Only**

- [ ] Create `src/jshell/builtins/cmd_export.h`
- [ ] Create `src/jshell/builtins/cmd_export.c`:
  - [ ] Implement `build_export_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
    - [ ] `<KEY=VALUE>` (one or more)
  - [ ] Implement `export_run()`:
    - [ ] Parse KEY=VALUE
    - [ ] Use `setenv()` or update `environ`
    - [ ] Support `--json` output
  - [ ] Implement `export_print_usage()`
  - [ ] Define `cmd_export_spec`
  - [ ] Implement `jshell_register_export_command()`

### 6.5 unset - Unset Environment Variable
**Builtin Only**

- [ ] Create `src/jshell/builtins/cmd_unset.h`
- [ ] Create `src/jshell/builtins/cmd_unset.c`:
  - [ ] Implement `build_unset_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
    - [ ] `<KEY>` (one or more)
  - [ ] Implement `unset_run()`:
    - [ ] Use `unsetenv()`
    - [ ] Support `--json` output
  - [ ] Implement `unset_print_usage()`
  - [ ] Define `cmd_unset_spec`
  - [ ] Implement `jshell_register_unset_command()`

### 6.6 type - Show How Name is Resolved
**Builtin Only**

- [ ] Create `src/jshell/builtins/cmd_type.h`
- [ ] Create `src/jshell/builtins/cmd_type.c`:
  - [ ] Implement `build_type_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `--json`
    - [ ] `<NAME>` (required)
  - [ ] Implement `type_run()`:
    - [ ] Check if builtin via `jshell_find_command()`
    - [ ] Check if external via PATH search
    - [ ] Check if alias
    - [ ] Support `--json` output: `{"name": "ls", "kind": "builtin"|"external"|"alias", "path": "/bin/ls"}`
  - [ ] Implement `type_print_usage()`
  - [ ] Define `cmd_type_spec`
  - [ ] Implement `jshell_register_type_command()`

---

## Phase 7: Human-Facing Tools (LOW PRIORITY)

### 7.1 echo - Print Text
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_echo.h`
- [ ] Create `src/jshell/builtins/cmd_echo.c`:
  - [ ] Implement `build_echo_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-n` (no trailing newline)
    - [ ] `<TEXT...>` (arguments to print)
  - [ ] Implement `echo_run()`
  - [ ] Implement `echo_print_usage()`
  - [ ] Define `cmd_echo_spec`
  - [ ] Implement `jshell_register_echo_command()`
- [ ] Create `src/apps/echo/echo_main.c`
- [ ] Update Makefile

### 7.2 printf - Formatted Output
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_printf.h`
- [ ] Create `src/jshell/builtins/cmd_printf.c`:
  - [ ] Implement `build_printf_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `<FORMAT>` (required)
    - [ ] `<ARGS...>` (optional)
  - [ ] Implement `printf_run()`:
    - [ ] Parse format string
    - [ ] Apply arguments
    - [ ] Print formatted output
  - [ ] Implement `printf_print_usage()`
  - [ ] Define `cmd_printf_spec`
  - [ ] Implement `jshell_register_printf_command()`
- [ ] Create `src/apps/printf/printf_main.c`
- [ ] Update Makefile

### 7.3 sleep - Delay
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_sleep.h`
- [ ] Create `src/jshell/builtins/cmd_sleep.c`:
  - [ ] Implement `build_sleep_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `<SECONDS>` (required, can be float)
  - [ ] Implement `sleep_run()`:
    - [ ] Use `nanosleep()` or `usleep()`
  - [ ] Implement `sleep_print_usage()`
  - [ ] Define `cmd_sleep_spec`
  - [ ] Implement `jshell_register_sleep_command()`
- [ ] Create `src/apps/sleep/sleep_main.c`
- [ ] Update Makefile

### 7.4 date - Show System Time
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_date.h`
- [ ] Create `src/jshell/builtins/cmd_date.c`:
  - [ ] Implement `build_date_argtable()` with:
    - [ ] `-h, --help`
  - [ ] Implement `date_run()`:
    - [ ] Use `time()` and `strftime()`
    - [ ] Print current date/time
  - [ ] Implement `date_print_usage()`
  - [ ] Define `cmd_date_spec`
  - [ ] Implement `jshell_register_date_command()`
- [ ] Create `src/apps/date/date_main.c`
- [ ] Update Makefile

### 7.5 true - Do Nothing, Succeed
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_true.h`
- [ ] Create `src/jshell/builtins/cmd_true.c`:
  - [ ] Implement `build_true_argtable()` with:
    - [ ] `-h, --help`
  - [ ] Implement `true_run()`:
    - [ ] Return 0
  - [ ] Implement `true_print_usage()`
  - [ ] Define `cmd_true_spec`
  - [ ] Implement `jshell_register_true_command()`
- [ ] Create `src/apps/true/true_main.c`
- [ ] Update Makefile

### 7.6 false - Do Nothing, Fail
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_false.h`
- [ ] Create `src/jshell/builtins/cmd_false.c`:
  - [ ] Implement `build_false_argtable()` with:
    - [ ] `-h, --help`
  - [ ] Implement `false_run()`:
    - [ ] Return 1
  - [ ] Implement `false_print_usage()`
  - [ ] Define `cmd_false_spec`
  - [ ] Implement `jshell_register_false_command()`
- [ ] Create `src/apps/false/false_main.c`
- [ ] Update Makefile

### 7.7 help - Shell Built-in Help
**Builtin Only**

- [ ] Create `src/jshell/builtins/cmd_help.h`
- [ ] Create `src/jshell/builtins/cmd_help.c`:
  - [ ] Implement `build_help_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `[COMMAND]` (optional)
  - [ ] Implement `help_run()`:
    - [ ] If no argument: list all commands via `jshell_for_each_command()`
    - [ ] If argument: show detailed help for that command via `spec->print_usage()`
  - [ ] Implement `help_print_usage()`
  - [ ] Define `cmd_help_spec`
  - [ ] Implement `jshell_register_help_command()`

### 7.8 history - Show Command History
**Builtin Only**

- [ ] Create `src/jshell/builtins/cmd_history.h`
- [ ] Create `src/jshell/builtins/cmd_history.c`:
  - [ ] Implement `build_history_argtable()` with:
    - [ ] `-h, --help`
  - [ ] Implement `history_run()`:
    - [ ] Display command history (if implemented)
  - [ ] Implement `history_print_usage()`
  - [ ] Define `cmd_history_spec`
  - [ ] Implement `jshell_register_history_command()`

### 7.9 alias - Manage Aliases
**Builtin Only**

- [ ] Create `src/jshell/builtins/cmd_alias.h`
- [ ] Create `src/jshell/builtins/cmd_alias.c`:
  - [ ] Implement `build_alias_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `[NAME=VALUE]` (optional)
  - [ ] Implement `alias_run()`:
    - [ ] If no argument: list all aliases
    - [ ] If argument: create/update alias
  - [ ] Implement `alias_print_usage()`
  - [ ] Define `cmd_alias_spec`
  - [ ] Implement `jshell_register_alias_command()`

### 7.10 unalias - Remove Alias
**Builtin Only**

- [ ] Create `src/jshell/builtins/cmd_unalias.h`
- [ ] Create `src/jshell/builtins/cmd_unalias.c`:
  - [ ] Implement `build_unalias_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `<NAME>` (required)
  - [ ] Implement `unalias_run()`:
    - [ ] Remove alias
  - [ ] Implement `unalias_print_usage()`
  - [ ] Define `cmd_unalias_spec`
  - [ ] Implement `jshell_register_unalias_command()`

### 7.11 less - Pager (Optional, Complex)
**Standalone Only**

- [ ] Create `src/apps/less/less.c`:
  - [ ] Implement basic pager functionality
  - [ ] Support `-N` (line numbers)
  - [ ] Support `-h, --help`
- [ ] Update Makefile

### 7.12 vi - Text Editor (Optional, Very Complex)
**Standalone Only**

- [ ] Consider using existing minimal vi implementation or deferring
- [ ] If implementing:
  - [ ] Create `src/apps/vi/`
  - [ ] Implement basic vi-like editing
  - [ ] Support `-h, --help`
- [ ] Update Makefile

---

## Phase 8: Networking (FUTURE)

### 8.1 http-get - Fetch URL
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_http_get.h`
- [ ] Create `src/jshell/builtins/cmd_http_get.c`:
  - [ ] Implement `build_http_get_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-H KEY:VALUE` (headers, repeatable)
    - [ ] `--json`
    - [ ] `<URL>` (required)
  - [ ] Implement `http_get_run()`:
    - [ ] Use sockets or libcurl (if allowed)
    - [ ] Fetch URL
    - [ ] Support `--json` output: `{"status": 200, "headers": {...}, "body": "..."}`
  - [ ] Implement `http_get_print_usage()`
  - [ ] Define `cmd_http_get_spec`
  - [ ] Implement `jshell_register_http_get_command()`
- [ ] Create `src/apps/http-get/http_get_main.c`
- [ ] Update Makefile

### 8.2 http-post - POST to URL
**Builtin + Standalone**

- [ ] Create `src/jshell/builtins/cmd_http_post.h`
- [ ] Create `src/jshell/builtins/cmd_http_post.c`:
  - [ ] Implement `build_http_post_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-H KEY:VALUE` (headers)
    - [ ] `-d DATA` (body data)
    - [ ] `--json`
    - [ ] `<URL>` (required)
  - [ ] Implement `http_post_run()`:
    - [ ] POST data to URL
    - [ ] Support `--json` output
  - [ ] Implement `http_post_print_usage()`
  - [ ] Define `cmd_http_post_spec`
  - [ ] Implement `jshell_register_http_post_command()`
- [ ] Create `src/apps/http-post/http_post_main.c`
- [ ] Update Makefile

---

## Phase 9: Package Manager Integration (FUTURE)

### 9.1 Refactor pkg Command
**Builtin Only**

- [ ] Refactor existing `pkg` implementation:
  - [ ] Move to `src/jshell/builtins/cmd_pkg.c`
  - [ ] Implement `build_pkg_argtable()` with subcommands:
    - [ ] `list --json`
    - [ ] `info NAME --json`
    - [ ] `search NAME --json`
    - [ ] `install NAME`
    - [ ] `remove NAME`
    - [ ] `build`
    - [ ] `check-update`
    - [ ] `upgrade`
    - [ ] `compile`
  - [ ] Implement `pkg_run()` with subcommand dispatch
  - [ ] Implement `pkg_print_usage()`
  - [ ] Define `cmd_pkg_spec`
  - [ ] Implement `jshell_register_pkg_command()`

---

## Phase 10: Testing and Documentation

### 10.1 Unit Tests
- [ ] Create test suite for each command
- [ ] Test `--help` output
- [ ] Test `--json` output format
- [ ] Test error handling
- [ ] Test edge cases

### 10.2 Integration Tests
- [ ] Test commands in shell context
- [ ] Test pipelines with new commands
- [ ] Test redirection with new commands
- [ ] Test background jobs with new commands

### 10.3 Documentation
- [ ] Generate documentation from `--help` output
- [ ] Create man pages or markdown docs for each command
- [ ] Update main README with command list
- [ ] Document JSON output schemas

### 10.4 Makefile Updates
- [ ] Ensure all standalone binaries build correctly
- [ ] Ensure all builtins link into shell
- [ ] Add install targets
- [ ] Add clean targets

---

## Notes

### Command Anatomy Checklist
For each command, ensure:
- [ ] Uses `argtable3` for CLI parsing
- [ ] Implements `build_<cmd>_argtable()` function
- [ ] Implements `<cmd>_run(int argc, char **argv)` function
- [ ] Implements `<cmd>_print_usage(FILE *out)` function
- [ ] Defines `cmd_<cmd>_spec` with all fields
- [ ] Implements `jshell_register_<cmd>_command()` function
- [ ] Supports `-h, --help`
- [ ] Agent-facing commands support `--json`
- [ ] Follows naming conventions (snake_case)
- [ ] Follows code style (2-space indent, 80-char lines)
- [ ] Includes proper error handling
- [ ] Cleans up resources properly

### JSON Output Standards
- [ ] All JSON output is valid, parseable JSON
- [ ] JSON schemas are documented
- [ ] JSON output includes status/error information
- [ ] JSON output is stable (no breaking changes without versioning)

### Regex vs Wildcards
- [ ] Shell expands wildcards before passing to commands
- [ ] Only `rg` and `edit-replace` interpret patterns as regex
- [ ] All other commands treat arguments as literals
- [ ] Document which commands are regex-aware
