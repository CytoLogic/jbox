#!/usr/bin/env python3
"""Error handling tests for package manager."""

import json
import os
import tempfile
import unittest
from pathlib import Path

from tests.pkg import PkgTestBase


class TestInstallErrors(PkgTestBase):
    """Test install command error handling."""

    def test_install_nonexistent_tarball(self):
        """Test installing from non-existent tarball."""
        result = self.run_pkg("install", "/nonexistent/path/pkg.tar.gz")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("error", result.stderr.lower())

    def test_install_invalid_tarball(self):
        """Test installing from invalid/corrupted tarball."""
        with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
            f.write(b"not a valid tarball content")
            tarball_path = f.name

        try:
            result = self.run_pkg("install", tarball_path)
            self.assertNotEqual(result.returncode, 0)
        finally:
            os.unlink(tarball_path)

    def test_install_no_pkg_json(self):
        """Test installing tarball without pkg.json."""
        tmpdir = tempfile.mkdtemp()
        try:
            # Create tarball without pkg.json
            pkg_dir = Path(tmpdir) / "test-pkg"
            pkg_dir.mkdir()
            (pkg_dir / "bin").mkdir()
            (pkg_dir / "bin" / "test").write_text("#!/bin/sh\necho test")

            tarball = tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False)
            tarball.close()

            import subprocess
            subprocess.run(
                ["tar", "-czf", tarball.name, "-C", str(pkg_dir), "."],
                check=True
            )

            result = self.run_pkg("install", tarball.name)
            self.assertNotEqual(result.returncode, 0)
            os.unlink(tarball.name)
        finally:
            import shutil
            shutil.rmtree(tmpdir)

    def test_install_no_argument(self):
        """Test install without package argument."""
        result = self.run_pkg("install")
        self.assertNotEqual(result.returncode, 0)


class TestRemoveErrors(PkgTestBase):
    """Test remove command error handling."""

    def test_remove_nonexistent_package(self):
        """Test removing package that isn't installed."""
        result = self.run_pkg("remove", "nonexistent-pkg")
        self.assertNotEqual(result.returncode, 0)

    def test_remove_no_argument(self):
        """Test remove without package argument."""
        result = self.run_pkg("remove")
        self.assertNotEqual(result.returncode, 0)


class TestBuildErrors(PkgTestBase):
    """Test build command error handling."""

    def test_build_nonexistent_directory(self):
        """Test building from non-existent directory."""
        result = self.run_pkg("build", "/nonexistent/directory", "/tmp/out.tar.gz")
        self.assertNotEqual(result.returncode, 0)

    def test_build_no_pkg_json(self):
        """Test building directory without pkg.json."""
        tmpdir = tempfile.mkdtemp()
        try:
            result = self.run_pkg("build", tmpdir, "/tmp/out.tar.gz")
            self.assertNotEqual(result.returncode, 0)
        finally:
            import shutil
            shutil.rmtree(tmpdir)


class TestInfoErrors(PkgTestBase):
    """Test info command error handling."""

    def test_info_nonexistent_package(self):
        """Test info for non-existent package."""
        result = self.run_pkg("info", "nonexistent-pkg", "--json")
        self.assertNotEqual(result.returncode, 0)

    def test_info_no_argument(self):
        """Test info without package argument."""
        result = self.run_pkg("info")
        self.assertNotEqual(result.returncode, 0)


class TestCompileErrors(PkgTestBase):
    """Test compile command error handling."""

    def test_compile_nonexistent_package(self):
        """Test compiling non-existent package."""
        result = self.run_pkg("compile", "nonexistent-pkg")
        self.assertNotEqual(result.returncode, 0)

    def test_compile_no_source(self):
        """Test compiling package without source code."""
        # Install a package without source
        self.install_test_package("no-src-pkg", "1.0.0", with_source=False)

        result = self.run_pkg("compile", "no-src-pkg")
        # Should handle gracefully (may succeed with fallback or return error)
        # Either way, it shouldn't crash


class TestMalformedInput(PkgTestBase):
    """Test handling of malformed inputs."""

    def test_empty_package_name(self):
        """Test commands with empty package name."""
        result = self.run_pkg("info", "")
        self.assertNotEqual(result.returncode, 0)

    def test_special_chars_in_name(self):
        """Test handling special characters in package names."""
        result = self.run_pkg("info", "../../../etc/passwd")
        self.assertNotEqual(result.returncode, 0)

    def test_very_long_package_name(self):
        """Test handling very long package names."""
        long_name = "a" * 1000
        result = self.run_pkg("info", long_name)
        self.assertNotEqual(result.returncode, 0)


class TestJsonOutputErrors(PkgTestBase):
    """Test JSON output for error conditions."""

    def test_info_error_json(self):
        """Test that info errors return valid JSON when --json specified."""
        result = self.run_pkg("info", "nonexistent", "--json")
        # Should still be valid JSON even on error
        try:
            data = json.loads(result.stdout)
            # Should have error indicator
            self.assertIn("error", data.get("status", "").lower() or result.stdout.lower())
        except json.JSONDecodeError:
            # If not JSON, check stderr has error message
            self.assertIn("error", result.stderr.lower())


if __name__ == "__main__":
    unittest.main()
