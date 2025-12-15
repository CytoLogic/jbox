#!/usr/bin/env python3
"""Unit tests for the cd builtin command."""

import os
import tempfile
import unittest
from pathlib import Path

from tests.helpers import JShellRunner


class TestCdBuiltin(unittest.TestCase):
    """Test cases for the cd builtin command."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_help_short(self):
        """Test -h flag shows help."""
        result = JShellRunner.run("cd -h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: cd", result.stdout)
        self.assertIn("--help", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = JShellRunner.run("cd --help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: cd", result.stdout)

    def test_cd_to_tmp(self):
        """Test cd to /tmp and verify with pwd."""
        result = JShellRunner.run("cd /tmp; pwd")
        self.assertEqual(result.returncode, 0)
        self.assertIn("/tmp", result.stdout)

    def test_cd_to_home(self):
        """Test cd without arguments goes to HOME."""
        home = os.environ.get("HOME", "")
        if not home:
            self.skipTest("HOME not set")
        result = JShellRunner.run("cd; pwd")
        self.assertEqual(result.returncode, 0)
        self.assertIn(home, result.stdout)

    def test_cd_nonexistent_dir(self):
        """Test cd to nonexistent directory shows error."""
        result = JShellRunner.run("cd /nonexistent/path/that/does/not/exist")
        self.assertIn("No such file or directory", result.stderr)

    def test_cd_to_file(self):
        """Test cd to a file shows error."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            temp_path = f.name

        try:
            result = JShellRunner.run(f"cd {temp_path}")
            self.assertIn("Not a directory", result.stderr)
        finally:
            os.unlink(temp_path)

    def test_cd_preserves_change(self):
        """Test that cd changes persist for subsequent commands."""
        with tempfile.TemporaryDirectory() as tmpdir:
            result = JShellRunner.run(f"cd {tmpdir}; pwd")
            self.assertEqual(result.returncode, 0)
            self.assertIn(tmpdir, result.stdout)


if __name__ == "__main__":
    unittest.main()
