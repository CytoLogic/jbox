#!/usr/bin/env python3
"""Unit tests for the pwd builtin command."""

import json
import os
import tempfile
import unittest
from pathlib import Path

from tests.helpers import JShellRunner


class TestPwdBuiltin(unittest.TestCase):
    """Test cases for the pwd builtin command."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_help_short(self):
        """Test -h flag shows help."""
        result = JShellRunner.run("pwd -h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: pwd", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = JShellRunner.run("pwd --help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: pwd", result.stdout)

    def test_pwd_shows_directory(self):
        """Test pwd shows current working directory."""
        result = JShellRunner.run("pwd")
        self.assertEqual(result.returncode, 0)
        self.assertTrue(result.stdout.startswith("/"))

    def test_pwd_after_cd(self):
        """Test pwd after changing directory with cd."""
        result = JShellRunner.run("cd /tmp; pwd")
        self.assertEqual(result.returncode, 0)
        self.assertIn("/tmp", result.stdout)

    def test_pwd_json(self):
        """Test --json flag outputs valid JSON."""
        data = JShellRunner.run_json("pwd --json")
        self.assertIn("cwd", data)
        self.assertTrue(data["cwd"].startswith("/"))

    def test_pwd_json_after_cd(self):
        """Test --json output after cd."""
        result = JShellRunner.run("cd /tmp; pwd --json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIn("cwd", data)
        self.assertIn("/tmp", data["cwd"])


if __name__ == "__main__":
    unittest.main()
