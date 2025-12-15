#!/usr/bin/env python3
"""Tests for pkg install extraction and compilation."""

import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from tests.pkg import PkgTestBase


class TestPkgInstallExtraction(PkgTestBase):
    """Test that pkg install extracts packages to correct directory structure."""

    def test_install_extracts_to_correct_directory(self):
        """Verify tarball is extracted to ~/.jshell/pkgs/<name>-<version>/."""
        tarball = self.build_test_tarball("extract-test", "2.0.0")
        try:
            result = self.run_pkg("install", str(tarball))
            self.assertEqual(result.returncode, 0, f"Install failed: {result.stderr}")

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
            self.assertEqual(result.returncode, 0, f"Install failed: {result.stderr}")

            pkg_dir = self.JSHELL_HOME / "pkgs" / "src-test-1.0.0"

            # Verify Makefile exists
            makefile = pkg_dir / "Makefile"
            self.assertTrue(makefile.exists(), "Makefile not found in package dir")

            # Verify src/ directory with source files
            src_dir = pkg_dir / "src"
            self.assertTrue(src_dir.exists(), "src/ directory not found")

            # Verify source file exists
            source_file = src_dir / "src-test_main.c"
            self.assertTrue(source_file.exists(), "Source file not found")

        finally:
            tarball.unlink()

    def test_install_creates_pkgdb_entry(self):
        """Verify install creates entry in pkgdb.json."""
        tarball = self.build_test_tarball("db-test", "1.0.0")
        try:
            result = self.run_pkg("install", str(tarball))
            self.assertEqual(result.returncode, 0)

            # Check pkgdb.json
            pkgdb_path = self.JSHELL_HOME / "pkgs" / "pkgdb.json"
            self.assertTrue(pkgdb_path.exists(), "pkgdb.json not created")

            with open(pkgdb_path) as f:
                data = json.load(f)

            packages = {p["name"]: p for p in data.get("packages", [])}
            self.assertIn("db-test", packages)
            self.assertEqual(packages["db-test"]["version"], "1.0.0")

        finally:
            tarball.unlink()


class TestPkgInstallCompilation(PkgTestBase):
    """Test that pkg install compiles packages with source."""

    def test_install_compiles_package(self):
        """Test that install runs compilation for packages with Makefile."""
        tarball = self.build_test_tarball("compile-test", "1.0.0", with_source=True)
        try:
            result = self.run_pkg("install", str(tarball))
            self.assertEqual(result.returncode, 0, f"Install failed: {result.stderr}")

            # Verify "Compiling" message appears
            self.assertIn("Compiling", result.stdout)

            # Verify package installed
            self.assertPackageInstalled("compile-test", "1.0.0")

            # Verify binary exists and is ELF (compiled)
            pkg_dir = self.JSHELL_HOME / "pkgs" / "compile-test-1.0.0"
            binary_path = pkg_dir / "bin" / "compile-test"
            self.assertTrue(binary_path.exists())

            file_result = subprocess.run(
                ["file", str(binary_path)],
                capture_output=True, text=True
            )
            self.assertIn("ELF", file_result.stdout,
                          f"Binary is not ELF: {file_result.stdout}")

        finally:
            tarball.unlink()

    def test_install_creates_working_symlink(self):
        """Test that symlinks work after install with compilation."""
        tarball = self.build_test_tarball("symlink-test", "1.0.0", with_source=True)
        try:
            result = self.run_pkg("install", str(tarball))
            self.assertEqual(result.returncode, 0)

            # Verify symlink exists
            self.assertBinaryExists("symlink-test")

            # Run via symlink
            symlink = self.JSHELL_HOME / "bin" / "symlink-test"
            run_result = subprocess.run(
                [str(symlink)],
                capture_output=True, text=True,
                env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
            )
            self.assertEqual(run_result.returncode, 0)
            self.assertIn("Hello from symlink-test", run_result.stdout)

        finally:
            tarball.unlink()

    def test_install_without_source_works(self):
        """Test that install still works for packages without source/Makefile."""
        tarball = self.build_test_tarball("nosrc-pkg", "1.0.0", with_source=False)
        try:
            result = self.run_pkg("install", str(tarball))
            self.assertEqual(result.returncode, 0)

            # Should NOT have "Compiling" message
            self.assertNotIn("Compiling", result.stdout)

            self.assertPackageInstalled("nosrc-pkg", "1.0.0")
            self.assertBinaryExists("nosrc-pkg")

        finally:
            tarball.unlink()

    def test_installed_binary_runs_correctly(self):
        """Test that the installed binary executes correctly."""
        tarball = self.build_test_tarball("run-test", "1.0.0", with_source=True)
        try:
            result = self.run_pkg("install", str(tarball))
            self.assertEqual(result.returncode, 0)

            # Run the binary directly
            bin_path = self.JSHELL_HOME / "pkgs" / "run-test-1.0.0" / "bin" / "run-test"
            run_result = subprocess.run(
                [str(bin_path)],
                capture_output=True, text=True,
                env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
            )
            self.assertEqual(run_result.returncode, 0)
            self.assertIn("Hello from run-test", run_result.stdout)

        finally:
            tarball.unlink()


class TestPkgCompileIntegration(PkgTestBase):
    """Test pkg compile command integration."""

    def test_pkg_compile_after_install(self):
        """Test that pkg compile works on installed package."""
        tarball = self.build_test_tarball("recompile-test", "1.0.0", with_source=True)
        try:
            # Install
            result = self.run_pkg("install", str(tarball))
            self.assertEqual(result.returncode, 0)

            # Recompile
            result = self.run_pkg("compile", "recompile-test")
            self.assertEqual(result.returncode, 0, f"Compile failed: {result.stderr}")

            # Verify binary still works
            symlink = self.JSHELL_HOME / "bin" / "recompile-test"
            run_result = subprocess.run(
                [str(symlink)],
                capture_output=True, text=True,
                env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
            )
            self.assertEqual(run_result.returncode, 0)

        finally:
            tarball.unlink()


class TestPkgLifecycleWithCompilation(PkgTestBase):
    """Test full package lifecycle with compilation."""

    def test_full_lifecycle_with_source(self):
        """Test: build → install (compile) → use → remove."""
        # 1. Create and build package
        pkg_dir = self.create_test_package(
            "lifecycle-src", "1.0.0",
            description="Lifecycle test with source",
            with_source=True
        )
        try:
            tarball = tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False)
            tarball.close()

            result = self.run_pkg("build", str(pkg_dir), tarball.name)
            self.assertEqual(result.returncode, 0, f"Build failed: {result.stderr}")

            # 2. Install (should compile)
            result = self.run_pkg("install", tarball.name)
            self.assertEqual(result.returncode, 0, f"Install failed: {result.stderr}")
            self.assertIn("Compiling", result.stdout)

            # 3. Verify installed
            self.assertPackageInstalled("lifecycle-src", "1.0.0")
            self.assertBinaryExists("lifecycle-src")

            # 4. Run via symlink
            symlink = self.JSHELL_HOME / "bin" / "lifecycle-src"
            run_result = subprocess.run(
                [str(symlink)],
                capture_output=True, text=True,
                env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
            )
            self.assertEqual(run_result.returncode, 0)
            self.assertIn("Hello from lifecycle-src", run_result.stdout)

            # 5. Get info
            result = self.run_pkg("info", "lifecycle-src", "--json")
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            self.assertEqual(data["name"], "lifecycle-src")
            self.assertEqual(data["version"], "1.0.0")

            # 6. Remove
            result = self.run_pkg("remove", "lifecycle-src")
            self.assertEqual(result.returncode, 0, f"Remove failed: {result.stderr}")

            # 7. Verify removed
            self.assertPackageNotInstalled("lifecycle-src")
            self.assertBinaryNotExists("lifecycle-src")

        finally:
            shutil.rmtree(pkg_dir.parent)
            if os.path.exists(tarball.name):
                os.unlink(tarball.name)


if __name__ == "__main__":
    unittest.main()
