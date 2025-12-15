# Package Manager (pkg) Implementation Plan

## Overview

Implement a minimal package manager for jshell. Packages are `.tar.gz` archives containing a `pkg.json` manifest plus files to install.

**Prerequisites:** The pkg command skeleton was implemented in `AI_TODO_1.md` Phase 9.1. This plan builds on that foundation.

### Directory Structure

- **Packages install to:** `~/.jshell/pkgs/<name>-<version>/`
- **Executables symlinked to:** `~/.jshell/bin/`
- **Package database:** `~/.jshell/pkgdb.txt`
- **Source code:** `src/apps/pkg/`

### pkg.json Format

```json
{
  "name": "example",
  "version": "1.0.0",
  "description": "Example package",
  "files": ["bin/example", "bin/helper"],
  "docs": ["README.md", "docs/usage.md"]
}
```

Fields:
- `name` (required): Package name (alphanumeric, hyphens, underscores)
- `version` (required): Semantic version string
- `description` (optional): One-line description
- `files` (required): Array of executable paths relative to package root
- `docs` (optional): Array of documentation file paths

### Standards

- Use GNU C23 standard
- Use `argtable3` for CLI parsing
- Use only standard C and POSIX APIs
- Use `fork`/`exec` for external programs (tar, make)
- Support `--json` output for all subcommands
- Support `-h` / `--help`

---

## Phase 1: Core Infrastructure ✅ COMPLETED

### 1.1 Create pkg Helper Module ✅ COMPLETED
Create utility functions for common pkg operations.

- [x] Create `src/apps/pkg/pkg_utils.h`:
  - [x] Declare `char *pkg_get_home_dir(void);` - returns ~/.jshell path
  - [x] Declare `char *pkg_get_pkgs_dir(void);` - returns ~/.jshell/pkgs path
  - [x] Declare `char *pkg_get_bin_dir(void);` - returns ~/.jshell/bin path
  - [x] Declare `char *pkg_get_db_path(void);` - returns ~/.jshell/pkgdb.txt path
  - [x] Declare `int pkg_ensure_dirs(void);` - creates directories if needed
  - [x] Declare `int pkg_run_command(char *const argv[]);` - fork/exec wrapper

- [x] Create `src/apps/pkg/pkg_utils.c`:
  - [x] Implement `pkg_get_home_dir()`
  - [x] Implement `pkg_get_pkgs_dir()`
  - [x] Implement `pkg_get_bin_dir()`
  - [x] Implement `pkg_get_db_path()`
  - [x] Implement `pkg_ensure_dirs()`
  - [x] Implement `pkg_run_command(char *const argv[])`
  - [x] Implement `pkg_remove_dir_recursive()` (bonus)
  - [x] Implement `pkg_read_file()` (bonus)

### 1.2 Create pkg.json Parser Module ✅ COMPLETED
Parse and validate pkg.json files.

- [x] Create `src/apps/pkg/pkg_json.h`:
  - [x] Define `PkgManifest` struct
  - [x] Declare `PkgManifest *pkg_manifest_parse(const char *json_str);`
  - [x] Declare `PkgManifest *pkg_manifest_load(const char *path);`
  - [x] Declare `void pkg_manifest_free(PkgManifest *m);`
  - [x] Declare `int pkg_manifest_validate(const PkgManifest *m);`

- [x] Create `src/apps/pkg/pkg_json.c`:
  - [x] Implement minimal JSON parser
  - [x] Implement `pkg_manifest_parse()`
  - [x] Implement `pkg_manifest_load()`
  - [x] Implement `pkg_manifest_free()`
  - [x] Implement `pkg_manifest_validate()`

### 1.3 Create Package Database Module ✅ COMPLETED
Manage the installed packages database.

- [x] Create `src/apps/pkg/pkg_db.h`:
  - [x] Define `PkgDbEntry` struct
  - [x] Define `PkgDb` struct
  - [x] Declare all database functions

- [x] Create `src/apps/pkg/pkg_db.c`:
  - [x] Implement `pkg_db_load()`
  - [x] Implement `pkg_db_save()`
  - [x] Implement `pkg_db_free()`
  - [x] Implement `pkg_db_find()`
  - [x] Implement `pkg_db_add()`
  - [x] Implement `pkg_db_remove()`

---

## Phase 2: pkg build ✅ COMPLETED

### 2.1 Implement pkg build Subcommand ✅ COMPLETED
Build a package tarball from a source directory.

- [x] Update `pkg_build()` signature in `cmd_pkg.c`
- [x] Implement `pkg_build()`:
  - [x] Validate `src_dir` exists and is a directory
  - [x] Check `pkg.json` exists in `src_dir`
  - [x] Load and validate manifest
  - [x] Verify all files listed in manifest exist
  - [x] Run tar command via fork/exec
  - [x] Output result (human and JSON)
  - [x] Handle all error cases

- [x] Update `pkg_print_usage()`
- [x] Create tests in `tests/apps/pkg/test_pkg.py` (consolidated)

---

## Phase 3: pkg install ✅ COMPLETED

### 3.1 Implement pkg install Subcommand ✅ COMPLETED
Install a package from a tarball.

- [x] Implement `pkg_install()`:
  - [x] Validate tarball exists
  - [x] Ensure directories exist with `pkg_ensure_dirs()`
  - [x] Create temp directory for extraction
  - [x] Extract tarball via fork/exec tar
  - [x] Load and validate `pkg.json` from extracted contents
  - [x] Check if package already installed
  - [x] Move to install path: `~/.jshell/pkgs/<name>-<version>/`
  - [x] Create symlinks in `~/.jshell/bin/`
  - [x] Update package database
  - [x] Clean up temp directory
  - [x] Output result (human and JSON)
  - [x] Handle all error cases

- [x] Update `pkg_print_usage()`
- [x] Create tests in `tests/apps/pkg/test_pkg.py` (consolidated)

---

## Phase 4: pkg remove ✅ COMPLETED

### 4.1 Implement pkg remove Subcommand ✅ COMPLETED
Remove an installed package.

- [x] Implement `pkg_remove()`:
  - [x] Load package database
  - [x] Find package by name
  - [x] Handle "not found" error
  - [x] Load `pkg.json` from package directory
  - [x] Remove symlinks from `~/.jshell/bin/`
  - [x] Remove package directory recursively
  - [x] Update and save package database
  - [x] Output result (human and JSON)
  - [x] Handle all error cases

- [x] Update `pkg_print_usage()`
- [x] Create tests in `tests/apps/pkg/test_pkg.py` (consolidated)

---

## Phase 5: pkg list ✅ COMPLETED

### 5.1 Implement pkg list Subcommand ✅ COMPLETED
List all installed packages.

- [x] Implement `pkg_list()`:
  - [x] Load package database
  - [x] Handle empty list case
  - [x] Output result (human and JSON)

- [x] Create tests in `tests/apps/pkg/test_pkg.py` (consolidated)

---

## Phase 6: pkg info ✅ COMPLETED

### 6.1 Implement pkg info Subcommand ✅ COMPLETED
Show detailed information about an installed package.

- [x] Implement `pkg_info()`:
  - [x] Load package database
  - [x] Find package by name
  - [x] Handle "not found" error
  - [x] Load `pkg.json` from package directory
  - [x] Output result (human and JSON)

- [x] Create tests in `tests/apps/pkg/test_pkg.py` (consolidated)

---

## Phase 7: pkg compile ✅ COMPLETED

### 7.1 Implement pkg compile Subcommand ✅ COMPLETED
Compile all apps in src/apps/ directory.

- [x] Implement `pkg_compile()`:
  - [x] Determine apps directory from cwd
  - [x] Support optional `app_name` argument
  - [x] Iterate subdirectories and run make
  - [x] Track success/failure counts
  - [x] Output result (human and JSON)

- [x] Update `pkg_print_usage()`

---

## Phase 8: Update Makefiles ✅ COMPLETED

### 8.1 Update pkg Makefile ✅ COMPLETED
Add new source files to pkg build.

- [x] Update `src/apps/pkg/Makefile`:
  - [x] Add `pkg_utils.o` to OBJS
  - [x] Add `pkg_json.o` to OBJS
  - [x] Add `pkg_db.o` to OBJS
  - [x] Add dependency rules for new files

---

## Phase 9: Future Subcommands (DEFERRED)

These subcommands require a remote package registry and are deferred:

### 9.1 pkg search (FUTURE)
- [ ] Connect to remote package registry
- [ ] Search and filter packages
- [ ] Output results in human and JSON format

### 9.2 pkg check-update (FUTURE)
- [ ] Compare installed versions with registry
- [ ] List available updates

### 9.3 pkg upgrade (FUTURE)
- [ ] Download and install updates from registry

---

## Notes

### Error Handling
All operations should:
- Return 0 on success, non-zero on failure
- Print human-readable errors to stderr
- Print JSON errors with `{"status": "error", "message": "..."}` format

### File Paths
- Always expand `~` to actual home directory
- Use absolute paths internally
- Display relative paths with `~` prefix for human output

### Cleanup
- Always clean up temporary files/directories on error
- Use signal handlers if necessary to clean up on SIGINT

### Testing
Each subcommand should have comprehensive tests:
- Normal operation
- Error conditions
- Edge cases
- JSON output validation

### Command Checklist
For each implemented subcommand:
- [ ] Implementation in `cmd_pkg.c`
- [ ] Usage documented in `pkg_print_usage()`
- [ ] Human-readable output
- [ ] JSON output with `--json`
- [ ] Error handling
- [ ] Unit tests
