#!/usr/bin/env python3
"""Unit tests for the type builtin command."""

import json
import unittest
from pathlib import Path

from tests.helpers import JShellRunner


class TestTypeBuiltin(unittest.TestCase):
    """Test cases for the type builtin command."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_help_short(self):
        """Test -h flag shows help."""
        result = JShellRunner.run("type -h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: type", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = JShellRunner.run("type --help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: type", result.stdout)

    def test_type_builtin(self):
        """Test type identifies builtins correctly."""
        result = JShellRunner.run("type cd")
        self.assertEqual(result.returncode, 0)
        self.assertIn("cd is a shell builtin", result.stdout)

    def test_type_builtin_pwd(self):
        """Test type identifies pwd as builtin."""
        result = JShellRunner.run("type pwd")
        self.assertEqual(result.returncode, 0)
        self.assertIn("pwd is a shell builtin", result.stdout)

    def test_type_external_registered(self):
        """Test type identifies registered external commands."""
        result = JShellRunner.run("type ls")
        self.assertEqual(result.returncode, 0)
        self.assertIn("external", result.stdout.lower())

    def test_type_path_lookup(self):
        """Test type finds commands in PATH."""
        result = JShellRunner.run("type bash")
        self.assertEqual(result.returncode, 0)
        self.assertIn("/bin/bash", result.stdout)

    def test_type_not_found(self):
        """Test type reports not found for nonexistent commands."""
        result = JShellRunner.run("type nonexistent_command_12345")
        self.assertIn("not found", result.stderr)

    def test_type_json_builtin(self):
        """Test --json output for builtin."""
        data = JShellRunner.run_json("type --json cd")
        self.assertIsInstance(data, list)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["name"], "cd")
        self.assertEqual(data[0]["kind"], "builtin")

    def test_type_json_external_path(self):
        """Test --json output for external command with path."""
        data = JShellRunner.run_json("type --json bash")
        self.assertIsInstance(data, list)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["name"], "bash")
        self.assertIn("path", data[0])
        self.assertIn("/bash", data[0]["path"])

    def test_type_json_not_found(self):
        """Test --json output for not found command."""
        data = JShellRunner.run_json("type --json nonexistent_command_12345")
        self.assertIsInstance(data, list)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["name"], "nonexistent_command_12345")
        self.assertEqual(data[0]["kind"], "not found")

    def test_type_multiple(self):
        """Test type with multiple arguments."""
        result = JShellRunner.run("type cd pwd bash")
        self.assertEqual(result.returncode, 0)
        self.assertIn("cd is a shell builtin", result.stdout)
        self.assertIn("pwd is a shell builtin", result.stdout)
        self.assertIn("bash is", result.stdout)


if __name__ == "__main__":
    unittest.main()
