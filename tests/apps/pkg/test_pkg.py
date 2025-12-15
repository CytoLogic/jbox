#!/usr/bin/env python3
"""Unit tests for the pkg command."""

import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestPkgCommand(unittest.TestCase):
    """Test cases for the pkg command."""

    PKG_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "pkg"
    JSHELL_HOME = Path.home() / ".jshell"

    @classmethod
    def setUpClass(cls):
        """Verify the pkg binary exists before running tests."""
        if not cls.PKG_BIN.exists():
            raise unittest.SkipTest(f"pkg binary not found at {cls.PKG_BIN}")
        # Backup existing .jshell if it exists
        cls.backup_dir = None
        if cls.JSHELL_HOME.exists():
            cls.backup_dir = cls.JSHELL_HOME.parent / ".jshell.backup"
            if cls.backup_dir.exists():
                shutil.rmtree(cls.backup_dir)
            shutil.move(cls.JSHELL_HOME, cls.backup_dir)

    @classmethod
    def tearDownClass(cls):
        """Restore .jshell if we backed it up."""
        if cls.JSHELL_HOME.exists():
            shutil.rmtree(cls.JSHELL_HOME)
        if cls.backup_dir and cls.backup_dir.exists():
            shutil.move(cls.backup_dir, cls.JSHELL_HOME)

    def setUp(self):
        """Clean up .jshell before each test."""
        if self.JSHELL_HOME.exists():
            shutil.rmtree(self.JSHELL_HOME)

    def run_pkg(self, *args, cwd=None):
        """Run the pkg command with given arguments and return result."""
        cmd = [str(self.PKG_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            cwd=cwd,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def create_test_package(self, name="test-pkg", version="1.0.0"):
        """Create a test package directory with pkg.json and return its path."""
        tmpdir = tempfile.mkdtemp()
        pkg_dir = Path(tmpdir) / name
        pkg_dir.mkdir()
        bin_dir = pkg_dir / "bin"
        bin_dir.mkdir()

        # Create executable
        exe_path = bin_dir / "hello"
        exe_path.write_text('#!/bin/sh\necho "Hello from test!"')
        exe_path.chmod(0o755)

        # Create pkg.json
        manifest = {
            "name": name,
            "version": version,
            "description": f"Test package {name}",
            "files": ["bin/hello"]
        }
        (pkg_dir / "pkg.json").write_text(json.dumps(manifest, indent=2))

        return pkg_dir

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_pkg("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: pkg", result.stdout)
        self.assertIn("--help", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_pkg("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: pkg", result.stdout)

    def test_help_shows_commands(self):
        """Test help shows all subcommands."""
        result = self.run_pkg("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("list", result.stdout)
        self.assertIn("info", result.stdout)
        self.assertIn("install", result.stdout)
        self.assertIn("remove", result.stdout)
        self.assertIn("build", result.stdout)
        self.assertIn("compile", result.stdout)

    def test_no_arguments_error(self):
        """Test pkg with no arguments shows error."""
        result = self.run_pkg()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Try 'pkg --help'", result.stderr)

    # List tests
    def test_list_empty(self):
        """Test list when no packages installed."""
        result = self.run_pkg("list")
        self.assertEqual(result.returncode, 0)
        self.assertIn("No packages installed", result.stdout)

    def test_list_empty_json(self):
        """Test list with --json when no packages installed."""
        result = self.run_pkg("list", "--json")
        self.assertEqual(result.returncode, 0)
        output = json.loads(result.stdout)
        self.assertEqual(output["packages"], [])

    # Build tests
    def test_build_requires_args(self):
        """Test build requires source dir and output path."""
        result = self.run_pkg("build")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("source directory required", result.stderr)

    def test_build_requires_output(self):
        """Test build requires output path."""
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self.run_pkg("build", tmpdir)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("output tarball path required", result.stderr)

    def test_build_missing_src_dir(self):
        """Test build with nonexistent source directory."""
        result = self.run_pkg("build", "/nonexistent/path", "/tmp/out.tar.gz")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("source directory not found", result.stderr)

    def test_build_missing_pkg_json(self):
        """Test build with missing pkg.json."""
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self.run_pkg("build", tmpdir, "/tmp/out.tar.gz")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("pkg.json not found", result.stderr)

    def test_build_success(self):
        """Test successful package build."""
        pkg_dir = self.create_test_package()
        try:
            with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
                output_path = f.name

            try:
                result = self.run_pkg("build", str(pkg_dir), output_path)
                self.assertEqual(result.returncode, 0)
                self.assertIn("Created package", result.stdout)
                self.assertTrue(Path(output_path).exists())
            finally:
                Path(output_path).unlink(missing_ok=True)
        finally:
            shutil.rmtree(pkg_dir.parent)

    def test_build_json(self):
        """Test build with --json output."""
        pkg_dir = self.create_test_package()
        try:
            with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
                output_path = f.name

            try:
                result = self.run_pkg("build", str(pkg_dir), output_path, "--json")
                self.assertEqual(result.returncode, 0)
                output = json.loads(result.stdout)
                self.assertEqual(output["status"], "ok")
                self.assertEqual(output["package"], "test-pkg")
                self.assertEqual(output["version"], "1.0.0")
            finally:
                Path(output_path).unlink(missing_ok=True)
        finally:
            shutil.rmtree(pkg_dir.parent)

    # Install tests
    def test_install_requires_tarball(self):
        """Test install requires tarball path."""
        result = self.run_pkg("install")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("tarball path required", result.stderr)

    def test_install_missing_tarball(self):
        """Test install with nonexistent tarball."""
        result = self.run_pkg("install", "/nonexistent/package.tar.gz")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("tarball not found", result.stderr)

    def test_install_success(self):
        """Test successful package installation."""
        pkg_dir = self.create_test_package()
        try:
            with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
                tarball_path = f.name

            try:
                # Build package
                result = self.run_pkg("build", str(pkg_dir), tarball_path)
                self.assertEqual(result.returncode, 0)

                # Install package
                result = self.run_pkg("install", tarball_path)
                self.assertEqual(result.returncode, 0)
                self.assertIn("Installed test-pkg", result.stdout)

                # Verify installation
                install_dir = self.JSHELL_HOME / "pkgs" / "test-pkg-1.0.0"
                self.assertTrue(install_dir.exists())
                self.assertTrue((install_dir / "pkg.json").exists())

                # Verify symlink
                bin_link = self.JSHELL_HOME / "bin" / "hello"
                self.assertTrue(bin_link.is_symlink())
            finally:
                Path(tarball_path).unlink(missing_ok=True)
        finally:
            shutil.rmtree(pkg_dir.parent)

    def test_install_json(self):
        """Test install with --json output."""
        pkg_dir = self.create_test_package()
        try:
            with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
                tarball_path = f.name

            try:
                self.run_pkg("build", str(pkg_dir), tarball_path)
                result = self.run_pkg("install", tarball_path, "--json")
                self.assertEqual(result.returncode, 0)
                output = json.loads(result.stdout)
                self.assertEqual(output["status"], "ok")
                self.assertEqual(output["name"], "test-pkg")
                self.assertEqual(output["version"], "1.0.0")
            finally:
                Path(tarball_path).unlink(missing_ok=True)
        finally:
            shutil.rmtree(pkg_dir.parent)

    def test_install_already_installed(self):
        """Test installing package that's already installed."""
        pkg_dir = self.create_test_package()
        try:
            with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
                tarball_path = f.name

            try:
                self.run_pkg("build", str(pkg_dir), tarball_path)
                self.run_pkg("install", tarball_path)

                # Try to install again
                result = self.run_pkg("install", tarball_path)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("already installed", result.stderr)
            finally:
                Path(tarball_path).unlink(missing_ok=True)
        finally:
            shutil.rmtree(pkg_dir.parent)

    # List tests with installed package
    def test_list_with_package(self):
        """Test list after installing a package."""
        pkg_dir = self.create_test_package()
        try:
            with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
                tarball_path = f.name

            try:
                self.run_pkg("build", str(pkg_dir), tarball_path)
                self.run_pkg("install", tarball_path)

                result = self.run_pkg("list")
                self.assertEqual(result.returncode, 0)
                self.assertIn("test-pkg", result.stdout)
                self.assertIn("1.0.0", result.stdout)
            finally:
                Path(tarball_path).unlink(missing_ok=True)
        finally:
            shutil.rmtree(pkg_dir.parent)

    def test_list_json_with_package(self):
        """Test list --json after installing a package."""
        pkg_dir = self.create_test_package()
        try:
            with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
                tarball_path = f.name

            try:
                self.run_pkg("build", str(pkg_dir), tarball_path)
                self.run_pkg("install", tarball_path)

                result = self.run_pkg("list", "--json")
                self.assertEqual(result.returncode, 0)
                output = json.loads(result.stdout)
                self.assertEqual(len(output["packages"]), 1)
                self.assertEqual(output["packages"][0]["name"], "test-pkg")
                self.assertEqual(output["packages"][0]["version"], "1.0.0")
            finally:
                Path(tarball_path).unlink(missing_ok=True)
        finally:
            shutil.rmtree(pkg_dir.parent)

    # Info tests
    def test_info_requires_name(self):
        """Test info requires package name."""
        result = self.run_pkg("info")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("package name required", result.stderr)

    def test_info_not_installed(self):
        """Test info for package not installed."""
        result = self.run_pkg("info", "nonexistent")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not installed", result.stderr)

    def test_info_success(self):
        """Test info for installed package."""
        pkg_dir = self.create_test_package()
        try:
            with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
                tarball_path = f.name

            try:
                self.run_pkg("build", str(pkg_dir), tarball_path)
                self.run_pkg("install", tarball_path)

                result = self.run_pkg("info", "test-pkg")
                self.assertEqual(result.returncode, 0)
                self.assertIn("Name:", result.stdout)
                self.assertIn("test-pkg", result.stdout)
                self.assertIn("Version:", result.stdout)
                self.assertIn("1.0.0", result.stdout)
                self.assertIn("Files:", result.stdout)
            finally:
                Path(tarball_path).unlink(missing_ok=True)
        finally:
            shutil.rmtree(pkg_dir.parent)

    def test_info_json(self):
        """Test info --json for installed package."""
        pkg_dir = self.create_test_package()
        try:
            with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
                tarball_path = f.name

            try:
                self.run_pkg("build", str(pkg_dir), tarball_path)
                self.run_pkg("install", tarball_path)

                result = self.run_pkg("info", "test-pkg", "--json")
                self.assertEqual(result.returncode, 0)
                output = json.loads(result.stdout)
                self.assertEqual(output["name"], "test-pkg")
                self.assertEqual(output["version"], "1.0.0")
                self.assertIn("files", output)
            finally:
                Path(tarball_path).unlink(missing_ok=True)
        finally:
            shutil.rmtree(pkg_dir.parent)

    # Remove tests
    def test_remove_requires_name(self):
        """Test remove requires package name."""
        result = self.run_pkg("remove")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("package name required", result.stderr)

    def test_remove_not_installed(self):
        """Test remove for package not installed."""
        result = self.run_pkg("remove", "nonexistent")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not installed", result.stderr)

    def test_remove_success(self):
        """Test successful package removal."""
        pkg_dir = self.create_test_package()
        try:
            with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
                tarball_path = f.name

            try:
                self.run_pkg("build", str(pkg_dir), tarball_path)
                self.run_pkg("install", tarball_path)

                # Verify installed
                install_dir = self.JSHELL_HOME / "pkgs" / "test-pkg-1.0.0"
                self.assertTrue(install_dir.exists())

                # Remove
                result = self.run_pkg("remove", "test-pkg")
                self.assertEqual(result.returncode, 0)
                self.assertIn("Removed test-pkg", result.stdout)

                # Verify removed
                self.assertFalse(install_dir.exists())
                bin_link = self.JSHELL_HOME / "bin" / "hello"
                self.assertFalse(bin_link.exists())

                # Verify not in list
                result = self.run_pkg("list")
                self.assertIn("No packages installed", result.stdout)
            finally:
                Path(tarball_path).unlink(missing_ok=True)
        finally:
            shutil.rmtree(pkg_dir.parent)

    def test_remove_json(self):
        """Test remove with --json output."""
        pkg_dir = self.create_test_package()
        try:
            with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as f:
                tarball_path = f.name

            try:
                self.run_pkg("build", str(pkg_dir), tarball_path)
                self.run_pkg("install", tarball_path)

                result = self.run_pkg("remove", "test-pkg", "--json")
                self.assertEqual(result.returncode, 0)
                output = json.loads(result.stdout)
                self.assertEqual(output["status"], "ok")
                self.assertEqual(output["name"], "test-pkg")
                self.assertEqual(output["version"], "1.0.0")
            finally:
                Path(tarball_path).unlink(missing_ok=True)
        finally:
            shutil.rmtree(pkg_dir.parent)

    # Search tests (future - should return not implemented)
    def test_search_not_implemented(self):
        """Test search returns not implemented."""
        result = self.run_pkg("search", "test")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    # Check-update tests (future - should return not implemented)
    def test_check_update_not_implemented(self):
        """Test check-update returns not implemented."""
        result = self.run_pkg("check-update")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    # Upgrade tests (future - should return not implemented)
    def test_upgrade_not_implemented(self):
        """Test upgrade returns not implemented."""
        result = self.run_pkg("upgrade")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    def test_unknown_command(self):
        """Test unknown subcommand shows error."""
        result = self.run_pkg("unknown")
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
