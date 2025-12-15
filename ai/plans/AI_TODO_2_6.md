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

### 1.1 Update jshell.h
Add new function signatures for non-interactive mode.

- [ ] Update `src/jshell/jshell.h`:
  - [ ] Change `int jshell_main(void);` to `int jshell_main(int argc, char **argv);`
  - [ ] Add `int jshell_exec_string(const char *cmd_string);`
  - [ ] Add `void jshell_print_usage(FILE *out);`

### 1.2 Add argtable3 Integration to jshell
Implement argument parsing for jshell.

- [ ] Update `src/jshell/jshell.c`:
  - [ ] Include `argtable3.h`
  - [ ] Implement `build_jshell_argtable()` with:
    - [ ] `-h, --help` (show usage)
    - [ ] `-c COMMAND` (execute command string)
    - [ ] `-v, --version` (show version, optional)
  - [ ] Implement `jshell_print_usage(FILE *out)`
  - [ ] Update `jshell_main(int argc, char **argv)`:
    - [ ] Parse arguments with argtable3
    - [ ] Handle `-h/--help` flag
    - [ ] Handle `-c` option - call `jshell_exec_string()`
    - [ ] Fall through to interactive mode if no `-c`

### 1.3 Implement jshell_exec_string()
Execute a command string without interactive mode.

- [ ] Implement `jshell_exec_string(const char *cmd_string)` in `jshell.c`:
  - [ ] Initialize job control (call `jshell_init_job_control()`)
  - [ ] Register commands (call `jshell_register_all_builtin_commands()`)
  - [ ] Register externals (call `jshell_register_all_external_commands()`)
  - [ ] Parse command string with `psInput()`
  - [ ] Execute via `interpretInput()`
  - [ ] Free parse tree
  - [ ] Return last command exit status
  - [ ] Do NOT print prompts or welcome messages

### 1.4 Track Last Exit Status
Add mechanism to capture and return command exit status.

- [ ] Add global or context variable for last exit status
- [ ] Update `jshell_ast_helpers.c` or interpreter to set exit status
- [ ] Ensure `jshell_exec_string()` returns this status

---

## Phase 2: Update jbox Entry Point

### 2.1 Add jshell Symlink Support
Update jbox.c to handle `jshell` invocation.

- [ ] Update `src/jbox.c`:
  - [ ] Add case for `strcmp(cmd, "jshell") == 0`
  - [ ] Pass `argc, argv` to `jshell_main()`
  - [ ] Remove or conditionalize "welcome to jbox!" message

### 2.2 Update jbox Interactive Mode
Handle the case when jbox is invoked directly.

- [ ] When invoked as `jbox` without arguments:
  - [ ] Call `jshell_main(argc, argv)` to let it handle args
  - [ ] Or show jbox help/dispatch menu
- [ ] When invoked as `jbox jshell ...`:
  - [ ] Forward arguments to `jshell_main()`

---

## Phase 3: Build System Updates

### 3.1 Create jshell Symlink
Add Makefile target for jshell symlink.

- [ ] Update `Makefile`:
  - [ ] Add target to create `bin/jshell` symlink pointing to `bin/jbox`
  - [ ] Add symlink creation to default build target
  - [ ] Add symlink removal to clean target

### 3.2 Update jshell Compilation
Ensure jshell.c links against argtable3.

- [ ] Verify `src/jshell/jshell.c` is compiled with argtable3 access
- [ ] Update any necessary include paths

---

## Phase 4: Create Test Helper Module

### 4.1 Create Python Test Helper
Create a shared test helper for running jshell commands.

- [ ] Create `tests/helpers/__init__.py`
- [ ] Create `tests/helpers/jshell.py`:
  ```python
  class JShellRunner:
      """Helper for running jshell commands in tests."""

      JSHELL = Path(__file__).parent.parent.parent / "bin" / "jshell"

      @classmethod
      def run(cls, command: str, env: dict = None) -> subprocess.CompletedProcess:
          """Run a command via jshell -c and return result."""
          ...

      @classmethod
      def run_json(cls, command: str) -> dict:
          """Run a command and parse JSON output."""
          ...
  ```

### 4.2 Helper Features
Implement helper functionality.

- [ ] `run(command)` - execute single command, return CompletedProcess
- [ ] `run_json(command)` - execute and parse JSON output
- [ ] `run_multi(commands)` - execute multiple commands (semicolon-separated)
- [ ] Automatic ASAN options handling
- [ ] Clean error messages on failure

---

## Phase 5: Update Builtin Tests

### 5.1 Update test_cd.py
Refactor cd tests to use new mechanism.

- [ ] Import `JShellRunner` from helpers
- [ ] Replace `run_cmd()` with `JShellRunner.run()`
- [ ] Replace `run_cmds()` with semicolon-separated commands
- [ ] Remove stdout filtering (welcome message, prompts)
- [ ] Simplify test assertions

### 5.2 Update test_pwd.py
Refactor pwd tests to use new mechanism.

- [ ] Import `JShellRunner` from helpers
- [ ] Replace `run_cmd()` with `JShellRunner.run()`
- [ ] Replace `run_cmds()` with semicolon-separated commands
- [ ] Use `JShellRunner.run_json()` for JSON tests
- [ ] Remove stdout filtering

### 5.3 Update test_env.py
Refactor env tests.

- [ ] Import `JShellRunner` from helpers
- [ ] Update test methods to use new helper
- [ ] Remove stdout filtering

### 5.4 Update test_export.py
Refactor export tests.

- [ ] Import `JShellRunner` from helpers
- [ ] Update test methods to use new helper
- [ ] Remove stdout filtering

### 5.5 Update test_unset.py
Refactor unset tests.

- [ ] Import `JShellRunner` from helpers
- [ ] Update test methods to use new helper
- [ ] Remove stdout filtering

### 5.6 Update test_type.py
Refactor type tests.

- [ ] Import `JShellRunner` from helpers
- [ ] Update test methods to use new helper
- [ ] Remove stdout filtering

### 5.7 Update Remaining Builtin Tests
Update all other builtin test files.

- [ ] `test_edit_replace_line.py`
- [ ] `test_edit_insert_line.py`
- [ ] `test_edit_delete_line.py`
- [ ] `test_edit_replace.py`
- [ ] `test_http_get.py`
- [ ] `test_http_post.py`

---

## Phase 6: Documentation and Verification

### 6.1 Update Help Text
Ensure help output is clear and complete.

- [ ] `jshell -h` shows:
  ```
  Usage: jshell [-h] [-c COMMAND]

  jshell - the jbox shell

  Options:
    -h, --help       Show this help message and exit
    -c COMMAND       Execute COMMAND and exit

  When invoked without -c, runs in interactive mode.
  ```

### 6.2 Verify Exit Code Propagation
Test that exit codes work correctly.

- [ ] `jshell -c "true"` returns 0
- [ ] `jshell -c "false"` returns 1 (if false builtin exists)
- [ ] `jshell -c "nonexistent_command"` returns non-zero
- [ ] Exit code from last command in chain is returned

### 6.3 Run All Tests
Verify all tests pass with new mechanism.

- [ ] Run `make -C tests builtins`
- [ ] Run individual test files
- [ ] Verify no regressions

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

## Checklist

### Phase 1: jshell Entry Point
- [ ] Update jshell.h with new signatures
- [ ] Implement argtable3 argument parsing
- [ ] Implement jshell_exec_string()
- [ ] Track and return exit status

### Phase 2: jbox Entry Point
- [ ] Add jshell symlink handling
- [ ] Update jbox main() to forward args

### Phase 3: Build System
- [ ] Create jshell symlink in Makefile
- [ ] Verify argtable3 linking

### Phase 4: Test Helper
- [ ] Create tests/helpers/ module
- [ ] Implement JShellRunner class

### Phase 5: Update Tests
- [ ] Update all 12 builtin test files
- [ ] Remove stdout filtering code
- [ ] Verify all tests pass

### Phase 6: Documentation
- [ ] Verify help text
- [ ] Test exit code propagation
- [ ] Run full test suite

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
