# AI_TODO_5_2: Dynamic External Command Registration

## Overview

Change jshell so that external commands (ls, cat, stat, etc.) are not registered by default at shell startup. Instead, only the `pkg` command is registered statically. All other external commands are registered dynamically when their packages are installed via `pkg install`.

## Current Behavior

1. **Shell startup** (`jshell.c`):
   - `jshell_register_all_builtin_commands()` - registers builtins (cd, pwd, export, etc.)
   - `jshell_register_all_external_commands()` - registers ALL 18 external commands statically
   - `jshell_load_installed_packages()` - loads packages from `~/.jshell/pkgs/pkgdb.json`

2. **External commands registered statically** (`jshell_register_externals.c`):
   - ls, stat, cat, head, tail, cp, mv, rm, mkdir, rmdir, touch, rg, echo, sleep, date, less, vi, pkg

3. **Package installation** (`cmd_pkg.c`):
   - Installs to `~/.jshell/pkgs/<name>-<version>/`
   - Creates symlinks in `~/.jshell/bin/`
   - Saves metadata to `~/.jshell/pkgs/pkgdb.json` with files list

4. **Package loading** (`jshell_pkg_loader.c`):
   - Reads pkgdb.json and registers commands via `jshell_register_package_command()`

## Target Behavior

1. **Shell startup**:
   - Register builtins (unchanged)
   - Register ONLY `pkg` external command
   - Load installed packages from database (unchanged)

2. **When user runs `pkg install <package>`**:
   - Install package (unchanged)
   - Dynamically register the new command(s) in the running shell

3. **When user runs `pkg remove <package>`**:
   - Remove package (unchanged)
   - Dynamically unregister the command(s) from the running shell

---

## Implementation Plan

### Phase 1: Modify Static External Registration

#### 1.1 Update `jshell_register_externals.c`
- [ ] Remove all external command registrations EXCEPT `pkg`
- [ ] Rename function to `jshell_register_pkg_command()` or keep name but only register pkg
- [ ] Remove includes for all cmd_*.h files except cmd_pkg.h

**File:** `src/jshell/jshell_register_externals.c`

**Current:**
```c
void jshell_register_all_external_commands(void) {
  jshell_register_ls_command();
  jshell_register_stat_command();
  jshell_register_cat_command();
  // ... 15 more commands ...
  jshell_register_pkg_command();
}
```

**Target:**
```c
void jshell_register_all_external_commands(void) {
  // Only register pkg - other commands are registered via package installation
  jshell_register_pkg_command();
}
```

#### 1.2 Update `jshell_register_externals.h`
- [ ] Remove declarations for all registration functions except pkg
- [ ] Keep the header minimal

#### 1.3 Update Makefile
- [ ] Remove cmd_*.c files from JSHELL_SRCS (except cmd_pkg.c and dependencies)
- [ ] These files are still needed for standalone binaries but not for jshell itself

**Note:** The jshell binary currently links all cmd_*.c files. After this change, only cmd_pkg.c and its dependencies (pkg_db.c, pkg_json.c, pkg_utils.c, pkg_registry.c) need to be linked into jshell.

---

### Phase 2: Dynamic Registration on Package Install

#### 2.1 Add function to register command after install
- [ ] Create helper function in `cmd_pkg.c` or `jshell_pkg_loader.c`
- [ ] Function: `pkg_register_installed_command(const char *name, const char *bin_path)`

**File:** `src/apps/pkg/cmd_pkg.c` (in `pkg_install` function)

**Current flow:**
```c
int pkg_install(const char *tarball) {
  // ... extract, validate, install ...
  pkg_db_add_full(name, version, description, files_str);
  pkg_db_save();
  printf("Installed %s version %s\n", name, version);
  return 0;
}
```

**Target flow:**
```c
int pkg_install(const char *tarball) {
  // ... extract, validate, install ...
  pkg_db_add_full(name, version, description, files_str);
  pkg_db_save();

  // Register command(s) in running shell
  // For each file in files_str that starts with "bin/":
  //   Extract command name (basename)
  //   Build bin_path: ~/.jshell/bin/<name>
  //   Call jshell_register_package_command(name, description, bin_path)

  printf("Installed %s version %s\n", name, version);
  return 0;
}
```

#### 2.2 Expose registration function to pkg command
- [ ] Since pkg is an external command (fork/exec), it cannot directly call shell functions
- [ ] **Solution A:** pkg writes to a file/signal that shell monitors (complex)
- [ ] **Solution B:** Shell re-reads database after pkg command completes (simpler)
- [ ] **Solution C:** Make pkg a builtin command (changes architecture)

**Recommended: Solution B** - After any `pkg` command execution, shell refreshes package registrations.

#### 2.3 Implement refresh mechanism
- [ ] Add `jshell_refresh_package_commands()` function
- [ ] Called after `pkg` command exits successfully
- [ ] Compares current registry against database, adds/removes as needed

**File:** `src/jshell/jshell_pkg_loader.c`

```c
// New function
int jshell_refresh_package_commands(void) {
  // 1. Get list of currently registered package commands
  // 2. Load database and get list of installed package commands
  // 3. Unregister commands that are no longer in database
  // 4. Register commands that are in database but not registered
  return 0;
}
```

#### 2.4 Hook refresh into command execution
- [ ] In `jshell_ast_helpers.c`, after executing a command named "pkg"
- [ ] Call `jshell_refresh_package_commands()`

**File:** `src/ast/jshell_ast_helpers.c`

```c
// In jshell_exec_single_cmd() or jshell_exec_package_cmd()
// After executing pkg command:
if (strcmp(cmd_name, "pkg") == 0) {
  jshell_refresh_package_commands();
}
```

---

### Phase 3: Dynamic Unregistration on Package Remove

#### 3.1 Ensure unregister works correctly
- [ ] Verify `jshell_unregister_command()` in `jshell_cmd_registry.c` works
- [ ] Already implemented but needs testing

#### 3.2 Refresh handles removal
- [ ] The refresh mechanism from Phase 2 should handle this automatically
- [ ] Commands removed from database will be unregistered on refresh

---

### Phase 4: Testing

#### 4.1 Manual testing scenarios
- [ ] Fresh shell start - only `pkg` and builtins available
- [ ] `pkg install ls` - `ls` command becomes available immediately
- [ ] `pkg remove ls` - `ls` command no longer available
- [ ] Shell restart - previously installed packages still work

#### 4.2 Update existing tests
- [ ] Tests that assume external commands exist may need updating
- [ ] Add setup step to install required packages first
- [ ] Or create test-specific initialization that registers commands

#### 4.3 New tests
- [ ] Test dynamic registration after install
- [ ] Test dynamic unregistration after remove
- [ ] Test shell startup with empty package database
- [ ] Test shell startup with populated package database

---

## File Changes Summary

| File | Change |
|------|--------|
| `src/jshell/jshell_register_externals.c` | Remove all registrations except pkg |
| `src/jshell/jshell_register_externals.h` | Remove unused declarations |
| `src/jshell/jshell_pkg_loader.c` | Add `jshell_refresh_package_commands()` |
| `src/jshell/jshell_pkg_loader.h` | Declare new refresh function |
| `src/ast/jshell_ast_helpers.c` | Call refresh after pkg command |
| `Makefile` | Remove cmd_*.c from JSHELL_SRCS (except pkg) |

---

## Dependencies and Considerations

### Build System
- External app binaries (ls, cat, etc.) still built as standalone apps
- They are NOT linked into jshell binary anymore
- pkg command and its dependencies still linked into jshell

### User Experience
- First-time users need to run `pkg install` for basic commands
- Could provide a "bootstrap" script or default package set
- Help command should indicate how to install missing commands

### Backwards Compatibility
- Existing installed packages will work (loaded from database)
- Users with empty database will need to install packages

### Error Handling
- When user runs uninstalled command: "command not found: ls (install with: pkg install ls)"
- Registry lookup already handles "not found" case

---

## Implementation Order

1. **Phase 1.1-1.2**: Modify registration files (quick, low risk)
2. **Phase 2.3**: Implement refresh mechanism (core functionality)
3. **Phase 2.4**: Hook into command execution (integration)
4. **Phase 1.3**: Update Makefile (build system)
5. **Phase 4**: Testing (validation)

---

## Status

- [x] Phase 1: Modify Static External Registration
  - [x] 1.1 Update jshell_register_externals.c
  - [x] 1.2 Update jshell_register_externals.h (already minimal)
  - [x] 1.3 Update Makefile
- [x] Phase 2: Dynamic Registration on Package Install
  - [x] 2.1 Add registration helper (already existed: jshell_reload_packages())
  - [x] 2.2 Choose and implement solution (B: shell re-reads database after pkg)
  - [x] 2.3 Implement refresh mechanism (already existed: jshell_reload_packages())
  - [x] 2.4 Hook into command execution (added in jshell_exec_job())
- [x] Phase 3: Dynamic Unregistration on Package Remove
  - [x] 3.1 Verify unregister works
  - [x] 3.2 Refresh handles removal (same mechanism handles both)
- [ ] Phase 4: Testing
  - [x] 4.1 Manual testing (verified install/remove dynamic updates)
  - [ ] 4.2 Update existing tests
  - [ ] 4.3 New tests
