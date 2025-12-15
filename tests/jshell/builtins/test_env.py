#!/usr/bin/env python3
"""Unit tests for the env builtin command."""

import json
import os
import unittest
from pathlib import Path

from tests.helpers import JShellRunner


class TestEnvBuiltin(unittest.TestCase):
    """Test cases for the env builtin command."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_help_short(self):
        """Test -h flag shows help."""
        result = JShellRunner.run("env -h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: env", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = JShellRunner.run("env --help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: env", result.stdout)

    def test_env_lists_variables(self):
        """Test env lists environment variables."""
        result = JShellRunner.run("env", env={"TEST_VAR": "test_value"})
        self.assertEqual(result.returncode, 0)
        self.assertIn("PATH=", result.stdout)
        self.assertIn("TEST_VAR=test_value", result.stdout)

    def test_env_json(self):
        """Test --json flag outputs valid JSON."""
        data = JShellRunner.run_json("env --json", env={"TEST_VAR": "test_value"})
        self.assertIn("env", data)
        self.assertIsInstance(data["env"], dict)
        self.assertIn("PATH", data["env"])
        self.assertEqual(data["env"]["TEST_VAR"], "test_value")

    def test_env_json_has_home(self):
        """Test JSON output includes HOME variable."""
        data = JShellRunner.run_json("env --json")
        if "HOME" in os.environ:
            self.assertIn("HOME", data["env"])


if __name__ == "__main__":
    unittest.main()
