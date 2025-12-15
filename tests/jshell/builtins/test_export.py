#!/usr/bin/env python3
"""Unit tests for the export builtin command."""

import json
import os
import re
import subprocess
import unittest
from pathlib import Path


class TestExportBuiltin(unittest.TestCase):
    """Test cases for the export builtin command."""

    JBOX = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX}")

    def run_cmd(self, *args):
        """Run the builtin command via jbox and return result."""
        if args:
            cmd_str = "export " + " ".join(f'"{arg}"' for arg in args)
        else:
            cmd_str = "export"
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
        self.assertIn("Usage: export", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_cmd("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: export", result.stdout)

    def test_export_sets_variable(self):
        """Test export sets an environment variable."""
        result = self.run_cmds('export "MY_VAR=my_value"', "env")
        self.assertEqual(result.returncode, 0)
        self.assertIn("MY_VAR=my_value", result.stdout)

    def test_export_multiple_variables(self):
        """Test export sets multiple environment variables."""
        result = self.run_cmds(
            'export "VAR1=value1" "VAR2=value2"',
            "env"
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("VAR1=value1", result.stdout)
        self.assertIn("VAR2=value2", result.stdout)

    def test_export_overwrites_variable(self):
        """Test export overwrites an existing variable."""
        result = self.run_cmds(
            'export "MY_VAR=first"',
            'export "MY_VAR=second"',
            "env --json"
        )
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertEqual(data["env"]["MY_VAR"], "second")

    def test_export_invalid_format(self):
        """Test export with invalid format shows error."""
        result = self.run_cmd("invalid")
        self.assertIn("not a valid identifier", result.stderr)

    def test_export_json(self):
        """Test --json flag outputs valid JSON."""
        result = self.run_cmd("--json", "MY_VAR=test_value")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIsInstance(data, list)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["key"], "MY_VAR")
        self.assertEqual(data[0]["value"], "test_value")
        self.assertEqual(data[0]["status"], "ok")

    def test_export_empty_value(self):
        """Test export with empty value."""
        result = self.run_cmds('export "EMPTY_VAR="', "env --json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertEqual(data["env"]["EMPTY_VAR"], "")

    def test_export_value_with_equals(self):
        """Test export with value containing equals sign."""
        result = self.run_cmds('export "EQ_VAR=a=b=c"', "env --json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertEqual(data["env"]["EQ_VAR"], "a=b=c")


if __name__ == "__main__":
    unittest.main()
