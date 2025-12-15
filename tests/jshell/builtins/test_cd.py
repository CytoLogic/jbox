#!/usr/bin/env python3
"""Unit tests for the cd builtin command."""

import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestCdBuiltin(unittest.TestCase):
    """Test cases for the cd builtin command."""

    JBOX = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX}")

    def run_cmd(self, *args):
        """Run the builtin command via jbox and return result."""
        cmd_str = "cd " + " ".join(f'"{arg}"' for arg in args)
        result = subprocess.run(
            [str(self.JBOX)],
            input=cmd_str + "\n",
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        stdout = result.stdout
        stdout = re.sub(r'\[DEBUG\]:.*\n', '', stdout)
        stdout = stdout.replace('welcome to jbox!\n', '')
        stdout = stdout.replace('(jsh)>', '')
        result.stdout = stdout.strip()
        stderr = result.stderr
        stderr = re.sub(r'\[DEBUG\]:.*\n', '', stderr)
        result.stderr = stderr.strip()
        return result

    def run_cmds(self, *cmds):
        """Run multiple commands via jbox and return result."""
        cmd_str = "\n".join(cmds)
        result = subprocess.run(
            [str(self.JBOX)],
            input=cmd_str + "\n",
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        stdout = result.stdout
        stdout = re.sub(r'\[DEBUG\]:.*\n', '', stdout)
        stdout = stdout.replace('welcome to jbox!\n', '')
        stdout = stdout.replace('(jsh)>', '')
        result.stdout = stdout.strip()
        stderr = result.stderr
        stderr = re.sub(r'\[DEBUG\]:.*\n', '', stderr)
        result.stderr = stderr.strip()
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_cmd("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: cd", result.stdout)
        self.assertIn("--help", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_cmd("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: cd", result.stdout)

    def test_cd_to_tmp(self):
        """Test cd to /tmp and verify with pwd."""
        result = self.run_cmds("cd /tmp", "pwd")
        self.assertEqual(result.returncode, 0)
        self.assertIn("/tmp", result.stdout)

    def test_cd_to_home(self):
        """Test cd without arguments goes to HOME."""
        home = os.environ.get("HOME", "")
        if not home:
            self.skipTest("HOME not set")
        result = self.run_cmds("cd", "pwd")
        self.assertEqual(result.returncode, 0)
        self.assertIn(home, result.stdout)

    def test_cd_nonexistent_dir(self):
        """Test cd to nonexistent directory shows error."""
        result = self.run_cmd("/nonexistent/path/that/does/not/exist")
        self.assertIn("No such file or directory", result.stderr)

    def test_cd_to_file(self):
        """Test cd to a file shows error."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path)
            self.assertIn("Not a directory", result.stderr)
        finally:
            os.unlink(temp_path)

    def test_cd_preserves_change(self):
        """Test that cd changes persist for subsequent commands."""
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self.run_cmds(f"cd {tmpdir}", "pwd")
            self.assertEqual(result.returncode, 0)
            self.assertIn(tmpdir, result.stdout)


if __name__ == "__main__":
    unittest.main()
