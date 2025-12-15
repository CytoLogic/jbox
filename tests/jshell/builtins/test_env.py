#!/usr/bin/env python3
"""Unit tests for the env builtin command."""

import json
import os
import re
import subprocess
import unittest
from pathlib import Path


class TestEnvBuiltin(unittest.TestCase):
    """Test cases for the env builtin command."""

    JBOX = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX}")

    def run_cmd(self, *args):
        """Run the builtin command via jbox and return result."""
        if args:
            cmd_str = "env " + " ".join(f'"{arg}"' for arg in args)
        else:
            cmd_str = "env"
        result = subprocess.run(
            [str(self.JBOX)],
            input=cmd_str + "\n",
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0",
                 "TEST_VAR": "test_value"}
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
        self.assertIn("Usage: env", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_cmd("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: env", result.stdout)

    def test_env_lists_variables(self):
        """Test env lists environment variables."""
        result = self.run_cmd()
        self.assertEqual(result.returncode, 0)
        # Should contain PATH at minimum
        self.assertIn("PATH=", result.stdout)
        # Should contain our test variable
        self.assertIn("TEST_VAR=test_value", result.stdout)

    def test_env_json(self):
        """Test --json flag outputs valid JSON."""
        result = self.run_cmd("--json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIn("env", data)
        self.assertIsInstance(data["env"], dict)
        # Check that PATH is in the environment
        self.assertIn("PATH", data["env"])
        # Check our test variable
        self.assertEqual(data["env"]["TEST_VAR"], "test_value")

    def test_env_json_has_home(self):
        """Test JSON output includes HOME variable."""
        result = self.run_cmd("--json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        # HOME should typically be set
        if "HOME" in os.environ:
            self.assertIn("HOME", data["env"])


if __name__ == "__main__":
    unittest.main()
