# Package Manager Enhancement Implementation Plan

## Overview

Enhance the package manager (`pkg`) to support a complete package lifecycle including:
- JSON-based package database (`pkgdb.json`)
- Package update checking with user-friendly output
- Package upgrades with download, compilation, and installation
- Compilation of installed packages from source
- Shell integration tests using `jshell -c`

### Current State

The package manager has a working skeleton with:
- `pkg list/info/search/install/remove/build` - Basic operations working
- `pkg check-update/upgrade` - Implemented but needs refinement
- `pkg compile` - Compiles from `src/apps/`, not installed packages
- `~/.jshell/pkgdb.txt` - Plain text database (name version format)
- Package server at `src/pkg_srv/server.js`

### Target State

- `~/.jshell/pkgs/pkgdb.json` - JSON database with full package metadata
- `pkg check-update` - Clear "up to date" or "Update available: X.X.X → Y.Y.Y" messages
- `pkg upgrade` - Download to `~/.jshell/pkgs/_tmp/`, compile, install, symlink
- `pkg compile` - Compile installed packages (not just source)
- Updated app Makefiles for proper package building
- Comprehensive tests using `jshell -c`
- **Dynamic command registration**: Shell reads pkgdb.json on startup and registers installed packages as external commands that fork/exec from `~/.jshell/bin/`

---

## Implementation Progress

| Phase | Status | Commit | Tests |
|-------|--------|--------|-------|
| Phase 1: Test Infrastructure | ⬜ TODO | - | - |
| Phase 2: JSON Package Database | ⬜ TODO | - | - |
| Phase 3: Dynamic Shell Command Registration | ⬜ TODO | - | - |
| Phase 4: Package Check-Update Enhancement | ⬜ TODO | - | - |
| Phase 5: Package Upgrade Enhancement | ⬜ TODO | - | - |
| Phase 6: Package Compile for Installed Packages | ⬜ TODO | - | - |
| Phase 7: Makefile Updates | ⬜ TODO | - | - |
| Phase 8: Integration Tests | ⬜ TODO | - | - |

---

## Phase 1: Test Infrastructure

Create test infrastructure for testing pkg commands via `jshell -c`.

### 1.1 Create Shell Integration Test Helper
**File**: `tests/helpers/shell_runner.py`

- [ ] Create helper class for running `jshell -c` commands:
  ```python
  class ShellRunner:
      JSHELL = Path(__file__).parent.parent.parent / "bin" / "jshell"

      @classmethod
      def run(cls, command: str, env: dict = None, timeout: float = 30.0):
          """Run command via jshell -c and return result."""

      @classmethod
      def run_script(cls, script: str, timeout: float = 30.0):
          """Run multi-line script via jshell -c."""
  ```

### 1.2 Create Package Manager Test Base
**File**: `tests/pkg/test_pkg_base.py`

- [ ] Create base test class with common setup/teardown:
  - [ ] Backup and restore `~/.jshell` directory
  - [ ] Helper to create test package tarballs
  - [ ] Helper to start/stop package registry server
  - [ ] Helper to populate test registry with packages

### 1.3 Update Test Helpers __init__.py
**File**: `tests/helpers/__init__.py`

- [ ] Export `ShellRunner` class
- [ ] Add any common test utilities

---

## Phase 2: JSON Package Database

Convert the package database from plain text (`pkgdb.txt`) to JSON format (`pkgdb.json`).

### 2.1 Design pkgdb.json Schema
**File**: `~/.jshell/pkgs/pkgdb.json`

Schema:
```json
{
  "version": 1,
  "packages": [
    {
      "name": "ls",
      "version": "0.0.1",
      "installed_at": "2024-01-15T10:30:00Z",
      "files": ["bin/ls"],
      "description": "list directory contents"
    }
  ]
}
```

### 2.2 Update pkg_db.h
**File**: `src/apps/pkg/pkg_db.h`

- [ ] Add new fields to `PkgDbEntry`:
  ```c
  typedef struct {
    char *name;
    char *version;
    char *installed_at;      // ISO 8601 timestamp
    char *description;       // Optional description
    char **files;            // Array of installed files
    int files_count;
  } PkgDbEntry;
  ```

- [ ] Add JSON-specific functions:
  ```c
  PkgDb *pkg_db_load_json(void);
  int pkg_db_save_json(const PkgDb *db);
  int pkg_db_migrate_from_txt(void);  // One-time migration
  ```

### 2.3 Update pkg_db.c
**File**: `src/apps/pkg/pkg_db.c`

- [ ] Implement `pkg_db_load_json()`:
  - [ ] Read `~/.jshell/pkgs/pkgdb.json`
  - [ ] Parse JSON using existing `pkg_json.c` utilities
  - [ ] Fallback to `pkgdb.txt` if JSON doesn't exist
  - [ ] Auto-migrate from txt to JSON on first load

- [ ] Implement `pkg_db_save_json()`:
  - [ ] Write proper JSON format
  - [ ] Include all metadata fields
  - [ ] Create `~/.jshell/pkgs/` directory if needed

- [ ] Implement `pkg_db_migrate_from_txt()`:
  - [ ] Read existing `pkgdb.txt`
  - [ ] Convert entries to new format
  - [ ] Add default values for new fields
  - [ ] Write `pkgdb.json`
  - [ ] Keep backup of `pkgdb.txt`

- [ ] Update `pkg_db_load()` to use JSON:
  - [ ] Call `pkg_db_load_json()` internally
  - [ ] Handle migration transparently

- [ ] Update `pkg_db_save()` to use JSON:
  - [ ] Call `pkg_db_save_json()` internally

- [ ] Update `pkg_db_add()`:
  - [ ] Accept additional metadata (files, description)
  - [ ] Set `installed_at` timestamp

### 2.4 Update pkg_utils.h/c
**File**: `src/apps/pkg/pkg_utils.h`, `src/apps/pkg/pkg_utils.c`

- [ ] Update `pkg_get_db_path()`:
  - [ ] Return path to `pkgdb.json` instead of `pkgdb.txt`

- [ ] Add `pkg_get_tmp_dir()`:
  - [ ] Return `~/.jshell/pkgs/_tmp/`
  - [ ] Create directory if it doesn't exist

### 2.5 Create Database Tests
**File**: `tests/pkg/test_pkg_db.py`

- [ ] Test JSON database creation
- [ ] Test loading/saving entries
- [ ] Test migration from txt to JSON
- [ ] Test database integrity after operations

---

## Phase 3: Dynamic Shell Command Registration

Remove static external command registration and instead dynamically register commands from installed packages at shell startup.

### 3.1 Modify Command Registry Architecture
**File**: `src/jshell/jshell_cmd_registry.h`

- [ ] Add new command type for package commands:
  ```c
  typedef enum {
    CMD_BUILTIN,       // Run in shell process (call run function)
    CMD_EXTERNAL,      // Compiled into shell, call run function
    CMD_PACKAGE        // Fork/exec binary from ~/.jshell/bin/
  } jshell_cmd_type_t;
  ```

- [ ] Add binary path field to command spec:
  ```c
  typedef struct jshell_cmd_spec {
    const char *name;
    const char *summary;
    const char *long_help;
    jshell_cmd_type_t type;
    int (*run)(int argc, char **argv);      // NULL for CMD_PACKAGE
    void (*print_usage)(FILE *out);         // NULL for CMD_PACKAGE
    const char *bin_path;                   // Path to binary for CMD_PACKAGE
  } jshell_cmd_spec_t;
  ```

- [ ] Add function to register package commands:
  ```c
  void jshell_register_package_command(const char *name,
                                        const char *summary,
                                        const char *bin_path);
  ```

- [ ] Add function to unregister a command:
  ```c
  int jshell_unregister_command(const char *name);
  ```

### 3.2 Update Command Registry Implementation
**File**: `src/jshell/jshell_cmd_registry.c`

- [ ] Implement `jshell_register_package_command()`:
  - [ ] Allocate new `jshell_cmd_spec_t`
  - [ ] Set `type = CMD_PACKAGE`
  - [ ] Set `run = NULL`, `print_usage = NULL`
  - [ ] Set `bin_path` to the binary path
  - [ ] Add to registry

- [ ] Implement `jshell_unregister_command()`:
  - [ ] Find command by name
  - [ ] Remove from registry
  - [ ] Free allocated memory for package commands

- [ ] Update `jshell_for_each_command()` to handle package commands

### 3.3 Update Command Execution
**File**: `src/ast/jshell_ast_helpers.c`

- [ ] Modify `jshell_exec_builtin()` or command dispatch:
  - [ ] Check command type
  - [ ] For `CMD_PACKAGE`: fork/exec the binary from `bin_path`
  - [ ] For `CMD_BUILTIN`/`CMD_EXTERNAL`: call `run()` function as before

- [ ] Create `jshell_exec_package_cmd()`:
  ```c
  int jshell_exec_package_cmd(const jshell_cmd_spec_t *spec,
                               int argc, char **argv);
  ```
  - [ ] Fork process
  - [ ] In child: `execv(spec->bin_path, argv)`
  - [ ] In parent: wait for child and return exit status

### 3.4 Remove Static External Command Registration
**File**: `src/jshell/jshell_register_externals.c`

- [ ] Remove all `#include` directives for external apps
- [ ] Empty `jshell_register_all_external_commands()`:
  ```c
  void jshell_register_all_external_commands(void) {
    // External commands are now registered dynamically
    // from pkgdb.json at shell startup
  }
  ```

### 3.5 Create Package Command Loader
**File**: `src/jshell/jshell_pkg_loader.h`, `src/jshell/jshell_pkg_loader.c`

- [ ] Create `jshell_pkg_loader.h`:
  ```c
  #ifndef JSHELL_PKG_LOADER_H
  #define JSHELL_PKG_LOADER_H

  // Load installed packages from pkgdb.json and register commands
  int jshell_load_installed_packages(void);

  // Reload packages (after install/remove)
  int jshell_reload_packages(void);

  // Get path to jshell home directory
  char *jshell_get_home_dir(void);

  // Get path to jshell bin directory
  char *jshell_get_bin_dir(void);

  #endif
  ```

- [ ] Create `jshell_pkg_loader.c`:
  - [ ] Implement `jshell_load_installed_packages()`:
    - [ ] Get path to `~/.jshell/pkgs/pkgdb.json`
    - [ ] Read and parse JSON file
    - [ ] For each installed package:
      - [ ] Get package name and files list
      - [ ] For each binary in files (e.g., `bin/ls`):
        - [ ] Construct full path: `~/.jshell/bin/<binary>`
        - [ ] Call `jshell_register_package_command()`
    - [ ] Return number of packages loaded

  - [ ] Implement `jshell_reload_packages()`:
    - [ ] Unregister all `CMD_PACKAGE` commands
    - [ ] Call `jshell_load_installed_packages()`

  - [ ] Implement helper functions for paths

### 3.6 Update Shell Initialization
**File**: `src/jshell/jshell.c`

- [ ] Add `#include "jshell_pkg_loader.h"`
- [ ] Call `jshell_load_installed_packages()` after `jshell_register_all_builtin_commands()`:
  ```c
  void jshell_init(void) {
    jshell_register_all_builtin_commands();
    // External registry is now empty
    jshell_register_all_external_commands();
    // Load packages from ~/.jshell/pkgs/pkgdb.json
    jshell_load_installed_packages();
  }
  ```

### 3.7 Update pkg install/remove to Reload Commands
**File**: `src/apps/pkg/cmd_pkg.c`

- [ ] After successful `pkg install`:
  - [ ] Note: Since pkg is an external process, it cannot directly call `jshell_reload_packages()`
  - [ ] Option 1: Shell re-reads pkgdb.json before each command lookup
  - [ ] Option 2: Shell checks pkgdb.json mtime and reloads if changed
  - [ ] Option 3: User runs `hash -r` or similar to refresh (simpler)

- [ ] Document that new packages are available after shell restart or `hash -r`

### 3.8 Update Makefile
**File**: `Makefile`

- [ ] Add `jshell_pkg_loader.c` to `JSHELL_SRCS`
- [ ] Remove external app objects from jbox build (they're now standalone only)
- [ ] Update dependencies

### 3.9 Create Dynamic Registration Tests
**File**: `tests/pkg/test_pkg_registration.py`

- [ ] Test shell starts with empty external registry
- [ ] Test package commands available after install
- [ ] Test command execution forks correct binary
- [ ] Test package commands unavailable after remove
- [ ] Test shell restart loads packages

---

## Phase 4: Package Check-Update Enhancement

Improve `pkg check-update` output to be more user-friendly.

### 4.1 Update pkg_check_update Function
**File**: `src/apps/pkg/cmd_pkg.c`

- [ ] Modify `pkg_check_update()` output format:
  - [ ] For each installed package:
    - [ ] If up to date: `"ls is up to date (0.0.1)"`
    - [ ] If update available: `"Update available for ls: 0.0.1 → 0.0.2"`
  - [ ] Summary at end: `"2 packages up to date, 1 update available"`

- [ ] Improve JSON output:
  ```json
  {
    "status": "ok",
    "summary": {
      "up_to_date": 2,
      "updates_available": 1,
      "errors": 0
    },
    "packages": [
      {
        "name": "ls",
        "installed": "0.0.1",
        "available": "0.0.1",
        "status": "up_to_date"
      },
      {
        "name": "cat",
        "installed": "0.0.1",
        "available": "0.0.2",
        "status": "update_available"
      }
    ]
  }
  ```

### 4.2 Create Check-Update Tests
**File**: `tests/pkg/test_pkg_check_update.py`

- [ ] Test with no packages installed
- [ ] Test with up-to-date packages
- [ ] Test with outdated packages
- [ ] Test with registry unavailable
- [ ] Test JSON output format
- [ ] Test via `jshell -c "pkg check-update"`

---

## Phase 5: Package Upgrade Enhancement

Enhance `pkg upgrade` to use proper temp directory and compile before install.

### 5.1 Update Directory Structure
**File**: `src/apps/pkg/pkg_utils.c`

- [ ] Add `pkg_ensure_tmp_dir()`:
  - [ ] Create `~/.jshell/pkgs/_tmp/` if it doesn't exist
  - [ ] Clean up old temp files on startup

- [ ] Add `pkg_cleanup_tmp_dir()`:
  - [ ] Remove contents of `_tmp/` directory
  - [ ] Called after successful install

### 5.2 Update pkg_upgrade Function
**File**: `src/apps/pkg/cmd_pkg.c`

- [ ] Modify upgrade flow:
  1. [ ] Check for updates (existing)
  2. [ ] Download tarball to `~/.jshell/pkgs/_tmp/<name>-<version>.tar.gz`
  3. [ ] Extract to `~/.jshell/pkgs/_tmp/<name>-<version>/`
  4. [ ] If package has source code, compile with `pkg_compile_package()`
  5. [ ] Remove old version
  6. [ ] Install new version
  7. [ ] Cleanup temp files

- [ ] Add progress output:
  ```
  Checking for updates...
  Downloading cat 0.0.2...
  Compiling cat...
  Installing cat 0.0.2...
  Upgraded cat: 0.0.1 → 0.0.2
  ```

- [ ] Improve JSON output:
  ```json
  {
    "status": "ok",
    "upgraded": [
      {
        "name": "cat",
        "from": "0.0.1",
        "to": "0.0.2",
        "status": "ok"
      }
    ],
    "failed": [],
    "up_to_date": ["ls", "head"]
  }
  ```

### 5.3 Create Upgrade Tests
**File**: `tests/pkg/test_pkg_upgrade.py`

- [ ] Test upgrade with no packages
- [ ] Test upgrade with all up-to-date
- [ ] Test upgrade with outdated package
- [ ] Test upgrade with download failure
- [ ] Test upgrade with compile failure
- [ ] Test cleanup after upgrade
- [ ] Test via `jshell -c "pkg upgrade"`

---

## Phase 6: Package Compile for Installed Packages

Modify `pkg compile` to work with installed packages, not just source.

### 6.1 Design Package Source Structure

Installed packages should contain source code for compilation:
```
~/.jshell/pkgs/<name>-<version>/
├── pkg.json
├── src/           # Source code (if compilable)
│   ├── cmd_<name>.c
│   ├── cmd_<name>.h
│   └── <name>_main.c
├── bin/           # Compiled binaries
│   └── <name>
└── Makefile       # Build instructions (optional)
```

### 6.2 Update pkg_compile Function
**File**: `src/apps/pkg/cmd_pkg.c`

- [ ] Modify `pkg_compile()` to handle installed packages:
  - [ ] If name provided, look in `~/.jshell/pkgs/<name>-<version>/`
  - [ ] If no name, compile all installed packages with source
  - [ ] Check for Makefile or src/ directory
  - [ ] Run compilation
  - [ ] Place binary in `bin/` subdirectory
  - [ ] Update symlink in `~/.jshell/bin/`

- [ ] Add `pkg_compile_package()` helper:
  ```c
  int pkg_compile_package(const char *pkg_path, int json_output);
  ```

- [ ] Output format:
  ```
  Compiling cat...
    Source: ~/.jshell/pkgs/cat-0.0.2/src/
    Output: ~/.jshell/pkgs/cat-0.0.2/bin/cat
  Compiled 1 package successfully.
  ```

### 6.3 Create Compile Tests
**File**: `tests/pkg/test_pkg_compile.py`

- [ ] Test compile with no installed packages
- [ ] Test compile specific package
- [ ] Test compile all packages
- [ ] Test compile with missing source
- [ ] Test compile with build error
- [ ] Test binary is executable after compile
- [ ] Test symlink is updated
- [ ] Test via `jshell -c "pkg compile <name>"`

---

## Phase 7: Makefile Updates

Update app Makefiles to build packages properly for the new format.

### 7.1 Update pkg.mk Shared Rules
**File**: `src/apps/pkg.mk`

- [ ] Update package staging structure:
  ```makefile
  pkg: $(PKG_BIN) $(PKG_JSON)
      @echo "Building package $(PKG_NAME)-$(PKG_VERSION)..."
      @mkdir -p $(PKG_STAGING)/bin
      @mkdir -p $(PKG_STAGING)/src
      @cp $(PKG_BIN) $(PKG_STAGING)/bin/$(PKG_NAME)
      @cp $(PKG_JSON) $(PKG_STAGING)/
      # Include source files for recompilation
      @cp *.c *.h $(PKG_STAGING)/src/ 2>/dev/null || true
      @cp Makefile $(PKG_STAGING)/ 2>/dev/null || true
      @mkdir -p $(PKG_REPO_DIR)
      @tar -czf $(PKG_DEST) -C $(PKG_STAGING) .
      @rm -rf $(PKG_STAGING)
      @echo "  Created: $(PKG_DEST)"
  ```

- [ ] Add `pkg-src` target:
  - [ ] Include source files in package
  - [ ] Include Makefile for compilation

### 7.2 Update Individual App Makefiles

For each app in `src/apps/`, ensure the Makefile:
- [ ] Defines `PKG_SRCS` variable with source files
- [ ] Includes proper compilation rules
- [ ] Can be invoked from installed package directory

Apps to update:
- [ ] `src/apps/cat/Makefile`
- [ ] `src/apps/cp/Makefile`
- [ ] `src/apps/date/Makefile`
- [ ] `src/apps/echo/Makefile`
- [ ] `src/apps/head/Makefile`
- [ ] `src/apps/less/Makefile`
- [ ] `src/apps/ls/Makefile`
- [ ] `src/apps/mkdir/Makefile`
- [ ] `src/apps/mv/Makefile`
- [ ] `src/apps/rg/Makefile`
- [ ] `src/apps/rm/Makefile`
- [ ] `src/apps/rmdir/Makefile`
- [ ] `src/apps/sleep/Makefile`
- [ ] `src/apps/stat/Makefile`
- [ ] `src/apps/tail/Makefile`
- [ ] `src/apps/touch/Makefile`
- [ ] `src/apps/vi/Makefile`

### 7.3 Update pkg.json for Each App

For each app, update `pkg.json` to include source files:
```json
{
  "name": "ls",
  "version": "0.0.2",
  "description": "list directory contents",
  "files": ["bin/ls"],
  "sources": ["src/cmd_ls.c", "src/cmd_ls.h", "src/ls_main.c"],
  "build": "make -C src"
}
```

Apps to update:
- [ ] All 17 apps in `src/apps/*/pkg.json`

---

## Phase 8: Integration Tests

Create comprehensive integration tests using `jshell -c`.

### 8.1 Create End-to-End Package Lifecycle Tests
**File**: `tests/pkg/test_pkg_lifecycle.py`

- [ ] Test complete package lifecycle:
  ```python
  def test_full_lifecycle(self):
      # 1. Start with clean slate
      # 2. Search for package
      result = ShellRunner.run('pkg search ls')
      assert 'ls' in result.stdout

      # 3. Install package
      result = ShellRunner.run('pkg install ls-0.0.1.tar.gz')
      assert result.returncode == 0

      # 4. List shows package
      result = ShellRunner.run('pkg list')
      assert 'ls' in result.stdout

      # 5. Info shows details
      result = ShellRunner.run('pkg info ls')
      assert 'version' in result.stdout

      # 6. Check update
      result = ShellRunner.run('pkg check-update')
      assert 'up to date' in result.stdout

      # 7. Remove package
      result = ShellRunner.run('pkg remove ls')
      assert result.returncode == 0
  ```

### 8.2 Create Multi-Package Tests
**File**: `tests/pkg/test_pkg_multi.py`

- [ ] Test installing multiple packages
- [ ] Test upgrading multiple packages
- [ ] Test removing multiple packages
- [ ] Test dependency handling (future)

### 8.3 Create Error Handling Tests
**File**: `tests/pkg/test_pkg_errors.py`

- [ ] Test install of non-existent package
- [ ] Test install of corrupted tarball
- [ ] Test upgrade with network failure
- [ ] Test compile with missing dependencies
- [ ] Test remove of non-existent package

### 8.4 Create Shell Pipeline Tests
**File**: `tests/pkg/test_pkg_shell.py`

- [ ] Test `pkg list --json | jq .packages[0].name`
- [ ] Test `pkg search ls --json | jq '.results | length'`
- [ ] Test combining pkg with other shell features

### 8.5 Update tests/Makefile
**File**: `tests/Makefile`

- [ ] Add `pkg-integration` target:
  ```makefile
  pkg-integration:
      cd $(PROJECT_ROOT) && $(PYTHON) -m unittest \
          tests.pkg.test_pkg_lifecycle \
          tests.pkg.test_pkg_multi \
          tests.pkg.test_pkg_errors \
          tests.pkg.test_pkg_shell -v
  ```

- [ ] Add individual test targets:
  - [ ] `pkg-db-tests`
  - [ ] `pkg-check-update-tests`
  - [ ] `pkg-upgrade-tests`
  - [ ] `pkg-compile-tests`
  - [ ] `pkg-lifecycle-tests`

---

## Implementation Order

### Sprint 1: Foundation (Phase 1-2)
1. Create test infrastructure
2. Implement JSON package database
3. Write database tests

### Sprint 2: Dynamic Registration (Phase 3)
1. Modify command registry architecture
2. Create package command loader
3. Remove static external command registration
4. Update shell initialization
5. Write registration tests

### Sprint 3: Core Features (Phase 4-5)
1. Enhance check-update output
2. Enhance upgrade flow
3. Write check-update and upgrade tests

### Sprint 4: Compilation (Phase 6)
1. Implement package compile for installed packages
2. Write compile tests
3. Test end-to-end compile flow

### Sprint 5: Build System (Phase 7)
1. Update pkg.mk
2. Update individual app Makefiles
3. Update pkg.json files
4. Test package building

### Sprint 6: Integration (Phase 8)
1. Create lifecycle tests
2. Create error handling tests
3. Create shell pipeline tests
4. Full integration testing

---

## File Reference

### New Files to Create

```
# Test Infrastructure
tests/helpers/shell_runner.py       # Shell command runner helper
tests/pkg/__init__.py               # Package tests module
tests/pkg/test_pkg_base.py          # Base test class
tests/pkg/test_pkg_db.py            # Database tests
tests/pkg/test_pkg_registration.py  # Dynamic registration tests
tests/pkg/test_pkg_check_update.py  # Check-update tests
tests/pkg/test_pkg_upgrade.py       # Upgrade tests
tests/pkg/test_pkg_compile.py       # Compile tests
tests/pkg/test_pkg_lifecycle.py     # End-to-end tests
tests/pkg/test_pkg_multi.py         # Multi-package tests
tests/pkg/test_pkg_errors.py        # Error handling tests
tests/pkg/test_pkg_shell.py         # Shell integration tests

# Dynamic Command Registration
src/jshell/jshell_pkg_loader.h      # Package loader header
src/jshell/jshell_pkg_loader.c      # Package loader implementation
```

### Files to Modify

```
# Command Registry
src/jshell/jshell_cmd_registry.h    # Add CMD_PACKAGE type, bin_path field
src/jshell/jshell_cmd_registry.c    # Implement package command registration
src/jshell/jshell_register_externals.c  # Empty external registration
src/jshell/jshell.c                 # Call package loader on startup
src/ast/jshell_ast_helpers.c        # Handle CMD_PACKAGE execution

# Package Manager
src/apps/pkg/pkg_db.h               # Add JSON support
src/apps/pkg/pkg_db.c               # Implement JSON database
src/apps/pkg/pkg_utils.h            # Add tmp dir functions
src/apps/pkg/pkg_utils.c            # Implement tmp dir functions
src/apps/pkg/cmd_pkg.c              # Enhance check-update, upgrade, compile

# Build System
Makefile                            # Add jshell_pkg_loader.c, remove external app objects
src/apps/pkg.mk                     # Update package build rules
src/apps/*/Makefile                 # Update 17 app Makefiles
src/apps/*/pkg.json                 # Update 17 app manifests
tests/helpers/__init__.py           # Export new helpers
tests/Makefile                      # Add new test targets
```

---

## Notes

### Dynamic Command Registration

The shell command registry architecture changes:
- **Empty at startup**: No external commands are compiled into jbox
- **pkgdb.json driven**: Shell reads `~/.jshell/pkgs/pkgdb.json` on startup
- **Fork/exec execution**: Package commands fork and exec binaries from `~/.jshell/bin/`
- **No function pointers**: `CMD_PACKAGE` commands have `run = NULL`, use `bin_path` instead
- **Command refresh**: New packages available after shell restart (or `hash -r` if implemented)

This separation means:
- The jbox binary is smaller (no external app code linked in)
- Packages are truly standalone binaries
- Users can install/remove packages without rebuilding jbox
- The shell only needs the command name and binary path

### Database Migration

The migration from `pkgdb.txt` to `pkgdb.json` should be:
- Automatic on first load
- Non-destructive (keep backup of txt file)
- Transparent to existing code

### Package Compilation

Packages can be:
1. **Pre-compiled**: Binary only, no source included
2. **Source-included**: Source + pre-compiled binary
3. **Source-only**: Requires compilation on install

The default should be source-included for maximum flexibility.

### Testing Strategy

1. **Unit tests**: Test individual functions in isolation
2. **Integration tests**: Test via `jshell -c`
3. **End-to-end tests**: Test complete workflows

Use the package registry server for tests that require network access.

---

## Checklist Summary

- [ ] Phase 1: Test Infrastructure (3 tasks)
- [ ] Phase 2: JSON Package Database (5 tasks)
- [ ] Phase 3: Dynamic Shell Command Registration (9 tasks)
- [ ] Phase 4: Package Check-Update Enhancement (2 tasks)
- [ ] Phase 5: Package Upgrade Enhancement (3 tasks)
- [ ] Phase 6: Package Compile for Installed Packages (3 tasks)
- [ ] Phase 7: Makefile Updates (3 tasks)
- [ ] Phase 8: Integration Tests (5 tasks)

**Total: 33 major tasks**
