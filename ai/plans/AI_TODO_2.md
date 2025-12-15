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

## Phase 1: Core Infrastructure

### 1.1 Create pkg Helper Module
Create utility functions for common pkg operations.

- [ ] Create `src/apps/pkg/pkg_utils.h`:
  - [ ] Declare `char *pkg_get_home_dir(void);` - returns ~/.jshell path
  - [ ] Declare `char *pkg_get_pkgs_dir(void);` - returns ~/.jshell/pkgs path
  - [ ] Declare `char *pkg_get_bin_dir(void);` - returns ~/.jshell/bin path
  - [ ] Declare `char *pkg_get_db_path(void);` - returns ~/.jshell/pkgdb.txt path
  - [ ] Declare `int pkg_ensure_dirs(void);` - creates directories if needed
  - [ ] Declare `int pkg_run_command(char *const argv[]);` - fork/exec wrapper

- [ ] Create `src/apps/pkg/pkg_utils.c`:
  - [ ] Implement `pkg_get_home_dir()`:
    - [ ] Get HOME environment variable
    - [ ] Append `/.jshell`
    - [ ] Return allocated string (caller frees)
  - [ ] Implement `pkg_get_pkgs_dir()`:
    - [ ] Return `~/.jshell/pkgs`
  - [ ] Implement `pkg_get_bin_dir()`:
    - [ ] Return `~/.jshell/bin`
  - [ ] Implement `pkg_get_db_path()`:
    - [ ] Return `~/.jshell/pkgdb.txt`
  - [ ] Implement `pkg_ensure_dirs()`:
    - [ ] Create `~/.jshell` if not exists
    - [ ] Create `~/.jshell/pkgs` if not exists
    - [ ] Create `~/.jshell/bin` if not exists
    - [ ] Return 0 on success, -1 on error
  - [ ] Implement `pkg_run_command(char *const argv[])`:
    - [ ] Fork child process
    - [ ] In child: `execvp(argv[0], argv)`
    - [ ] In parent: `waitpid()` and return exit status

### 1.2 Create pkg.json Parser Module
Parse and validate pkg.json files.

- [ ] Create `src/apps/pkg/pkg_json.h`:
  - [ ] Define `PkgManifest` struct:
    ```c
    typedef struct {
      char *name;
      char *version;
      char *description;
      char **files;
      int files_count;
      char **docs;
      int docs_count;
    } PkgManifest;
    ```
  - [ ] Declare `PkgManifest *pkg_manifest_parse(const char *json_str);`
  - [ ] Declare `PkgManifest *pkg_manifest_load(const char *path);`
  - [ ] Declare `void pkg_manifest_free(PkgManifest *m);`
  - [ ] Declare `int pkg_manifest_validate(const PkgManifest *m);`

- [ ] Create `src/apps/pkg/pkg_json.c`:
  - [ ] Implement minimal JSON parser (or use simple string parsing):
    - [ ] Parse `"name": "value"` patterns
    - [ ] Parse `"files": ["a", "b"]` arrays
    - [ ] Handle whitespace and newlines
  - [ ] Implement `pkg_manifest_parse()`:
    - [ ] Extract name, version, description fields
    - [ ] Extract files array
    - [ ] Extract docs array (optional)
    - [ ] Allocate and populate PkgManifest struct
  - [ ] Implement `pkg_manifest_load()`:
    - [ ] Read file contents
    - [ ] Call `pkg_manifest_parse()`
  - [ ] Implement `pkg_manifest_free()`:
    - [ ] Free all allocated strings
    - [ ] Free arrays
    - [ ] Free struct
  - [ ] Implement `pkg_manifest_validate()`:
    - [ ] Check name is non-empty
    - [ ] Check version is non-empty
    - [ ] Check files array has at least one entry
    - [ ] Return 0 if valid, -1 if invalid

### 1.3 Create Package Database Module
Manage the installed packages database.

- [ ] Create `src/apps/pkg/pkg_db.h`:
  - [ ] Define `PkgDbEntry` struct:
    ```c
    typedef struct {
      char *name;
      char *version;
    } PkgDbEntry;
    ```
  - [ ] Define `PkgDb` struct:
    ```c
    typedef struct {
      PkgDbEntry *entries;
      int count;
      int capacity;
    } PkgDb;
    ```
  - [ ] Declare `PkgDb *pkg_db_load(void);`
  - [ ] Declare `int pkg_db_save(const PkgDb *db);`
  - [ ] Declare `void pkg_db_free(PkgDb *db);`
  - [ ] Declare `PkgDbEntry *pkg_db_find(const PkgDb *db, const char *name);`
  - [ ] Declare `int pkg_db_add(PkgDb *db, const char *name, const char *version);`
  - [ ] Declare `int pkg_db_remove(PkgDb *db, const char *name);`

- [ ] Create `src/apps/pkg/pkg_db.c`:
  - [ ] Implement `pkg_db_load()`:
    - [ ] Open `~/.jshell/pkgdb.txt`
    - [ ] Read lines in format `<name> <version>`
    - [ ] Populate PkgDb struct
    - [ ] Return empty db if file doesn't exist
  - [ ] Implement `pkg_db_save()`:
    - [ ] Ensure parent directory exists
    - [ ] Write all entries to file
    - [ ] Format: `<name> <version>\n`
  - [ ] Implement `pkg_db_free()`:
    - [ ] Free all entries
    - [ ] Free arrays
  - [ ] Implement `pkg_db_find()`:
    - [ ] Linear search by name
    - [ ] Return pointer or NULL
  - [ ] Implement `pkg_db_add()`:
    - [ ] Resize array if needed
    - [ ] Add new entry
  - [ ] Implement `pkg_db_remove()`:
    - [ ] Find entry by name
    - [ ] Remove from array (shift remaining)
    - [ ] Update count

---

## Phase 2: pkg build

### 2.1 Implement pkg build Subcommand
Build a package tarball from a source directory.

- [ ] Update `pkg_build()` signature in `cmd_pkg.c`:
  - [ ] Change to `pkg_build(const char *src_dir, const char *output_tar, int json_output)`
  - [ ] Update argument parsing to accept two arguments

- [ ] Implement `pkg_build()`:
  - [ ] Validate `src_dir` exists and is a directory
  - [ ] Check `pkg.json` exists in `src_dir`
  - [ ] Load and validate manifest with `pkg_manifest_load()`
  - [ ] Verify all files listed in manifest exist
  - [ ] Build tar command arguments:
    ```c
    char *argv[] = {"tar", "-czf", output_tar, "-C", src_dir, ".", NULL};
    ```
  - [ ] Call `pkg_run_command(argv)` using fork/exec
  - [ ] Output result:
    - [ ] Human: `Created package: <output_tar>`
    - [ ] JSON: `{"status": "ok", "package": "<name>", "version": "<ver>", "output": "<path>"}`
  - [ ] Handle errors:
    - [ ] Missing src_dir
    - [ ] Missing pkg.json
    - [ ] Invalid manifest
    - [ ] tar command failure

- [ ] Update `pkg_print_usage()`:
  - [ ] Document: `build <src-dir> <output.tar.gz>  build a package for distribution`

- [ ] Create tests in `tests/apps/pkg/test_pkg_build.py`:
  - [ ] Test building package from valid directory
  - [ ] Test error when src_dir missing
  - [ ] Test error when pkg.json missing
  - [ ] Test error when files in manifest missing
  - [ ] Test JSON output format

---

## Phase 3: pkg install

### 3.1 Implement pkg install Subcommand
Install a package from a tarball.

- [ ] Update `pkg_install()` signature in `cmd_pkg.c`:
  - [ ] Keep as `pkg_install(const char *tarball, int json_output)`

- [ ] Implement `pkg_install()`:
  - [ ] Validate tarball exists
  - [ ] Ensure directories exist with `pkg_ensure_dirs()`
  - [ ] Create temp directory for extraction
  - [ ] Extract tarball to temp dir:
    ```c
    char *argv[] = {"tar", "-xzf", tarball, "-C", temp_dir, NULL};
    ```
  - [ ] Load `pkg.json` from extracted contents
  - [ ] Validate manifest
  - [ ] Check if package already installed (in pkgdb.txt)
  - [ ] Determine install path: `~/.jshell/pkgs/<name>-<version>/`
  - [ ] Move extracted contents to install path
  - [ ] For each file in manifest `files` array:
    - [ ] Create symlink in `~/.jshell/bin/` pointing to installed file
    - [ ] Make executable (`chmod +x`)
  - [ ] Update package database
  - [ ] Clean up temp directory
  - [ ] Output result:
    - [ ] Human: `Installed <name> version <version>`
    - [ ] JSON: `{"status": "ok", "name": "<name>", "version": "<ver>", "path": "<install_path>"}`
  - [ ] Handle errors:
    - [ ] Missing tarball
    - [ ] Invalid tarball (extraction fails)
    - [ ] Missing pkg.json in tarball
    - [ ] Package already installed (prompt or error)
    - [ ] Symlink creation failure

- [ ] Update `pkg_print_usage()`:
  - [ ] Document: `install <tarball>  install package from tarball`

- [ ] Create tests in `tests/apps/pkg/test_pkg_install.py`:
  - [ ] Test installing valid package
  - [ ] Test symlinks created in bin directory
  - [ ] Test pkgdb.txt updated
  - [ ] Test error when tarball missing
  - [ ] Test error when pkg.json missing in tarball
  - [ ] Test reinstall behavior
  - [ ] Test JSON output format

---

## Phase 4: pkg remove

### 4.1 Implement pkg remove Subcommand
Remove an installed package.

- [ ] Keep `pkg_remove()` signature as `pkg_remove(const char *name, int json_output)`

- [ ] Implement `pkg_remove()`:
  - [ ] Load package database
  - [ ] Find package by name
  - [ ] If not found, error:
    - [ ] Human: `pkg remove: package '<name>' not installed`
    - [ ] JSON: `{"status": "error", "message": "package not installed", "name": "<name>"}`
  - [ ] Get version from database entry
  - [ ] Determine package path: `~/.jshell/pkgs/<name>-<version>/`
  - [ ] Load `pkg.json` from package directory
  - [ ] For each file in manifest `files` array:
    - [ ] Determine symlink path in `~/.jshell/bin/`
    - [ ] Remove symlink if it exists and points to this package
  - [ ] Remove package directory recursively:
    - [ ] Use `nftw()` or manual recursion
    - [ ] Or call `rm -rf` via fork/exec
  - [ ] Remove entry from package database
  - [ ] Save updated database
  - [ ] Output result:
    - [ ] Human: `Removed <name> version <version>`
    - [ ] JSON: `{"status": "ok", "name": "<name>", "version": "<ver>"}`
  - [ ] Handle errors:
    - [ ] Package not installed
    - [ ] Directory removal failure
    - [ ] Database update failure

- [ ] Update `pkg_print_usage()`:
  - [ ] Document: `remove <name>  remove an installed package`

- [ ] Create tests in `tests/apps/pkg/test_pkg_remove.py`:
  - [ ] Test removing installed package
  - [ ] Test symlinks removed from bin directory
  - [ ] Test pkgdb.txt updated
  - [ ] Test error when package not installed
  - [ ] Test JSON output format

---

## Phase 5: pkg list

### 5.1 Implement pkg list Subcommand
List all installed packages.

- [ ] Implement `pkg_list()`:
  - [ ] Load package database
  - [ ] If empty:
    - [ ] Human: `No packages installed.`
    - [ ] JSON: `{"packages": []}`
  - [ ] For each entry:
    - [ ] Human: `<name>  <version>`
    - [ ] JSON: append to array
  - [ ] JSON output: `{"packages": [{"name": "...", "version": "..."}]}`

- [ ] Create tests in `tests/apps/pkg/test_pkg_list.py`:
  - [ ] Test listing when no packages installed
  - [ ] Test listing with installed packages
  - [ ] Test JSON output format

---

## Phase 6: pkg info

### 6.1 Implement pkg info Subcommand
Show detailed information about an installed package.

- [ ] Implement `pkg_info()`:
  - [ ] Load package database
  - [ ] Find package by name
  - [ ] If not found, error
  - [ ] Load `pkg.json` from package directory
  - [ ] Human output:
    ```
    Name:        <name>
    Version:     <version>
    Description: <description>
    Files:       <file1>, <file2>, ...
    Location:    ~/.jshell/pkgs/<name>-<version>/
    ```
  - [ ] JSON output:
    ```json
    {
      "name": "...",
      "version": "...",
      "description": "...",
      "files": [...],
      "path": "..."
    }
    ```

- [ ] Create tests in `tests/apps/pkg/test_pkg_info.py`:
  - [ ] Test info for installed package
  - [ ] Test error when package not installed
  - [ ] Test JSON output format

---

## Phase 7: pkg compile

### 7.1 Implement pkg compile Subcommand
Compile all apps in src/apps/ directory.

- [ ] Update `pkg_compile()` signature:
  - [ ] `pkg_compile(const char *app_name, int json_output)` where app_name is optional

- [ ] Implement `pkg_compile()`:
  - [ ] Determine apps directory (relative to pkg binary or configurable)
  - [ ] If `app_name` is NULL:
    - [ ] Iterate all subdirectories in `src/apps/`
    - [ ] For each directory, run make
  - [ ] If `app_name` is specified:
    - [ ] Only compile that specific app
  - [ ] For each app:
    - [ ] Check directory exists
    - [ ] Check Makefile exists
    - [ ] Run make:
      ```c
      char *argv[] = {"make", "-C", app_dir, NULL};
      ```
    - [ ] Track success/failure
  - [ ] Human output:
    ```
    Compiling cat... ok
    Compiling ls... ok
    Compiling rg... FAILED

    Compiled 18/19 apps successfully.
    ```
  - [ ] JSON output:
    ```json
    {
      "status": "ok",
      "results": [
        {"name": "cat", "status": "ok"},
        {"name": "ls", "status": "ok"},
        {"name": "rg", "status": "error", "message": "..."}
      ],
      "success_count": 18,
      "total_count": 19
    }
    ```

- [ ] Update `pkg_print_usage()`:
  - [ ] Document: `compile [name]  compile apps from source (all if name omitted)`

- [ ] Create tests in `tests/apps/pkg/test_pkg_compile.py`:
  - [ ] Test compiling all apps
  - [ ] Test compiling specific app
  - [ ] Test error when app directory missing
  - [ ] Test JSON output format

---

## Phase 8: Update Makefiles

### 8.1 Update pkg Makefile
Add new source files to pkg build.

- [ ] Update `src/apps/pkg/Makefile`:
  - [ ] Add `pkg_utils.o` to OBJS
  - [ ] Add `pkg_json.o` to OBJS
  - [ ] Add `pkg_db.o` to OBJS
  - [ ] Add dependency rules for new files

### 8.2 Verify App Makefiles
Ensure all app Makefiles produce correct outputs.

- [ ] For each app in `src/apps/`:
  - [ ] Verify Makefile builds standalone binary to `bin/standalone-apps/<name>`
  - [ ] Verify Makefile builds static library `lib<name>.a`
  - [ ] Test with `make clean && make`

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
