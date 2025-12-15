#!/usr/bin/env python3
"""Shell integration tests for package manager.

Tests pkg commands when run through jshell -c.
"""

import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path

from tests.pkg import PkgTestBase
from tests.helpers import JShellRunner


class TestPkgViaShell(PkgTestBase):
    """Test pkg commands via jshell -c."""

    @classmethod
    def setUpClass(cls):
        """Verify jshell exists before running tests."""
        super().setUpClass()
        if not JShellRunner.exists():
            raise unittest.SkipTest(f"jshell not found at {JShellRunner.JSHELL}")

    def test_pkg_list_via_shell(self):
        """Test 'pkg list' via jshell -c."""
        self.install_test_package("shell-pkg", "1.0.0")

        result = self.run_shell("pkg list --json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIn("packages", data)

        names = {p["name"] for p in data["packages"]}
        self.assertIn("shell-pkg", names)

    def test_pkg_info_via_shell(self):
        """Test 'pkg info' via jshell -c."""
        self.install_test_package("info-test", "2.0.0")

        result = self.run_shell("pkg info info-test --json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertEqual(data["name"], "info-test")
        self.assertEqual(data["version"], "2.0.0")

    def test_pkg_check_update_via_shell(self):
        """Test 'pkg check-update' via jshell -c."""
        self.install_test_package("update-test", "1.0.0")

        result = self.run_shell("pkg check-update --json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIn("status", data)
        self.assertIn("summary", data)

    def test_pkg_help_via_shell(self):
        """Test 'pkg --help' via jshell -c."""
        result = self.run_shell("pkg --help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("pkg", result.stdout.lower())

    def test_multiple_pkg_commands_via_shell(self):
        """Test multiple pkg commands in sequence."""
        # Install a package
        self.install_test_package("multi-test", "1.0.0")

        # Run multiple commands
        commands = [
            "pkg list --json",
            "pkg info multi-test --json",
        ]

        for cmd in commands:
            result = self.run_shell(cmd)
            self.assertEqual(result.returncode, 0, f"Command '{cmd}' failed")
            # Should be valid JSON
            json.loads(result.stdout)


class TestPkgShellPipelines(PkgTestBase):
    """Test pkg output in shell pipelines."""

    @classmethod
    def setUpClass(cls):
        """Verify jshell exists before running tests."""
        super().setUpClass()
        if not JShellRunner.exists():
            raise unittest.SkipTest(f"jshell not found at {JShellRunner.JSHELL}")

    def test_list_count_packages(self):
        """Test counting packages via shell."""
        # Install 3 packages
        self.install_test_package("count-a", "1.0.0")
        self.install_test_package("count-b", "1.0.0")
        self.install_test_package("count-c", "1.0.0")

        result = self.run_shell("pkg list --json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertEqual(len(data["packages"]), 3)

    def test_json_output_parseable(self):
        """Test that JSON output is machine-parseable."""
        self.install_test_package("json-test", "1.0.0")

        result = self.run_shell("pkg list --json")
        self.assertEqual(result.returncode, 0)

        # Output should be valid JSON
        data = json.loads(result.stdout)

        # Verify expected structure
        self.assertIn("packages", data)
        self.assertIsInstance(data["packages"], list)


class TestInstalledPackageExecution(PkgTestBase):
    """Test that installed packages can be executed via shell."""

    @classmethod
    def setUpClass(cls):
        """Verify jshell exists before running tests."""
        super().setUpClass()
        if not JShellRunner.exists():
            raise unittest.SkipTest(f"jshell not found at {JShellRunner.JSHELL}")

    def test_installed_binary_in_path(self):
        """Test that installed package binary is accessible."""
        self.install_test_package("exec-test", "1.0.0")

        # Verify binary exists
        bin_path = self.JSHELL_HOME / "bin" / "exec-test"
        self.assertTrue(bin_path.exists() or bin_path.is_symlink())

        # The binary should be executable
        self.assertTrue(os.access(str(bin_path), os.X_OK))


class TestPkgShellEnvironment(PkgTestBase):
    """Test pkg behavior in different shell environments."""

    @classmethod
    def setUpClass(cls):
        """Verify jshell exists before running tests."""
        super().setUpClass()
        if not JShellRunner.exists():
            raise unittest.SkipTest(f"jshell not found at {JShellRunner.JSHELL}")

    def test_pkg_with_custom_cwd(self):
        """Test pkg commands from different working directory."""
        tmpdir = tempfile.mkdtemp()
        try:
            self.install_test_package("cwd-test", "1.0.0")

            result = JShellRunner.run("pkg list --json", cwd=tmpdir)
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)

            names = {p["name"] for p in data["packages"]}
            self.assertIn("cwd-test", names)
        finally:
            shutil.rmtree(tmpdir)

    def test_pkg_with_env_var(self):
        """Test pkg commands with custom environment."""
        self.install_test_package("env-test", "1.0.0")

        result = JShellRunner.run(
            "pkg list --json",
            env={"CUSTOM_VAR": "test_value"}
        )
        self.assertEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
