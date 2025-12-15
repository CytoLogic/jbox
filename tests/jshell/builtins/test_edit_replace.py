#!/usr/bin/env python3
"""Unit tests for the edit-replace builtin command."""

import json
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestEditReplaceBuiltin(unittest.TestCase):
    """Test cases for the edit-replace builtin command."""

    JBOX = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX}")

    def run_cmd(self, *args):
        """Run the builtin command via jbox and return result."""
        cmd_str = "edit-replace " + " ".join(f'"{arg}"' for arg in args)
        result = subprocess.run(
            [str(self.JBOX)],
            input=cmd_str + "\n",
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        # Strip shell prompt and welcome message from stdout
        stdout = result.stdout
        # Remove debug messages and prompts
        stdout = re.sub(r'\[DEBUG\]:.*\n', '', stdout)
        stdout = stdout.replace('welcome to jbox!\n', '')
        stdout = stdout.replace('(jsh)>', '')
        result.stdout = stdout.strip()
        # Process stderr too
        stderr = result.stderr
        stderr = re.sub(r'\[DEBUG\]:.*\n', '', stderr)
        result.stderr = stderr.strip()
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_cmd("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: edit-replace", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)
        self.assertIn("--fixed-strings", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_cmd("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: edit-replace", result.stdout)

    def test_missing_arguments(self):
        """Test error when arguments are missing."""
        result = self.run_cmd()
        # Shell doesn't propagate exit codes, so check for error output
        self.assertTrue(
            "error" in result.stderr.lower() or "error" in result.stdout.lower()
            or "missing" in result.stderr.lower() or "required" in result.stderr.lower()
        )

    def test_basic_replace(self):
        """Test basic regex replacement."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("hello world\nhello again\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "hello", "goodbye")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "goodbye world\ngoodbye again\n")
        finally:
            os.unlink(temp_path)

    def test_regex_pattern(self):
        """Test regex pattern matching."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("foo123bar\nfoo456bar\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "foo[0-9]+bar", "replaced")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "replaced\nreplaced\n")
        finally:
            os.unlink(temp_path)

    def test_fixed_strings(self):
        """Test literal string matching with --fixed-strings."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("foo.bar\nfooXbar\n")
            temp_path = f.name

        try:
            # Without --fixed-strings, . matches any char
            result = self.run_cmd(temp_path, "foo.bar", "replaced")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "replaced\nreplaced\n")  # Both match
        finally:
            os.unlink(temp_path)

    def test_fixed_strings_literal(self):
        """Test literal dot matching with --fixed-strings."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("foo.bar\nfooXbar\n")
            temp_path = f.name

        try:
            result = self.run_cmd("--fixed-strings", temp_path, "foo.bar", "replaced")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "replaced\nfooXbar\n")  # Only literal match
        finally:
            os.unlink(temp_path)

    def test_case_insensitive(self):
        """Test case-insensitive matching."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("Hello\nHELLO\nhello\n")
            temp_path = f.name

        try:
            result = self.run_cmd("-i", temp_path, "hello", "hi")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "hi\nhi\nhi\n")
        finally:
            os.unlink(temp_path)

    def test_json_output(self):
        """Test --json flag outputs valid JSON."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("foo bar foo\nbaz foo\n")
            temp_path = f.name

        try:
            result = self.run_cmd("--json", temp_path, "foo", "X")
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["path"], temp_path)
            self.assertEqual(data["status"], "ok")
            self.assertEqual(data["matches"], 3)
            self.assertEqual(data["replacements"], 3)
        finally:
            os.unlink(temp_path)

    def test_no_matches(self):
        """Test when pattern has no matches."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("hello world\n")
            temp_path = f.name

        try:
            result = self.run_cmd("--json", temp_path, "xyz", "abc")
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["status"], "ok")
            self.assertEqual(data["matches"], 0)
            self.assertEqual(data["replacements"], 0)

            # File should be unchanged
            content = Path(temp_path).read_text()
            self.assertEqual(content, "hello world\n")
        finally:
            os.unlink(temp_path)

    def test_invalid_regex(self):
        """Test error with invalid regex pattern."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("test\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "[invalid", "replacement")
            # Check for error message
            self.assertTrue(
                "error" in result.stderr.lower() or "invalid" in result.stderr.lower()
                or "regex" in result.stderr.lower()
            )
        finally:
            os.unlink(temp_path)

    def test_nonexistent_file(self):
        """Test error when file doesn't exist."""
        result = self.run_cmd("/nonexistent/path/file.txt", "pattern", "replacement")
        self.assertIn("No such file", result.stderr)

    def test_nonexistent_file_json(self):
        """Test JSON error when file doesn't exist."""
        result = self.run_cmd("--json", "/nonexistent/path/file.txt", "pattern", "rep")
        data = json.loads(result.stdout)
        self.assertEqual(data["status"], "error")

    def test_multiple_matches_per_line(self):
        """Test multiple matches on the same line."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("aaa bbb aaa ccc aaa\n")
            temp_path = f.name

        try:
            result = self.run_cmd("--json", temp_path, "aaa", "X")
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["matches"], 3)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "X bbb X ccc X\n")
        finally:
            os.unlink(temp_path)

    def test_empty_replacement(self):
        """Test replacing with empty string (deletion)."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("hello world\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "hello ", "")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "world\n")
        finally:
            os.unlink(temp_path)

    def test_preserve_lines_without_matches(self):
        """Test that lines without matches are preserved."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line with foo\nline without\nfoo at start\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "foo", "bar")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "line with bar\nline without\nbar at start\n")
        finally:
            os.unlink(temp_path)


if __name__ == "__main__":
    unittest.main()
