# CLI Tools and Shell Builtins Implementation Plan

## Overview

Implement CLI tools and shell builtins conforming to `ai/CLItools.md` and `ai/APPANATOMY.md`.

### Directory Structure

- **Builtins:** `src/jshell/builtins/`
  - Compiled into jbox shell binary
  - Executed directly by `jshell_exec_builtin()` in `jshell_ast_helpers.c`
  - Registered via `jshell_register_<cmd>_command()` in `jshell_register_builtins.c`
  - Required for commands that modify shell state (cd, export, jobs, etc.)

- **External Apps:** `src/apps/<command_name>/`
  - Built as standalone binaries in `bin/`
  - Executed by shell via fork/exec (PATH lookup)
  - Each app has: `cmd_<name>.h`, `cmd_<name>.c`, `<name>_main.c`
  - Registered via `jshell_register_<cmd>_command()` in `jshell_register_externals.c`
  - Registration allows shell to provide help, tab completion, and `type` command support
  - Tests in `tests/apps/<command_name>/test_<name>.py`

### Standards

- All commands use `argtable3` for CLI parsing
- All commands follow `cmd_spec_t` anatomy
- Agent-facing commands support `--json` output
- All commands support `-h` / `--help`
- External apps include Python unittest test suite

### Test Convention

For each external app, create `tests/apps/<cmd>/test_<cmd>.py`:
- Test `-h` and `--help` flags
- Test `--json` output format (if applicable)
- Test normal operation with various inputs
- Test error handling (missing args, invalid files, etc.)
- Test edge cases (empty files, special characters, etc.)

---

## Phase 1: Core Infrastructure ✅ COMPLETED

### 1.1 Update Command Registry ✅ COMPLETED
- [x] Update `src/jshell/jshell_cmd_registry.h`:
  - [x] Add `typedef enum { CMD_BUILTIN, CMD_EXTERNAL } jshell_cmd_type_t;`
  - [x] Add `jshell_cmd_type_t type;` field to `jshell_cmd_spec_t`
- [x] Update existing command specs to include `.type = CMD_BUILTIN`

### 1.2 Create Builtin Registration System ✅ COMPLETED
- [x] Create `src/jshell/jshell_register_builtins.h`:
- [x] Declare `void jshell_register_all_builtin_commands(void);`
- [x] Declare individual registration functions (e.g., `void jshell_register_jobs_command(void);`)
- [x] Create `src/jshell/jshell_register_builtins.c`:
  - [x] Implement `jshell_register_all_builtin_commands()` that calls all individual registration functions
- [x] Update `src/jshell/jshell.c`:
  - [x] Call `jshell_register_all_builtin_commands()` in `jshell_main()` initialization

### 1.3 Update AST Helpers for Builtin Dispatch ✅ COMPLETED
- [x] Update `src/ast/jshell_ast_helpers.c`:
  - [x] Ensure `jshell_find_builtin()` uses `jshell_find_command()`
  - [x] Ensure `jshell_exec_builtin()` respects `spec->type`

### 1.4 Create External Command Registration System ✅ COMPLETED
- [x] Create `src/jshell/jshell_register_externals.h`:
  - [x] Declare `void jshell_register_all_external_commands(void);`
  - [x] Declare individual registration functions (e.g., `void jshell_register_ls_command(void);`)
- [x] Create `src/jshell/jshell_register_externals.c`:
  - [x] Include headers from `src/apps/<cmd>/cmd_<cmd>.h` for each external command
  - [x] Implement `jshell_register_all_external_commands()` that calls all individual registration functions
- [x] Update `src/jshell/jshell.c`:
  - [x] Call `jshell_register_all_external_commands()` in `jshell_main()` initialization (after builtins)
- [x] Update Makefile:
  - [x] Add `jshell_register_externals.c` to `JSHELL_SRCS`
  - [x] Add all `cmd_<name>.c` files from `src/apps/` to jbox build (for registration only)

---

## Phase 2: Filesystem Tools (Agent-Facing, HIGH PRIORITY)

All filesystem tools are **External Apps** - built as standalone binaries and executed via fork/exec.

### 2.1 ls - List Directory Contents ✅ COMPLETED
**External App** (`src/apps/ls/`)

- [x] Create `src/apps/ls/cmd_ls.h`:
  - [x] Declare `extern jshell_cmd_spec_t cmd_ls_spec;`
  - [x] Declare `void jshell_register_ls_command(void);`
- [x] Create `src/apps/ls/cmd_ls.c`:
  - [x] Implement `build_ls_argtable()` with:
    - [x] `-h, --help` (help)
    - [x] `-a` (include hidden files)
    - [x] `-l` (long format)
    - [x] `--json` (JSON output)
    - [x] `[PATH...]` (positional file/directory arguments)
  - [x] Implement `ls_run(int argc, char **argv)`
  - [x] Implement `ls_print_usage(FILE *out)` using `argtable3`
  - [x] Define `cmd_ls_spec` with `.type = CMD_EXTERNAL`
  - [x] Implement `jshell_register_ls_command()`
- [x] Create `src/apps/ls/ls_main.c`:
  - [x] Implement `main()` that calls `cmd_ls_spec.run(argc, argv)`
- [x] Update Makefile to build standalone `ls` binary
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/ls/test_ls.py`:
  - [x] Implement unit tests using Python unittest framework

### 2.2 stat - File Metadata ✅ COMPLETED
**External App** (`src/apps/stat/`)

- [x] Create `src/apps/stat/cmd_stat.h`:
  - [x] Declare `extern jshell_cmd_spec_t cmd_stat_spec;`
  - [x] Declare `void jshell_register_stat_command(void);`
- [x] Create `src/apps/stat/cmd_stat.c`:
  - [x] Implement `build_stat_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
    - [x] `<FILE>` (required positional argument)
  - [x] Implement `stat_run()`
  - [x] Implement `stat_print_usage()`
  - [x] Define `cmd_stat_spec` with `.type = CMD_EXTERNAL`
  - [x] Implement `jshell_register_stat_command()`
- [x] Create `src/apps/stat/stat_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/stat/test_stat.py`:
  - [x] Implement unit tests using Python unittest framework

### 2.3 cat - Print File Contents ✅ COMPLETED
**External App** (`src/apps/cat/`)

- [x] Create `src/apps/cat/cmd_cat.h`
- [x] Create `src/apps/cat/cmd_cat.c`:
  - [x] Implement `build_cat_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json` (optional, wraps output)
    - [x] `<FILE...>` (one or more files)
  - [x] Implement `cat_run()`
  - [x] Implement `cat_print_usage()`
  - [x] Define `cmd_cat_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/cat/cat_main.c`
- [x] Update Makefile
- [x] Create `tests/apps/cat/test_cat.py`:
  - [x] Implement unit tests using Python unittest framework

### 2.4 head - View Start of File ✅ COMPLETED
**External App** (`src/apps/head/`)

- [x] Create `src/apps/head/cmd_head.h`
- [x] Create `src/apps/head/cmd_head.c`:
  - [x] Implement `build_head_argtable()` with:
    - [x] `-h, --help`
    - [x] `-n N` (number of lines, default 10)
    - [x] `--json`
    - [x] `<FILE>`
  - [x] Implement `head_run()`
  - [x] Implement `head_print_usage()`
  - [x] Define `cmd_head_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/head/head_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/head/test_head.py`:
  - [x] Implement unit tests using Python unittest framework
- [x] Update `tests/Makefile` to add head test target

### 2.5 tail - View End of File ✅ COMPLETED
**External App** (`src/apps/tail/`)

- [x] Create `src/apps/tail/cmd_tail.h`
- [x] Create `src/apps/tail/cmd_tail.c`:
  - [x] Implement `build_tail_argtable()` with:
    - [x] `-h, --help`
    - [x] `-n N` (number of lines, default 10)
    - [x] `--json`
    - [x] `<FILE>`
  - [x] Implement `tail_run()`
  - [x] Implement `tail_print_usage()`
  - [x] Define `cmd_tail_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/tail/tail_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/tail/test_tail.py`:
  - [x] Implement unit tests using Python unittest framework
- [x] Update `tests/Makefile` to add tail test target

### 2.6 cp - Copy Files/Directories ✅ COMPLETED
**External App** (`src/apps/cp/`)

- [x] Create `src/apps/cp/cmd_cp.h`
- [x] Create `src/apps/cp/cmd_cp.c`:
  - [x] Implement `build_cp_argtable()` with:
    - [x] `-h, --help`
    - [x] `-r` (recursive)
    - [x] `-f` (force overwrite)
    - [x] `--json`
    - [x] `<SOURCE>` (required)
    - [x] `<DEST>` (required)
  - [x] Implement `cp_run()`
  - [x] Implement `cp_print_usage()`
  - [x] Define `cmd_cp_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/cp/cp_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/cp/test_cp.py`:
  - [x] Implement unit tests using Python unittest framework
- [x] Update `tests/Makefile` to add cp test target

### 2.7 mv - Move/Rename Files ✅ COMPLETED
**External App** (`src/apps/mv/`)

- [x] Create `src/apps/mv/cmd_mv.h`
- [x] Create `src/apps/mv/cmd_mv.c`:
  - [x] Implement `build_mv_argtable()` with:
    - [x] `-h, --help`
    - [x] `-f` (force)
    - [x] `--json`
    - [x] `<SOURCE>`
    - [x] `<DEST>`
  - [x] Implement `mv_run()`
  - [x] Implement `mv_print_usage()`
  - [x] Define `cmd_mv_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/mv/mv_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/mv/test_mv.py`:
  - [x] Implement unit tests using Python unittest framework
- [x] Update `tests/Makefile` to add mv test target

### 2.8 rm - Remove Files/Directories ✅ COMPLETED
**External App** (`src/apps/rm/`)

- [x] Create `src/apps/rm/cmd_rm.h`
- [x] Create `src/apps/rm/cmd_rm.c`:
  - [x] Implement `build_rm_argtable()` with:
    - [x] `-h, --help`
    - [x] `-r` (recursive)
    - [x] `-f` (force, no prompt)
    - [x] `--json`
    - [x] `<FILE...>` (one or more files)
  - [x] Implement `rm_run()`
  - [x] Implement `rm_print_usage()`
  - [x] Define `cmd_rm_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/rm/rm_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/rm/test_rm.py`:
  - [x] Implement unit tests using Python unittest framework
- [x] Update `tests/Makefile` to add rm test target

### 2.9 mkdir - Create Directories ✅ COMPLETED
**External App** (`src/apps/mkdir/`)

- [x] Create `src/apps/mkdir/cmd_mkdir.h`
- [x] Create `src/apps/mkdir/cmd_mkdir.c`:
  - [x] Implement `build_mkdir_argtable()` with:
    - [x] `-h, --help`
    - [x] `-p` (create parents)
    - [x] `--json`
    - [x] `<DIR...>` (one or more directories)
  - [x] Implement `mkdir_run()`
  - [x] Implement `mkdir_print_usage()`
  - [x] Define `cmd_mkdir_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/mkdir/mkdir_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/mkdir/test_mkdir.py`:
  - [x] Implement unit tests using Python unittest framework
- [x] Update `tests/Makefile` to add mkdir test target

### 2.10 rmdir - Remove Empty Directories ✅ COMPLETED
**External App** (`src/apps/rmdir/`)

- [x] Create `src/apps/rmdir/cmd_rmdir.h`
- [x] Create `src/apps/rmdir/cmd_rmdir.c`:
  - [x] Implement `build_rmdir_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
    - [x] `<DIR...>`
  - [x] Implement `rmdir_run()`
  - [x] Implement `rmdir_print_usage()`
  - [x] Define `cmd_rmdir_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/rmdir/rmdir_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/rmdir/test_rmdir.py`:
  - [x] Implement unit tests using Python unittest framework
- [x] Update `tests/Makefile` to add rmdir test target

### 2.11 touch - Create Empty File or Update Timestamps ✅ COMPLETED
**External App** (`src/apps/touch/`)

- [x] Create `src/apps/touch/cmd_touch.h`
- [x] Create `src/apps/touch/cmd_touch.c`:
  - [x] Implement `build_touch_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
    - [x] `<FILE...>`
  - [x] Implement `touch_run()`
  - [x] Implement `touch_print_usage()`
  - [x] Define `cmd_touch_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/touch/touch_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/touch/test_touch.py`:
  - [x] Implement unit tests using Python unittest framework
- [x] Update `tests/Makefile` to add touch test target

---

## Phase 3: Search and Text Tools (Agent-Facing, HIGH PRIORITY)

### 3.1 rg - Regex Search ✅ COMPLETED
**External App** (`src/apps/rg/`)

- [x] Create `src/apps/rg/cmd_rg.h`
- [x] Create `src/apps/rg/cmd_rg.c`:
  - [x] Implement `build_rg_argtable()` with:
    - [x] `-h, --help`
    - [x] `-n` (show line numbers)
    - [x] `-i` (case-insensitive)
    - [x] `-w` (match whole words)
    - [x] `-C N` (context lines)
    - [x] `--fixed-strings` (literal, not regex)
    - [x] `--json` (emit JSON per match)
    - [x] `<PATTERN>` (required)
    - [x] `<FILE...>` (files to search)
  - [x] Implement `rg_run()`:
    - [x] Use POSIX regex (`regcomp`, `regexec`)
    - [x] Search files for pattern
    - [x] Support all flags
    - [x] Support `--json` output: `{"file": "...", "line": N, "column": N, "text": "..."}`
  - [x] Implement `rg_print_usage()`
  - [x] Define `cmd_rg_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/rg/rg_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/rg/test_rg.py`:
  - [x] Implement unit tests using Python unittest framework
- [x] Update `tests/Makefile` to add rg test target

---

## Phase 4: Structured Editing Commands (Agent-Facing, HIGH PRIORITY) ✅ COMPLETED

These commands are **Builtins** because they provide core file editing functionality that should be built into the shell.

### 4.1 edit-replace-line - Replace Single Line ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`)

- [x] Create `src/jshell/builtins/cmd_edit_replace_line.h`:
  - [x] Declare `extern const jshell_cmd_spec_t cmd_edit_replace_line_spec;`
  - [x] Declare `void jshell_register_edit_replace_line_command(void);`
- [x] Create `src/jshell/builtins/cmd_edit_replace_line.c`:
  - [x] Implement `build_edit_replace_line_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
    - [x] `<FILE>` (required)
    - [x] `<LINE_NUM>` (required, integer)
    - [x] `<TEXT>` (required, replacement text)
  - [x] Implement `edit_replace_line_run()`
  - [x] Implement `edit_replace_line_print_usage()`
  - [x] Define `cmd_edit_replace_line_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_edit_replace_line_command()`
- [x] Update `jshell_register_builtins.h` and `.c`
- [x] Create `tests/jshell/builtins/test_edit_replace_line.py`

### 4.2 edit-insert-line - Insert Line Before Given Line ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`)

- [x] Create `src/jshell/builtins/cmd_edit_insert_line.h`:
  - [x] Declare `extern const jshell_cmd_spec_t cmd_edit_insert_line_spec;`
  - [x] Declare `void jshell_register_edit_insert_line_command(void);`
- [x] Create `src/jshell/builtins/cmd_edit_insert_line.c`:
  - [x] Implement `build_edit_insert_line_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
    - [x] `<FILE>`
    - [x] `<LINE_NUM>`
    - [x] `<TEXT>`
  - [x] Implement `edit_insert_line_run()`
  - [x] Implement `edit_insert_line_print_usage()`
  - [x] Define `cmd_edit_insert_line_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_edit_insert_line_command()`
- [x] Update `jshell_register_builtins.h` and `.c`
- [x] Create `tests/jshell/builtins/test_edit_insert_line.py`

### 4.3 edit-delete-line - Delete Single Line ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`)

- [x] Create `src/jshell/builtins/cmd_edit_delete_line.h`:
  - [x] Declare `extern const jshell_cmd_spec_t cmd_edit_delete_line_spec;`
  - [x] Declare `void jshell_register_edit_delete_line_command(void);`
- [x] Create `src/jshell/builtins/cmd_edit_delete_line.c`:
  - [x] Implement `build_edit_delete_line_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
    - [x] `<FILE>`
    - [x] `<LINE_NUM>`
  - [x] Implement `edit_delete_line_run()`
  - [x] Implement `edit_delete_line_print_usage()`
  - [x] Define `cmd_edit_delete_line_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_edit_delete_line_command()`
- [x] Update `jshell_register_builtins.h` and `.c`
- [x] Create `tests/jshell/builtins/test_edit_delete_line.py`

### 4.4 edit-replace - Global Find/Replace with Regex ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`)

- [x] Create `src/jshell/builtins/cmd_edit_replace.h`:
  - [x] Declare `extern const jshell_cmd_spec_t cmd_edit_replace_spec;`
  - [x] Declare `void jshell_register_edit_replace_command(void);`
- [x] Create `src/jshell/builtins/cmd_edit_replace.c`:
  - [x] Implement `build_edit_replace_argtable()` with:
    - [x] `-h, --help`
    - [x] `-i` (case-insensitive)
    - [x] `--fixed-strings` (literal, not regex)
    - [x] `--json`
    - [x] `<FILE>`
    - [x] `<PATTERN>` (regex or literal)
    - [x] `<REPLACEMENT>`
  - [x] Implement `edit_replace_run()`:
    - [x] Use POSIX regex (`regcomp`, `regexec`)
    - [x] Support `--fixed-strings` for literal matches
    - [x] Support `-i` for case-insensitive regex
  - [x] Implement `edit_replace_print_usage()`
  - [x] Define `cmd_edit_replace_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_edit_replace_command()`
- [x] Update `jshell_register_builtins.h` and `.c`
- [x] Create `tests/jshell/builtins/test_edit_replace.py`

---

## Phase 5: Process and Job Control (MEDIUM PRIORITY) ✅ COMPLETED

These commands are **Builtins** because they need access to shell internal state (job table, process groups).

### 5.1 jobs - List Background Jobs (Refactor Existing) ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`)

- [x] Refactor `src/jshell/builtins/jobs.c`:
  - [x] Rename to `cmd_jobs.c`
  - [x] Implement `build_jobs_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
  - [x] Refactor `jobs_run()` to use `argtable3`
  - [x] Add `--json` output support
  - [x] Implement `jobs_print_usage()` using `argtable3`
  - [x] Define `cmd_jobs_spec` with `.type = CMD_BUILTIN`
  - [x] Rename `jshell_register_jobs_cmd()` to `jshell_register_jobs_command()`
- [x] Update `src/jshell/builtins/cmd_jobs.h` (create if needed)
- [x] Update `jshell_register_builtins.c` to call registration

### 5.2 ps - List Processes/Threads ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`)

- [x] Create `src/jshell/builtins/cmd_ps.h`
- [x] Create `src/jshell/builtins/cmd_ps.c`:
  - [x] Implement `build_ps_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
  - [x] Implement `ps_run()`
  - [x] Implement `ps_print_usage()`
  - [x] Define `cmd_ps_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_ps_command()`
- [x] Update `jshell_register_builtins.h` and `.c`

### 5.3 kill - Send Signal to Job/Process ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`)

- [x] Create `src/jshell/builtins/cmd_kill.h`
- [x] Create `src/jshell/builtins/cmd_kill.c`:
  - [x] Implement `build_kill_argtable()` with:
    - [x] `-h, --help`
    - [x] `-s SIGNAL` (signal name or number)
    - [x] `--json`
    - [x] `<PID>` (required)
  - [x] Implement `kill_run()`
  - [x] Implement `kill_print_usage()`
  - [x] Define `cmd_kill_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_kill_command()`
- [x] Update `jshell_register_builtins.h` and `.c`

### 5.4 wait - Wait for Job to Finish ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`)

- [x] Create `src/jshell/builtins/cmd_wait.h`
- [x] Create `src/jshell/builtins/cmd_wait.c`:
  - [x] Implement `build_wait_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
    - [x] `<JOB_ID>` (optional)
  - [x] Implement `wait_run()`
  - [x] Implement `wait_print_usage()`
  - [x] Define `cmd_wait_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_wait_command()`
- [x] Update `jshell_register_builtins.h` and `.c`

---

## Phase 6: Shell and Environment (MEDIUM PRIORITY) ✅ COMPLETED

All shell and environment commands are implemented as **Builtins** for consistency.

### 6.1 cd - Change Directory ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`) - Must be builtin to change shell's working directory

- [x] Create `src/jshell/builtins/cmd_cd.h`
- [x] Create `src/jshell/builtins/cmd_cd.c`:
  - [x] Implement `build_cd_argtable()` with:
    - [x] `-h, --help`
    - [x] `[DIR]` (optional, defaults to HOME)
  - [x] Implement `cd_run()`
  - [x] Implement `cd_print_usage()`
  - [x] Define `cmd_cd_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_cd_command()`
- [x] Update `jshell_register_builtins.h` and `.c`
- [x] Create `tests/jshell/builtins/test_cd.py`

### 6.2 pwd - Print Working Directory ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`)

- [x] Create `src/jshell/builtins/cmd_pwd.h`
- [x] Create `src/jshell/builtins/cmd_pwd.c`:
  - [x] Implement `build_pwd_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
  - [x] Implement `pwd_run()`
  - [x] Implement `pwd_print_usage()`
  - [x] Define `cmd_pwd_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_pwd_command()`
- [x] Update `jshell_register_builtins.h` and `.c`
- [x] Create `tests/jshell/builtins/test_pwd.py`

### 6.3 env - List Environment Variables ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`)

- [x] Create `src/jshell/builtins/cmd_env.h`
- [x] Create `src/jshell/builtins/cmd_env.c`:
  - [x] Implement `build_env_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
  - [x] Implement `env_run()`
  - [x] Implement `env_print_usage()`
  - [x] Define `cmd_env_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_env_command()`
- [x] Update `jshell_register_builtins.h` and `.c`
- [x] Create `tests/jshell/builtins/test_env.py`

### 6.4 export - Set Environment Variable ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`) - Must be builtin to modify shell's environment

- [x] Create `src/jshell/builtins/cmd_export.h`
- [x] Create `src/jshell/builtins/cmd_export.c`:
  - [x] Implement `build_export_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
    - [x] `<KEY=VALUE>` (one or more)
  - [x] Implement `export_run()`
  - [x] Implement `export_print_usage()`
  - [x] Define `cmd_export_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_export_command()`
- [x] Update `jshell_register_builtins.h` and `.c`
- [x] Create `tests/jshell/builtins/test_export.py`

### 6.5 unset - Unset Environment Variable ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`) - Must be builtin to modify shell's environment

- [x] Create `src/jshell/builtins/cmd_unset.h`
- [x] Create `src/jshell/builtins/cmd_unset.c`:
  - [x] Implement `build_unset_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
    - [x] `<KEY>` (one or more)
  - [x] Implement `unset_run()`
  - [x] Implement `unset_print_usage()`
  - [x] Define `cmd_unset_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_unset_command()`
- [x] Update `jshell_register_builtins.h` and `.c`
- [x] Create `tests/jshell/builtins/test_unset.py`

### 6.6 type - Show How Name is Resolved ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`) - Needs access to shell's command registry

- [x] Create `src/jshell/builtins/cmd_type.h`
- [x] Create `src/jshell/builtins/cmd_type.c`:
  - [x] Implement `build_type_argtable()` with:
    - [x] `-h, --help`
    - [x] `--json`
    - [x] `<NAME>` (required)
  - [x] Implement `type_run()`
  - [x] Implement `type_print_usage()`
  - [x] Define `cmd_type_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_type_command()`
- [x] Update `jshell_register_builtins.h` and `.c`
- [x] Create `tests/jshell/builtins/test_type.py`

---

## Phase 7: Human-Facing Tools (LOW PRIORITY)

### 7.1 echo - Print Text ✅ COMPLETED
**External App** (`src/apps/echo/`)

- [x] Create `src/apps/echo/cmd_echo.h`
- [x] Create `src/apps/echo/cmd_echo.c`:
  - [x] Implement `build_echo_argtable()` with:
    - [x] `-h, --help`
    - [x] `-n` (no trailing newline)
    - [x] `<TEXT...>` (arguments to print)
  - [x] Implement `echo_run()`
  - [x] Implement `echo_print_usage()`
  - [x] Define `cmd_echo_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/echo/echo_main.c`
- [x] Update Makefile
- [x] Create `tests/apps/echo/test_echo.py`:
  - [x] Implement unit tests using Python unittest framework

### 7.2 sleep - Delay ✅ COMPLETED
**External App** (`src/apps/sleep/`)

- [x] Create `src/apps/sleep/cmd_sleep.h`
- [x] Create `src/apps/sleep/cmd_sleep.c`:
  - [x] Implement `build_sleep_argtable()` with:
    - [x] `-h, --help`
    - [x] `<SECONDS>` (required, can be float)
  - [x] Implement `sleep_run()`
  - [x] Implement `sleep_print_usage()`
  - [x] Define `cmd_sleep_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/sleep/sleep_main.c`
- [x] Update Makefile
- [x] Create `tests/apps/sleep/test_sleep.py`:
  - [x] Implement unit tests using Python unittest framework

### 7.3 date - Show System Time ✅ COMPLETED
**External App** (`src/apps/date/`)

- [x] Create `src/apps/date/cmd_date.h`
- [x] Create `src/apps/date/cmd_date.c`:
  - [x] Implement `build_date_argtable()` with:
    - [x] `-h, --help`
  - [x] Implement `date_run()`
  - [x] Implement `date_print_usage()`
  - [x] Define `cmd_date_spec` with `.type = CMD_EXTERNAL`
- [x] Create `src/apps/date/date_main.c`
- [x] Update Makefile
- [x] Create `tests/apps/date/test_date.py`:
  - [x] Implement unit tests using Python unittest framework

### 7.4 help - Shell Built-in Help ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`) - Needs access to command registry
Help should print all registered commands.

- [x] Create `src/jshell/builtins/cmd_help.h`
- [x] Create `src/jshell/builtins/cmd_help.c`:
  - [x] Implement `build_help_argtable()` with:
    - [x] `-h, --help`
    - [x] `[COMMAND]` (optional)
  - [x] Implement `help_run()`
  - [x] Implement `help_print_usage()`
  - [x] Define `cmd_help_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_help_command()`
- [x] Update `jshell_register_builtins.h` and `.c`

### 7.8 history - Show Command History ✅ COMPLETED
**Builtin** (`src/jshell/builtins/`) - Needs access to shell's history

- [x] Create `src/jshell/builtins/cmd_history.h`
- [x] Create `src/jshell/builtins/cmd_history.c`:
  - [x] Implement `build_history_argtable()` with:
    - [x] `-h, --help`
  - [x] Implement `history_run()`
  - [x] Implement `history_print_usage()`
  - [x] Define `cmd_history_spec` with `.type = CMD_BUILTIN`
  - [x] Implement `jshell_register_history_command()`
- [x] Update `jshell_register_builtins.h` and `.c`
- [x] Create `src/jshell/jshell_history.h` and `.c` for history storage

### 7.9 less - Pager ✅ COMPLETED
**External App** (`src/apps/less/`)

- [x] Create `src/apps/less/cmd_less.h`:
  - [x] Declare `extern jshell_cmd_spec_t cmd_less_spec;`
  - [x] Declare `void jshell_register_less_command(void);`
- [x] Create `src/apps/less/cmd_less.c`:
  - [x] Implement `build_less_argtable()` with:
    - [x] `-h, --help`
    - [x] `-N` (show line numbers)
    - [x] `[FILE]` (optional file argument)
  - [x] Implement `less_run()`:
    - [x] Read file content into memory
    - [x] Set terminal to raw mode for keyboard input
    - [x] Display content page by page
    - [x] Handle navigation: j/k/arrows, space/b, g/G, q, /pattern, n/N
  - [x] Implement `less_print_usage()`
  - [x] Define `cmd_less_spec` with `.type = CMD_EXTERNAL`
  - [x] Implement `jshell_register_less_command()`
- [x] Create `src/apps/less/less_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/less/test_less.py`:
  - [x] Implement unit tests using Python unittest framework
- [x] Update `tests/Makefile` to add less test target

### 7.10 vi - Text Editor ✅ COMPLETED
**External App** (`src/apps/vi/`)

- [x] Create `src/apps/vi/cmd_vi.h`:
  - [x] Declare `extern jshell_cmd_spec_t cmd_vi_spec;`
  - [x] Declare `void jshell_register_vi_command(void);`
- [x] Create `src/apps/vi/cmd_vi.c`:
  - [x] Implement `build_vi_argtable()` with:
    - [x] `-h, --help`
    - [x] `[FILE]` (optional file argument)
  - [x] Implement `vi_run()`:
    - [x] Normal mode: hjkl navigation, w/b word movement, 0/$ line ends
    - [x] Insert mode: text input, backspace, enter for new lines
    - [x] Command mode: :w, :q, :wq, :q!, :e commands
    - [x] Search: / for forward search, n for next match
    - [x] Editing: x delete, dd delete line, yy yank, p/P paste
  - [x] Implement `vi_print_usage()`
  - [x] Define `cmd_vi_spec` with `.type = CMD_EXTERNAL`
  - [x] Implement `jshell_register_vi_command()`
- [x] Create `src/apps/vi/vi_main.c`
- [x] Update Makefile
- [x] Register in `jshell_register_externals.c`
- [x] Create `tests/apps/vi/test_vi.py`:
  - [x] Implement unit tests using Python unittest framework
- [x] Update `tests/Makefile` to add vi test target

---

## Phase 8: Networking (FUTURE)

### 8.1 http-get - Fetch URL
**External App** (`src/apps/http-get/`)

- [ ] Create `src/apps/http-get/cmd_http_get.h`
- [ ] Create `src/apps/http-get/cmd_http_get.c`:
  - [ ] Implement `build_http_get_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-H KEY:VALUE` (headers, repeatable)
    - [ ] `--json`
    - [ ] `<URL>` (required)
  - [ ] Implement `http_get_run()`
  - [ ] Implement `http_get_print_usage()`
  - [ ] Define `cmd_http_get_spec` with `.type = CMD_EXTERNAL`
- [ ] Create `src/apps/http-get/http_get_main.c`
- [ ] Update Makefile
- [ ] Create `tests/apps/http-get/test_http_get.py`:
  - [ ] Implement unit tests using Python unittest framework

### 8.2 http-post - POST to URL
**External App** (`src/apps/http-post/`)

- [ ] Create `src/apps/http-post/cmd_http_post.h`
- [ ] Create `src/apps/http-post/cmd_http_post.c`:
  - [ ] Implement `build_http_post_argtable()` with:
    - [ ] `-h, --help`
    - [ ] `-H KEY:VALUE` (headers)
    - [ ] `-d DATA` (body data)
    - [ ] `--json`
    - [ ] `<URL>` (required)
  - [ ] Implement `http_post_run()`
  - [ ] Implement `http_post_print_usage()`
  - [ ] Define `cmd_http_post_spec` with `.type = CMD_EXTERNAL`
- [ ] Create `src/apps/http-post/http_post_main.c`
- [ ] Update Makefile
- [ ] Create `tests/apps/http-post/test_http_post.py`:
  - [ ] Implement unit tests using Python unittest framework

---

## Phase 9: Package Manager Integration (FUTURE)

### 9.1 Refactor pkg Command
**Builtin** (`src/jshell/builtins/`)

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
  - [ ] Define `cmd_pkg_spec` with `.type = CMD_BUILTIN`
  - [ ] Implement `jshell_register_pkg_command()`
- [ ] Update `jshell_register_builtins.h` and `.c`

---

## Notes

### Command Type Summary

**Builtins** (run by `jshell_exec_builtin()`, modify shell state):
- jobs, ps, kill, wait (job control)
- cd, export, unset (environment)
- type, help, history, alias, unalias (shell introspection)
- edit-replace-line, edit-insert-line, edit-delete-line, edit-replace (editing)
- pkg (package manager)

**External Apps** (fork/exec, standalone binaries):
- ls, stat, cat, head, tail (file viewing)
- cp, mv, rm, mkdir, rmdir, touch (file manipulation)
- rg (search)
- pwd, env (read-only environment)
- echo, sleep, date (utilities)
- less, vi (interactive tools)
- http-get, http-post (networking)

### Command Anatomy Checklist

For each **Builtin**, ensure:
- [ ] Source in `src/jshell/builtins/cmd_<name>.c`
- [ ] Header in `src/jshell/builtins/cmd_<name>.h`
- [ ] Uses `argtable3` for CLI parsing
- [ ] Implements `build_<cmd>_argtable()` function
- [ ] Implements `<cmd>_run(int argc, char **argv)` function
- [ ] Implements `<cmd>_print_usage(FILE *out)` function
- [ ] Defines `cmd_<cmd>_spec` with `.type = CMD_BUILTIN`
- [ ] Implements `jshell_register_<cmd>_command()` function
- [ ] Registered in `jshell_register_builtins.c`
- [ ] Supports `-h, --help`
- [ ] Agent-facing commands support `--json`

For each **External App**, ensure:
- [ ] Source in `src/apps/<name>/cmd_<name>.c`
- [ ] Header in `src/apps/<name>/cmd_<name>.h`
- [ ] Main wrapper in `src/apps/<name>/<name>_main.c`
- [ ] Uses `argtable3` for CLI parsing
- [ ] Implements `build_<cmd>_argtable()` function
- [ ] Implements `<cmd>_run(int argc, char **argv)` function
- [ ] Implements `<cmd>_print_usage(FILE *out)` function
- [ ] Defines `cmd_<cmd>_spec` with `.type = CMD_EXTERNAL`
- [ ] Makefile target to build standalone binary
- [ ] Tests in `tests/apps/<name>/test_<name>.py`
- [ ] Supports `-h, --help`
- [ ] Agent-facing commands support `--json`

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
