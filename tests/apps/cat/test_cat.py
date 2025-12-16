#!/usr/bin/env python3
"""Unit tests for the cat command."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestCatCommand(unittest.TestCase):
    """Test cases for the cat command."""

    CAT_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "cat"

    @classmethod
    def setUpClass(cls):
        """Verify the cat binary exists before running tests."""
        if not cls.CAT_BIN.exists():
            raise unittest.SkipTest(f"cat binary not found at {cls.CAT_BIN}")

    def run_cat(self, *args):
        """Run the cat command with given arguments and return result."""
        cmd = [str(self.CAT_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_cat("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: cat", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_cat("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: cat", result.stdout)

    def test_stdin_when_no_file(self):
        """Test reading from stdin when no file argument is provided."""
        result = subprocess.run(
            [str(self.CAT_BIN)],
            input="hello from stdin",
            capture_output=True,
            text=True,
            timeout=5,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "hello from stdin")

    def test_single_file(self):
        """Test reading a single file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("hello world")
            temp_path = f.name

        try:
            result = self.run_cat(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout, "hello world")
        finally:
            os.unlink(temp_path)

    def test_multiple_files(self):
        """Test reading multiple files."""
        with tempfile.TemporaryDirectory() as tmpdir:
            file1 = Path(tmpdir, "file1.txt")
            file2 = Path(tmpdir, "file2.txt")
            file1.write_text("first file\n")
            file2.write_text("second file\n")

            result = self.run_cat(str(file1), str(file2))
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout, "first file\nsecond file\n")

    def test_empty_file(self):
        """Test reading an empty file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            temp_path = f.name

        try:
            result = self.run_cat(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout, "")
        finally:
            os.unlink(temp_path)

    def test_json_output(self):
        """Test --json flag outputs valid JSON."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("test content")
            temp_path = f.name

        try:
            result = self.run_cat("--json", temp_path)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertIsInstance(data, list)
            self.assertEqual(len(data), 1)
            self.assertEqual(data[0]["path"], temp_path)
            self.assertEqual(data[0]["content"], "test content")
        finally:
            os.unlink(temp_path)

    def test_json_multiple_files(self):
        """Test --json with multiple files."""
        with tempfile.TemporaryDirectory() as tmpdir:
            file1 = Path(tmpdir, "file1.txt")
            file2 = Path(tmpdir, "file2.txt")
            file1.write_text("content one")
            file2.write_text("content two")

            result = self.run_cat("--json", str(file1), str(file2))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertIsInstance(data, list)
            self.assertEqual(len(data), 2)
            self.assertEqual(data[0]["content"], "content one")
            self.assertEqual(data[1]["content"], "content two")

    def test_json_escapes_special_characters(self):
        """Test JSON output properly escapes special characters in content."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write('line1\nline2\ttab\r"quoted"')
            temp_path = f.name

        try:
            result = self.run_cat("--json", temp_path)
            self.assertEqual(result.returncode, 0)

            # Should be valid JSON even with special chars
            data = json.loads(result.stdout)
            self.assertEqual(len(data), 1)
            self.assertIn("line1", data[0]["content"])
            self.assertIn("line2", data[0]["content"])
            self.assertIn("quoted", data[0]["content"])
        finally:
            os.unlink(temp_path)

    def test_nonexistent_file(self):
        """Test error handling for nonexistent file."""
        result = self.run_cat("/nonexistent/path/that/does/not/exist")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No such file", result.stderr)

    def test_nonexistent_file_json(self):
        """Test JSON error output for nonexistent file."""
        result = self.run_cat("--json", "/nonexistent/path/file.txt")
        self.assertNotEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertEqual(len(data), 1)
        self.assertIn("error", data[0])
        self.assertIn("path", data[0])

    def test_partial_failure(self):
        """Test partial failure when some files don't exist."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("valid content")
            temp_path = f.name

        try:
            result = self.run_cat(temp_path, "/nonexistent/file.txt")
            self.assertNotEqual(result.returncode, 0)
            # Should still output the valid file
            self.assertIn("valid content", result.stdout)
            # Should report error for invalid file
            self.assertIn("No such file", result.stderr)
        finally:
            os.unlink(temp_path)

    def test_binary_file(self):
        """Test reading a binary file."""
        with tempfile.NamedTemporaryFile(mode="wb", delete=False) as f:
            binary_content = bytes([0, 1, 2, 3, 255, 254, 253])
            f.write(binary_content)
            temp_path = f.name

        try:
            # Use bytes mode for binary file test
            cmd = [str(self.CAT_BIN), temp_path]
            result = subprocess.run(
                cmd,
                capture_output=True,
                env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
            )
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout, binary_content)
        finally:
            os.unlink(temp_path)

    def test_special_characters_in_path(self):
        """Test handling of special characters in file paths."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "file with spaces.txt")
            test_file.write_text("content with spaces in path")

            result = self.run_cat(str(test_file))
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout, "content with spaces in path")

    def test_json_special_characters_in_path(self):
        """Test JSON output with special characters in file path."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, 'file"with"quotes.txt')
            try:
                test_file.write_text("content")
                result = self.run_cat("--json", str(test_file))
                self.assertEqual(result.returncode, 0)
                # Should be valid JSON even with special chars in path
                data = json.loads(result.stdout)
                self.assertEqual(len(data), 1)
                self.assertIn("quotes", data[0]["path"])
            except OSError:
                self.skipTest("Filesystem doesn't support quotes in filenames")

    def test_large_file(self):
        """Test reading a larger file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            content = "line\n" * 10000
            f.write(content)
            temp_path = f.name

        try:
            result = self.run_cat(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout, content)
        finally:
            os.unlink(temp_path)

    def test_multiline_content(self):
        """Test reading file with multiple lines."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = self.run_cat(temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.split("\n")
            self.assertEqual(lines[0], "line1")
            self.assertEqual(lines[1], "line2")
            self.assertEqual(lines[2], "line3")
        finally:
            os.unlink(temp_path)

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_cat("-z", "/tmp/somefile")
        self.assertNotEqual(result.returncode, 0)

    def test_preserve_trailing_newline(self):
        """Test that trailing newlines are preserved."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("content\n")
            temp_path = f.name

        try:
            result = self.run_cat(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertTrue(result.stdout.endswith("\n"))
        finally:
            os.unlink(temp_path)

    def test_no_trailing_newline(self):
        """Test file without trailing newline."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("no newline at end")
            temp_path = f.name

        try:
            result = self.run_cat(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout, "no newline at end")
            self.assertFalse(result.stdout.endswith("\n"))
        finally:
            os.unlink(temp_path)


if __name__ == "__main__":
    unittest.main()
