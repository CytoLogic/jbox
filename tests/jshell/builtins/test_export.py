#!/usr/bin/env python3
"""Unit tests for the export builtin command."""

import json
import unittest
from pathlib import Path

from tests.helpers import JShellRunner


class TestExportBuiltin(unittest.TestCase):
    """Test cases for the export builtin command."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_help_short(self):
        """Test -h flag shows help."""
        result = JShellRunner.run("export -h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: export", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = JShellRunner.run("export --help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: export", result.stdout)

    def test_export_sets_variable(self):
        """Test export sets an environment variable."""
        result = JShellRunner.run('export "MY_VAR=my_value"; env')
        self.assertEqual(result.returncode, 0)
        self.assertIn("MY_VAR=my_value", result.stdout)

    def test_export_multiple_variables(self):
        """Test export sets multiple environment variables."""
        result = JShellRunner.run('export "VAR1=value1" "VAR2=value2"; env')
        self.assertEqual(result.returncode, 0)
        self.assertIn("VAR1=value1", result.stdout)
        self.assertIn("VAR2=value2", result.stdout)

    def test_export_overwrites_variable(self):
        """Test export overwrites an existing variable."""
        result = JShellRunner.run(
            'export "MY_VAR=first"; export "MY_VAR=second"; env --json')
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertEqual(data["env"]["MY_VAR"], "second")

    def test_export_invalid_format(self):
        """Test export with invalid format shows error."""
        result = JShellRunner.run("export invalid")
        self.assertIn("not a valid identifier", result.stderr)

    def test_export_json(self):
        """Test --json flag outputs valid JSON."""
        result = JShellRunner.run('export --json "MY_VAR=test_value"')
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIsInstance(data, list)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["key"], "MY_VAR")
        self.assertEqual(data[0]["value"], "test_value")
        self.assertEqual(data[0]["status"], "ok")

    def test_export_empty_value(self):
        """Test export with empty value."""
        result = JShellRunner.run('export "EMPTY_VAR="; env --json')
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertEqual(data["env"]["EMPTY_VAR"], "")

    def test_export_value_with_equals(self):
        """Test export with value containing equals sign."""
        result = JShellRunner.run('export "EQ_VAR=a=b=c"; env --json')
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertEqual(data["env"]["EQ_VAR"], "a=b=c")


if __name__ == "__main__":
    unittest.main()
