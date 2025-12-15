#!/usr/bin/env python3
"""Unit tests for the unset builtin command."""

import json
import unittest
from pathlib import Path

from tests.helpers import JShellRunner


class TestUnsetBuiltin(unittest.TestCase):
    """Test cases for the unset builtin command."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_help_short(self):
        """Test -h flag shows help."""
        result = JShellRunner.run("unset -h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: unset", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = JShellRunner.run("unset --help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: unset", result.stdout)

    def test_unset_variable(self):
        """Test unset removes an environment variable."""
        result = JShellRunner.run(
            "unset TEST_VAR_TO_UNSET; env",
            env={"TEST_VAR_TO_UNSET": "test_value"}
        )
        self.assertEqual(result.returncode, 0)
        self.assertNotIn("TEST_VAR_TO_UNSET=", result.stdout)

    def test_unset_multiple_variables(self):
        """Test unset removes multiple environment variables."""
        result = JShellRunner.run(
            'export "VAR1=value1" "VAR2=value2"; unset VAR1 VAR2; env'
        )
        self.assertEqual(result.returncode, 0)
        self.assertNotIn("VAR1=", result.stdout)
        self.assertNotIn("VAR2=", result.stdout)

    def test_unset_json(self):
        """Test --json flag outputs valid JSON."""
        result = JShellRunner.run(
            "unset --json TEST_VAR_TO_UNSET",
            env={"TEST_VAR_TO_UNSET": "test_value"}
        )
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIsInstance(data, list)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["key"], "TEST_VAR_TO_UNSET")
        self.assertEqual(data[0]["status"], "ok")

    def test_unset_nonexistent_variable(self):
        """Test unset on nonexistent variable still succeeds."""
        result = JShellRunner.run("unset NONEXISTENT_VAR_12345")
        self.assertEqual(result.returncode, 0)

    def test_unset_missing_argument(self):
        """Test unset with no arguments shows error."""
        result = JShellRunner.run("unset")
        self.assertTrue(
            "missing" in result.stderr.lower()
            or "missing" in result.stdout.lower()
        )


if __name__ == "__main__":
    unittest.main()
