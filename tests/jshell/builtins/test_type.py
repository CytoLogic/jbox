#!/usr/bin/env python3
"""Unit tests for the type builtin command."""

import json
import os
import re
import subprocess
import unittest
from pathlib import Path


class TestTypeBuiltin(unittest.TestCase):
    """Test cases for the type builtin command."""

    JBOX = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX}")

    def run_cmd(self, *args):
        """Run the builtin command via jbox and return result."""
        if args:
            cmd_str = "type " + " ".join(args)
        else:
            cmd_str = "type"
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
        self.assertIn("Usage: type", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_cmd("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: type", result.stdout)

    def test_type_builtin(self):
        """Test type identifies builtins correctly."""
        result = self.run_cmd("cd")
        self.assertEqual(result.returncode, 0)
        self.assertIn("cd is a shell builtin", result.stdout)

    def test_type_builtin_pwd(self):
        """Test type identifies pwd as builtin."""
        result = self.run_cmd("pwd")
        self.assertEqual(result.returncode, 0)
        self.assertIn("pwd is a shell builtin", result.stdout)

    def test_type_external_registered(self):
        """Test type identifies registered external commands."""
        result = self.run_cmd("ls")
        self.assertEqual(result.returncode, 0)
        self.assertIn("external", result.stdout.lower())

    def test_type_path_lookup(self):
        """Test type finds commands in PATH."""
        result = self.run_cmd("bash")
        self.assertEqual(result.returncode, 0)
        self.assertIn("/bin/bash", result.stdout)

    def test_type_not_found(self):
        """Test type reports not found for nonexistent commands."""
        result = self.run_cmd("nonexistent_command_12345")
        self.assertIn("not found", result.stderr)

    def test_type_json_builtin(self):
        """Test --json output for builtin."""
        result = self.run_cmd("--json", "cd")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIsInstance(data, list)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["name"], "cd")
        self.assertEqual(data[0]["kind"], "builtin")

    def test_type_json_external_path(self):
        """Test --json output for external command with path."""
        result = self.run_cmd("--json", "bash")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIsInstance(data, list)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["name"], "bash")
        self.assertIn("path", data[0])
        self.assertIn("/bash", data[0]["path"])

    def test_type_json_not_found(self):
        """Test --json output for not found command."""
        result = self.run_cmd("--json", "nonexistent_command_12345")
        data = json.loads(result.stdout)
        self.assertIsInstance(data, list)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["name"], "nonexistent_command_12345")
        self.assertEqual(data[0]["kind"], "not found")

    def test_type_multiple(self):
        """Test type with multiple arguments."""
        result = self.run_cmd("cd", "pwd", "bash")
        self.assertEqual(result.returncode, 0)
        self.assertIn("cd is a shell builtin", result.stdout)
        self.assertIn("pwd is a shell builtin", result.stdout)
        self.assertIn("bash is", result.stdout)


if __name__ == "__main__":
    unittest.main()
