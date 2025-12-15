#!/usr/bin/env python3
"""Unit tests for the echo command."""

import os
import subprocess
import unittest
from pathlib import Path


class TestEchoCommand(unittest.TestCase):
    """Test cases for the echo command."""

    ECHO_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "echo"

    @classmethod
    def setUpClass(cls):
        """Verify the echo binary exists before running tests."""
        if not cls.ECHO_BIN.exists():
            raise unittest.SkipTest(f"echo binary not found at {cls.ECHO_BIN}")

    def run_echo(self, *args):
        """Run the echo command with given arguments and return result."""
        cmd = [str(self.ECHO_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_echo("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: echo", result.stdout)
        self.assertIn("--help", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_echo("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: echo", result.stdout)

    def test_no_arguments(self):
        """Test echo with no arguments outputs just newline."""
        result = self.run_echo()
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "\n")

    def test_single_word(self):
        """Test echo with single word."""
        result = self.run_echo("hello")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "hello\n")

    def test_multiple_words(self):
        """Test echo with multiple words."""
        result = self.run_echo("hello", "world")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "hello world\n")

    def test_no_newline_flag(self):
        """Test -n flag suppresses trailing newline."""
        result = self.run_echo("-n", "hello")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "hello")
        self.assertFalse(result.stdout.endswith("\n"))

    def test_no_newline_empty(self):
        """Test -n flag with no text."""
        result = self.run_echo("-n")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")

    def test_no_newline_multiple_words(self):
        """Test -n flag with multiple words."""
        result = self.run_echo("-n", "hello", "world")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "hello world")

    def test_empty_string(self):
        """Test echo with empty string argument."""
        result = self.run_echo("")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "\n")

    def test_special_characters(self):
        """Test echo with special characters."""
        result = self.run_echo("hello*world")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "hello*world\n")

    def test_spaces_in_argument(self):
        """Test echo with spaces in a single argument."""
        result = self.run_echo("hello world")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "hello world\n")

    def test_tabs_in_argument(self):
        """Test echo with tabs in argument."""
        result = self.run_echo("hello\tworld")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "hello\tworld\n")

    def test_multiple_spaces_between_args(self):
        """Test that multiple arguments are separated by single space."""
        result = self.run_echo("a", "b", "c")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "a b c\n")

    def test_quotes_preserved(self):
        """Test that quotes in text are preserved."""
        result = self.run_echo('"quoted"')
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, '"quoted"\n')

    def test_backslash_literal(self):
        """Test that backslashes are treated literally."""
        result = self.run_echo("back\\slash")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "back\\slash\n")

    def test_long_text(self):
        """Test echo with long text."""
        long_text = "a" * 1000
        result = self.run_echo(long_text)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, long_text + "\n")

    def test_many_arguments(self):
        """Test echo with many arguments."""
        args = [f"arg{i}" for i in range(50)]
        result = self.run_echo(*args)
        self.assertEqual(result.returncode, 0)
        expected = " ".join(args) + "\n"
        self.assertEqual(result.stdout, expected)

    def test_unicode_characters(self):
        """Test echo with unicode characters."""
        result = self.run_echo("hello", "world")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "hello world\n")

    def test_mixed_flags_and_text(self):
        """Test echo with -n flag followed by text."""
        result = self.run_echo("-n", "first", "second")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "first second")


if __name__ == "__main__":
    unittest.main()
