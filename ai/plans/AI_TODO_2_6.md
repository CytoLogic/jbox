# JShell Non-Interactive Mode Implementation Plan

## Overview

Implement a non-interactive command execution mode for jshell, similar to `bash -c "command"`. This enables:
- Automated testing of shell builtins without parsing interactive output
- Script execution from command line
- Busybox-style symlink invocation (`jshell` -> `jbox`)

**Key Features:**
- `jshell -c "command"` executes a command and exits
- `jbox jshell -c "command"` alternative invocation
- Clean output without prompts or welcome messages
- Proper exit code propagation
- argtable3 for argument parsing (project standard)

### Current State

Currently, tests for builtins must:
1. Pipe commands to stdin
2. Strip welcome messages (`welcome to jbox!`)
3. Strip prompts (`(jsh)>`)
4. Strip debug output
5. Cannot easily get exit codes from individual commands

### Target State

Tests will be able to:
1. Run `jshell -c "pwd --json"` directly
2. Get clean output without filtering
3. Get proper exit codes
4. Chain multiple commands with semicolons

---

## Phase 1: Modify jshell Entry Point

### 1.1 Update jshell.h ✅ COMPLETED
Add new function signatures for non-interactive mode.

- [x] Update `src/jshell/jshell.h`:
  - [x] Change `int jshell_main(void);` to `int jshell_main(int argc, char **argv);`
  - [x] Add `int jshell_exec_string(const char *cmd_string);`
  - [x] Add `void jshell_print_usage(FILE *out);`

### 1.2 Add argtable3 Integration to jshell ✅ COMPLETED
Implement argument parsing for jshell.

- [x] Update `src/jshell/jshell.c`:
  - [x] Include `argtable3.h`
  - [x] Implement `build_jshell_argtable()` with:
    - [x] `-h, --help` (show usage)
    - [x] `-c COMMAND` (execute command string)
  - [x] Implement `jshell_print_usage(FILE *out)`
  - [x] Update `jshell_main(int argc, char **argv)`:
    - [x] Parse arguments with argtable3
    - [x] Handle `-h/--help` flag
    - [x] Handle `-c` option - call `jshell_exec_string()`
    - [x] Fall through to interactive mode if no `-c`

### 1.3 Implement jshell_exec_string() ✅ COMPLETED
Execute a command string without interactive mode.

- [x] Implement `jshell_exec_string(const char *cmd_string)` in `jshell.c`:
  - [x] Initialize job control (call `jshell_init_job_control()`)
  - [x] Register commands (call `jshell_register_all_builtin_commands()`)
  - [x] Register externals (call `jshell_register_all_external_commands()`)
  - [x] Parse command string with `psInput()`
  - [x] Execute via `interpretInput()`
  - [x] Free parse tree
  - [x] Return last command exit status
  - [x] Do NOT print prompts or welcome messages

### 1.4 Track Last Exit Status ✅ COMPLETED
Add mechanism to capture and return command exit status.

- [x] Add global or context variable for last exit status
- [x] Update `jshell_ast_helpers.c` or interpreter to set exit status
- [x] Ensure `jshell_exec_string()` returns this status

---

## Phase 2: Update jbox Entry Point ✅ COMPLETED

### 2.1 Add jshell Symlink Support ✅ COMPLETED
Update jbox.c to handle `jshell` invocation.

- [x] Update `src/jbox.c`:
  - [x] Add case for `strcmp(cmd, "jshell") == 0`
  - [x] Pass `argc, argv` to `jshell_main()`
  - [x] Remove or conditionalize "welcome to jbox!" message

### 2.2 Update jbox Interactive Mode ✅ COMPLETED
Handle the case when jbox is invoked directly.

- [x] When invoked as `jbox` without arguments:
  - [x] Call `jshell_main(argc, argv)` to let it handle args
- [x] When invoked as `jbox jshell ...`:
  - [x] Forward arguments to `jshell_main()`

---

## Phase 3: Build System Updates ✅ COMPLETED

### 3.1 Create jshell Symlink ✅ COMPLETED
Add Makefile target for jshell symlink.

- [x] Update `Makefile`:
  - [x] Add symlink creation to jbox build target
  - [x] Add symlink removal to clean target

### 3.2 Update jshell Compilation ✅ COMPLETED
Ensure jshell.c links against argtable3.

- [x] Verify `src/jshell/jshell.c` is compiled with argtable3 access

---

## Phase 4: Create Test Helper Module ✅ COMPLETED

### 4.1 Create Python Test Helper ✅ COMPLETED
Create a shared test helper for running jshell commands.

- [x] Create `tests/helpers/__init__.py`
- [x] Create `tests/helpers/jshell.py` with `JShellRunner` class

### 4.2 Helper Features ✅ COMPLETED
Implement helper functionality.

- [x] `run(command)` - execute single command, return CompletedProcess
- [x] `run_json(command)` - execute and parse JSON output
- [x] `run_multi(commands)` - execute multiple commands (semicolon-separated)
- [x] Automatic ASAN options handling
- [x] Clean debug output from results

---

## Phase 5: Update Builtin Tests ✅ COMPLETED

### 5.1 Update test_cd.py ✅ COMPLETED
- [x] Refactored to use `JShellRunner` helper

### 5.2 Update test_pwd.py ✅ COMPLETED
- [x] Refactored to use `JShellRunner` helper

### 5.3 Update test_env.py ✅ COMPLETED
- [x] Refactored to use `JShellRunner` helper

### 5.4 Update test_export.py ✅ COMPLETED
- [x] Refactored to use `JShellRunner` helper

### 5.5 Update test_unset.py ✅ COMPLETED
- [x] Refactored to use `JShellRunner` helper

### 5.6 Update test_type.py ✅ COMPLETED
- [x] Refactored to use `JShellRunner` helper

### 5.7 Update Remaining Builtin Tests
- [x] `test_edit_replace_line.py` - Updated to use `JShellRunner`
- [ ] `test_edit_insert_line.py` - Still uses old pattern (works)
- [ ] `test_edit_delete_line.py` - Still uses old pattern (works)
- [ ] `test_edit_replace.py` - Still uses old pattern (works)
- [ ] `test_http_get.py` - Still uses old pattern (works)
- [ ] `test_http_post.py` - Still uses old pattern (works)

Note: All 136 builtin tests pass. Remaining test files still work with old pattern.

---

## Phase 6: Documentation and Verification ✅ COMPLETED

### 6.1 Update Help Text ✅ COMPLETED
- [x] `jshell -h` shows proper help with `-c` option documented

### 6.2 Verify Exit Code Propagation ✅ COMPLETED
- [x] `jshell -c "pwd"` returns 0 (success)
- [x] `jshell -c "cd /nonexistent"` returns 1 (error)
- [x] Exit code from last command in chain is returned correctly

### 6.3 Run All Tests ✅ COMPLETED
- [x] All 136 builtin tests pass
- [x] No regressions

---

## Implementation Notes

### Argument Parsing Pattern

```c
static void build_jshell_argtable(struct arg_lit **help,
                                   struct arg_str **cmd,
                                   struct arg_end **end,
                                   void ***argtable_out)
{
    *help = arg_lit0("h", "help", "show help and exit");
    *cmd  = arg_str0("c", NULL, "COMMAND", "execute command and exit");
    *end  = arg_end(20);

    static void *argtable[4];
    argtable[0] = *help;
    argtable[1] = *cmd;
    argtable[2] = *end;
    argtable[3] = NULL;

    *argtable_out = argtable;
}
```

### jshell_main Pattern

```c
int jshell_main(int argc, char **argv) {
    struct arg_lit *help;
    struct arg_str *cmd;
    struct arg_end *end;
    void **argtable;

    build_jshell_argtable(&help, &cmd, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        jshell_print_usage(stdout);
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, "jshell");
        jshell_print_usage(stderr);
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 1;
    }

    if (cmd->count > 0) {
        // Non-interactive mode
        int status = jshell_exec_string(cmd->sval[0]);
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return status;
    }

    arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));

    // Interactive mode (existing code)
    return jshell_interactive();
}
```

### Test Helper Pattern

```python
from pathlib import Path
import subprocess
import os

class JShellRunner:
    JSHELL = Path(__file__).parent.parent.parent / "bin" / "jshell"

    @classmethod
    def run(cls, command: str, env: dict = None) -> subprocess.CompletedProcess:
        run_env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        if env:
            run_env.update(env)

        result = subprocess.run(
            [str(cls.JSHELL), "-c", command],
            capture_output=True,
            text=True,
            env=run_env
        )
        return result

    @classmethod
    def run_json(cls, command: str) -> dict:
        import json
        result = cls.run(command)
        return json.loads(result.stdout)
```

---

## Checklist ✅ ALL COMPLETED

### Phase 1: jshell Entry Point ✅
- [x] Update jshell.h with new signatures
- [x] Implement argtable3 argument parsing
- [x] Implement jshell_exec_string()
- [x] Track and return exit status

### Phase 2: jbox Entry Point ✅
- [x] Add jshell symlink handling
- [x] Update jbox main() to forward args

### Phase 3: Build System ✅
- [x] Create jshell symlink in Makefile
- [x] Verify argtable3 linking

### Phase 4: Test Helper ✅
- [x] Create tests/helpers/ module
- [x] Implement JShellRunner class

### Phase 5: Update Tests ✅
- [x] Update key builtin test files (7 updated, rest work with old pattern)
- [x] All 136 tests pass

### Phase 6: Documentation ✅
- [x] Verify help text
- [x] Test exit code propagation
- [x] Run full test suite - All pass

---

## Testing Commands

```bash
# Build with new changes
make jbox DEBUG=1

# Create symlink manually for testing
ln -sf jbox bin/jshell

# Test non-interactive mode
./bin/jshell -c "pwd"
./bin/jshell -c "pwd --json"
./bin/jshell -c "cd /tmp; pwd"
./bin/jshell -c "export FOO=bar; env" | grep FOO

# Test help
./bin/jshell -h
./bin/jshell --help

# Test exit codes
./bin/jshell -c "pwd"; echo $?
./bin/jshell -c "cd /nonexistent"; echo $?

# Run builtin tests
python -m unittest tests.jshell.builtins.test_pwd -v
python -m unittest tests.jshell.builtins.test_cd -v
```
