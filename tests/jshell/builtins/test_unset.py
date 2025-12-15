#!/usr/bin/env python3
"""Unit tests for the unset builtin command."""

import json
import os
import re
import subprocess
import unittest
from pathlib import Path


class TestUnsetBuiltin(unittest.TestCase):
    """Test cases for the unset builtin command."""

    JBOX = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX}")

    def run_cmd(self, *args):
        """Run the builtin command via jbox and return result."""
        if args:
            cmd_str = "unset " + " ".join(f'"{arg}"' for arg in args)
        else:
            cmd_str = "unset"
        result = subprocess.run(
            [str(self.JBOX)],
            input=cmd_str + "\n",
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0",
                 "TEST_VAR_TO_UNSET": "test_value"}
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
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0",
                 "TEST_VAR_TO_UNSET": "test_value"}
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
        self.assertIn("Usage: unset", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_cmd("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: unset", result.stdout)

    def test_unset_variable(self):
        """Test unset removes an environment variable."""
        result = self.run_cmds("unset TEST_VAR_TO_UNSET", "env")
        self.assertEqual(result.returncode, 0)
        self.assertNotIn("TEST_VAR_TO_UNSET=", result.stdout)

    def test_unset_multiple_variables(self):
        """Test unset removes multiple environment variables."""
        result = self.run_cmds(
            'export "VAR1=value1" "VAR2=value2"',
            "unset VAR1 VAR2",
            "env"
        )
        self.assertEqual(result.returncode, 0)
        self.assertNotIn("VAR1=", result.stdout)
        self.assertNotIn("VAR2=", result.stdout)

    def test_unset_json(self):
        """Test --json flag outputs valid JSON."""
        result = self.run_cmd("--json", "TEST_VAR_TO_UNSET")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIsInstance(data, list)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["key"], "TEST_VAR_TO_UNSET")
        self.assertEqual(data[0]["status"], "ok")

    def test_unset_nonexistent_variable(self):
        """Test unset on nonexistent variable still succeeds."""
        result = self.run_cmd("NONEXISTENT_VAR_12345")
        self.assertEqual(result.returncode, 0)

    def test_unset_missing_argument(self):
        """Test unset with no arguments shows error."""
        result = self.run_cmd()
        self.assertTrue(
            "missing" in result.stderr.lower() or "missing" in result.stdout.lower()
        )


if __name__ == "__main__":
    unittest.main()
