#!/usr/bin/env python3
"""Unit tests for the pwd builtin command."""

import json
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestPwdBuiltin(unittest.TestCase):
    """Test cases for the pwd builtin command."""

    JBOX = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX}")

    def run_cmd(self, *args):
        """Run the builtin command via jbox and return result."""
        if args:
            cmd_str = "pwd " + " ".join(f'"{arg}"' for arg in args)
        else:
            cmd_str = "pwd"
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
        self.assertIn("Usage: pwd", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_cmd("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: pwd", result.stdout)

    def test_pwd_shows_directory(self):
        """Test pwd shows current working directory."""
        result = self.run_cmd()
        self.assertEqual(result.returncode, 0)
        # The output should be a valid path
        self.assertTrue(result.stdout.startswith("/"))

    def test_pwd_after_cd(self):
        """Test pwd after changing directory with cd."""
        result = self.run_cmds("cd /tmp", "pwd")
        self.assertEqual(result.returncode, 0)
        self.assertIn("/tmp", result.stdout)

    def test_pwd_json(self):
        """Test --json flag outputs valid JSON."""
        result = self.run_cmd("--json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIn("cwd", data)
        self.assertTrue(data["cwd"].startswith("/"))

    def test_pwd_json_after_cd(self):
        """Test --json output after cd."""
        result = self.run_cmds("cd /tmp", "pwd --json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIn("cwd", data)
        self.assertIn("/tmp", data["cwd"])


if __name__ == "__main__":
    unittest.main()
