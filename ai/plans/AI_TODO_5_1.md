# Package Manager Command Verification Plan

## Status: ✅ COMPLETE

All phases implemented and verified with 60 passing tests.

## Overview

Verify and fix the `pkg` commands to work correctly with the new JSON-based package
database format (pkgdb.json) and the new package installation structure.

### Implementation Summary

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 1 | Verify existing commands work with JSON format | ✅ Complete |
| Phase 2 | Verify install extracts to correct directory | ✅ Complete |
| Phase 3 | Fix pkg install to call compilation | ✅ Complete |
| Phase 4 | Create verification tests | ✅ Complete |
| Phase 5 | Update documentation | ✅ Complete |

### Changes Made

1. **src/apps/pkg/cmd_pkg.c**:
   - Added forward declaration for `compile_package_dir()`
   - Added compilation step to `pkg_install()` after package extraction
   - Updated help text to document new behavior

2. **tests/pkg/test_pkg_base.py**:
   - Updated `create_test_package()` to generate Makefile for source packages
   - Updated to actually compile source packages during creation

3. **tests/pkg/test_pkg_install_compile.py** (new file):
   - 9 new tests for extraction and compilation verification

### Background

AI_TODO_5.md implemented:
- JSON package database (`~/.jshell/pkgs/pkgdb.json`)
- Package installation to `~/.jshell/pkgs/<name>-<version>/`
- Symlinks in `~/.jshell/bin/`
- `pkg compile` for installed packages
- Source file inclusion in packages via PKG_SRCS/PKG_HDRS

### Original State Analysis

After reviewing `src/apps/pkg/cmd_pkg.c` and `src/apps/pkg/pkg_db.c`:

| Command | JSON Support | Status | Issue |
|---------|-------------|--------|-------|
| `pkg list` | ✅ Uses pkg_db_load() | Working | None |
| `pkg info` | ✅ Uses pkg_db_load()/pkg_db_find() | Working | None |
| `pkg remove` | ✅ Removes from pkgdb.json | Working | None |
| `pkg install` | ✅ Updates pkgdb.json | **Fixed** | Now calls compile |
| `pkg compile` | ✅ Compiles installed packages | Working | None |

### Critical Issue (RESOLVED)

**`pkg install` now calls compilation after installation.**

New flow (after fix):
1. Extract tarball to temp directory
2. Read pkg.json manifest
3. Move to `~/.jshell/pkgs/<name>-<version>/`
4. **Compile package if Makefile exists**
5. Create symlinks to compiled binaries
6. Update pkgdb.json

---

## Implementation Plan

### Phase 1: Verify Existing Commands Work with JSON Format

#### 1.1 Verify `pkg list` Uses pkgdb.json

**Status**: ✅ VERIFIED

Code analysis of `pkg_list()` (lines 100-132):
- Calls `pkg_db_load()` which uses JSON format
- Iterates over `db->entries[]` from JSON database
- JSON output includes `{"packages": [{"name": ..., "version": ...}]}`

**Test**: Run existing tests to confirm
```bash
python -m unittest tests.pkg.test_pkg_lifecycle.TestPkgLifecycle.test_list_installed_packages -v
```

#### 1.2 Verify `pkg info` Uses pkgdb.json

**Status**: ✅ VERIFIED

Code analysis of `pkg_info()` (lines 135-241):
- Calls `pkg_db_load()` then `pkg_db_find()`
- Looks up package in `~/.jshell/pkgs/<name>-<version>/pkg.json`
- Returns full metadata from pkgdb.json

**Test**: Run existing tests to confirm
```bash
python -m unittest tests.pkg.test_pkg_lifecycle.TestPkgLifecycle.test_info_installed_package -v
```

#### 1.3 Verify `pkg remove` Uses pkgdb.json

**Status**: ✅ VERIFIED

Code analysis of `pkg_remove()` (lines 516-656):
- Calls `pkg_db_load()` then `pkg_db_find()` to look up package
- Gets version from entry, constructs path `~/.jshell/pkgs/<name>-<version>/`
- Reads pkg.json manifest for file list
- Removes symlinks from `~/.jshell/bin/` (only those pointing to package)
- Removes package directory recursively
- Calls `pkg_db_remove()` then `pkg_db_save()` to update pkgdb.json

**Test**: Run existing tests to confirm
```bash
python -m unittest tests.pkg.test_pkg_lifecycle.TestMultiPackageOperations.test_selective_removal -v
```

---

### Phase 2: Verify Install Extracts to Correct Directory

#### 2.1 Verify Tarball Extraction to Package Directory

**Status**: ✅ VERIFIED (needs test coverage)

Code analysis of `pkg_install()` (lines 305-513):

1. **Extract to temp directory** (lines 337-358):
   ```c
   char temp_dir[] = "/tmp/pkg-install-XXXXXX";
   mkdtemp(temp_dir);
   char *tar_argv[] = {"tar", "-xzf", (char *)tarball, "-C", temp_dir, NULL};
   pkg_run_command(tar_argv);
   ```

2. **Read manifest from temp** (lines 360-380):
   ```c
   snprintf(manifest_path, manifest_path_len, "%s/pkg.json", temp_dir);
   PkgManifest *m = pkg_manifest_load(manifest_path);
   ```

3. **Construct install path** (lines 418-438):
   ```c
   snprintf(install_path, install_path_len, "%s/%s-%s",
            pkgs_dir, m->name, m->version);
   // Result: ~/.jshell/pkgs/<name>-<version>
   ```

4. **Move temp to install location** (lines 440-455):
   ```c
   if (rename(temp_dir, install_path) != 0) {
     char *mv_argv[] = {"mv", temp_dir, install_path, NULL};
     pkg_run_command(mv_argv);
   }
   ```

**Expected directory structure after extraction**:
```
~/.jshell/pkgs/<name>-<version>/
├── pkg.json           # Package manifest
├── bin/               # Pre-built binaries
│   └── <name>
├── src/               # Source files (if included)
│   ├── cmd_<name>.c
│   ├── cmd_<name>.h
│   └── <name>_main.c
└── Makefile           # Build instructions (if included)
```

**Tasks**:
- [x] Add test to verify extraction creates correct directory structure
- [x] Add test to verify pkg.json is in package root
- [x] Add test to verify bin/ directory contains pre-built binary
- [x] Add test to verify src/ and Makefile are present (for source packages)

#### 2.2 Create Extraction Verification Tests

**File**: `tests/pkg/test_pkg_install_compile.py`

```python
def test_install_extracts_to_correct_directory(self):
    """Verify tarball is extracted to ~/.jshell/pkgs/<name>-<version>/."""
    tarball = self.build_test_tarball("extract-test", "2.0.0")
    try:
        result = self.run_pkg("install", str(tarball))
        self.assertEqual(result.returncode, 0)

        # Verify directory structure
        pkg_dir = self.JSHELL_HOME / "pkgs" / "extract-test-2.0.0"
        self.assertTrue(pkg_dir.exists(), f"Package dir not found: {pkg_dir}")

        # Verify pkg.json exists
        manifest = pkg_dir / "pkg.json"
        self.assertTrue(manifest.exists(), "pkg.json not found in package dir")

        # Verify bin/ directory with binary
        bin_dir = pkg_dir / "bin"
        self.assertTrue(bin_dir.exists(), "bin/ directory not found")
        binary = bin_dir / "extract-test"
        self.assertTrue(binary.exists(), "Binary not found in bin/")

    finally:
        tarball.unlink()

def test_install_extracts_source_files(self):
    """Verify source packages have src/ and Makefile after extraction."""
    tarball = self.build_test_tarball("src-test", "1.0.0", with_source=True)
    try:
        result = self.run_pkg("install", str(tarball))
        self.assertEqual(result.returncode, 0)

        pkg_dir = self.JSHELL_HOME / "pkgs" / "src-test-1.0.0"

        # Verify Makefile exists
        makefile = pkg_dir / "Makefile"
        self.assertTrue(makefile.exists(), "Makefile not found in package dir")

        # Verify src/ directory with source files
        src_dir = pkg_dir / "src"
        self.assertTrue(src_dir.exists(), "src/ directory not found")

    finally:
        tarball.unlink()
```

---

### Phase 3: Fix `pkg install` to Compile After Installation

#### 3.1 Modify pkg_install() to Call Compilation

**File**: `src/apps/pkg/cmd_pkg.c`

**Current code** (around line 456-496):
```c
// Move package to install location
if (rename(temp_dir, install_path) != 0) {
  // ... fallback to mv command ...
}

// Create symlinks for files
char *bin_dir = pkg_get_bin_dir();
for (int i = 0; i < m->files_count; i++) {
  // Create symlinks...
}

// Update database
pkg_db_add_full(db, m->name, m->version, ...);
pkg_db_save(db);
```

**Required changes**:
1. After moving package to install location, call compilation function
2. Only create symlinks AFTER successful compilation (or if no Makefile exists)
3. Update symlinks to point to compiled binaries

**Tasks**:
- [x] Modify `pkg_install()`:
  - [x] After moving package to `install_path`, check for Makefile
  - [x] If Makefile exists, call `compile_package_dir(name, install_path, json_output, 0)`
  - [x] Continue with symlink creation (which now points to compiled binaries)
  - [x] Handle compilation failure gracefully (package still installed, just not compiled)

- [x] Add compilation step (after line 455):
```c
// Move package to install location
if (rename(temp_dir, install_path) != 0) { ... }

// Compile package if it has a Makefile
char makefile_path[4096];
snprintf(makefile_path, sizeof(makefile_path), "%s/Makefile", install_path);
struct stat makefile_st;
if (stat(makefile_path, &makefile_st) == 0) {
  if (!json_output) {
    printf("Compiling %s...\n", m->name);
  }
  int compile_result = compile_package_dir(m->name, install_path, 0, 0);
  if (compile_result != 0 && !json_output) {
    fprintf(stderr, "Warning: compilation failed, using pre-built binary\n");
  }
}

// Create symlinks (existing code)
char *bin_dir = pkg_get_bin_dir();
// ...
```

#### 3.2 Update Symlink Creation Logic

**File**: `src/apps/pkg/cmd_pkg.c`

The symlink creation code (lines 465-494) already handles this correctly:
- It symlinks `~/.jshell/bin/<name>` → `~/.jshell/pkgs/<name>-<version>/bin/<name>`
- After compilation, the binary at `bin/<name>` is the compiled version

**Verify**: Symlinks point to package bin/ directory, which is populated by:
1. Pre-built binaries from tarball, OR
2. Compiled binaries from `make` (which outputs to bin/)

---

### Phase 4: Create Verification Tests

#### 4.1 Test Install with Compilation

**File**: `tests/pkg/test_pkg_install_compile.py`

- [x] Create test file with:
  - [x] `test_install_compiles_package` - Install package with source, verify compilation runs
  - [x] `test_install_creates_symlinks_after_compile` - Verify symlinks work after compile
  - [x] `test_install_without_source` - Package without Makefile still works
  - [x] `test_install_compile_failure_fallback` - Graceful handling if compile fails

```python
class TestPkgInstallCompile(PkgTestBase):
    """Test that pkg install compiles packages with source."""

    def test_install_compiles_package(self):
        """Test that install runs compilation for packages with Makefile."""
        # Build a package with source files
        tarball = self.build_test_tarball("compile-test", "1.0.0", with_source=True)
        try:
            result = self.run_pkg("install", str(tarball))
            self.assertEqual(result.returncode, 0)

            # Verify package installed
            self.assertPackageInstalled("compile-test", "1.0.0")

            # Verify binary exists and is executable
            self.assertBinaryExists("compile-test")

            # Run the binary to verify it works
            bin_path = self.JSHELL_HOME / "bin" / "compile-test"
            import subprocess
            result = subprocess.run([str(bin_path)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0)
            self.assertIn("Hello from compile-test", result.stdout)
        finally:
            tarball.unlink()

    def test_install_without_source_works(self):
        """Test that install still works for packages without source/Makefile."""
        # Build package without source
        tarball = self.build_test_tarball("nosrc-pkg", "1.0.0", with_source=False)
        try:
            result = self.run_pkg("install", str(tarball))
            self.assertEqual(result.returncode, 0)
            self.assertPackageInstalled("nosrc-pkg", "1.0.0")
            self.assertBinaryExists("nosrc-pkg")
        finally:
            tarball.unlink()
```

#### 4.2 Test Full Lifecycle with Compilation

**File**: `tests/pkg/test_pkg_lifecycle.py` (update existing)

- [x] Add test case:
  - [x] `test_lifecycle_with_compilation` - Build → Install (with compile) → Use → Remove

---

### Phase 5: Documentation Updates

#### 5.1 Update pkg --help

**File**: `src/apps/pkg/cmd_pkg.c`

- [x] Update `pkg_print_usage()` to document that:
  - `install` automatically compiles packages with Makefile/source
  - `compile` can be used to recompile installed packages

Current help text (line 74):
```c
fprintf(out, "  install <tarball>         install package from tarball\n");
```

Update to:
```c
fprintf(out, "  install <tarball>         install and compile package from tarball\n");
```

---

## Implementation Order

1. **Phase 1**: Verify existing commands work with JSON format (run existing tests)
2. **Phase 2**: Verify install extracts to correct directory structure
3. **Phase 3**: Fix pkg_install() to call compilation after extraction
4. **Phase 4**: Create verification tests for extraction and compilation
5. **Phase 5**: Update documentation

---

## Testing Checklist

### Pre-implementation Tests (should pass)
```bash
# Verify existing commands work with JSON format
python -m unittest tests.pkg.test_pkg_lifecycle -v
python -m unittest tests.pkg.test_pkg_errors -v
python -m unittest tests.pkg.test_pkg_shell -v
```

### Post-implementation Tests (new + existing)
```bash
# Run all pkg tests
python -m unittest discover -s tests/pkg -v
```

---

## File Changes Summary

### Files to Modify
```
src/apps/pkg/cmd_pkg.c          # Add compilation to pkg_install()
```

### Files to Create
```
tests/pkg/test_pkg_install_compile.py   # New tests for extraction and compilation
```

### Files to Update (tests)
```
tests/pkg/test_pkg_lifecycle.py         # Add lifecycle with compilation test
tests/pkg/test_pkg_base.py              # Verify create_test_package generates:
                                        #   - Correct directory structure
                                        #   - Source files for with_source=True
                                        #   - Makefile for compilable packages
```

---

## Notes

### Backwards Compatibility

The changes maintain backwards compatibility:
- Packages without Makefile/source: Install as before (no compilation)
- Packages with Makefile/source: Compilation runs automatically
- If compilation fails, package is still installed with pre-built binary

### Compilation Detection

A package is considered compilable if it has a Makefile in its root directory.
This is consistent with how `pkg compile` already works.

### Symlink Behavior

Symlinks always point to `~/.jshell/pkgs/<name>-<version>/bin/<name>`:
- For pre-built packages: This contains the shipped binary
- For compiled packages: This contains the compiled binary (Makefile output)

The Makefile convention (from pkg.mk) outputs binaries to `bin/<name>`.
