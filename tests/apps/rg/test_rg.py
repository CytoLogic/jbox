#!/usr/bin/env python3
"""Unit tests for the rg command."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestRgCommand(unittest.TestCase):
    """Test cases for the rg command."""

    RG_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "rg"

    @classmethod
    def setUpClass(cls):
        """Verify the rg binary exists before running tests."""
        if not cls.RG_BIN.exists():
            raise unittest.SkipTest(f"rg binary not found at {cls.RG_BIN}")

    def run_rg(self, *args, input_data=None):
        """Run the rg command with given arguments and return result."""
        cmd = [str(self.RG_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            input=input_data,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_rg("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: rg", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_rg("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: rg", result.stdout)

    def test_missing_pattern(self):
        """Test error when no pattern is provided."""
        result = self.run_rg()
        self.assertNotEqual(result.returncode, 0)

    def test_basic_search(self):
        """Test basic pattern search in a file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("hello world\nfoo bar\nhello again\n")
            temp_path = f.name

        try:
            result = self.run_rg("hello", temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("hello world", result.stdout)
            self.assertIn("hello again", result.stdout)
            self.assertNotIn("foo bar", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_regex_search(self):
        """Test regex pattern search."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("test123\ntest456\nnotest\n")
            temp_path = f.name

        try:
            result = self.run_rg("test[0-9]+", temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("test123", result.stdout)
            self.assertIn("test456", result.stdout)
            self.assertNotIn("notest", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_line_numbers(self):
        """Test -n flag shows line numbers."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\nmatch here\nline4\n")
            temp_path = f.name

        try:
            result = self.run_rg("-n", "match", temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("3:", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_ignore_case(self):
        """Test -i flag for case-insensitive search."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("Hello\nHELLO\nhello\nworld\n")
            temp_path = f.name

        try:
            result = self.run_rg("-i", "hello", temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            self.assertEqual(len(lines), 3)
        finally:
            os.unlink(temp_path)

    def test_word_match(self):
        """Test -w flag for whole word matching."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("test\ntesting\ntest123\na test here\n")
            temp_path = f.name

        try:
            result = self.run_rg("-w", "test", temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            self.assertEqual(len(lines), 2)
            self.assertIn("test", result.stdout)
            self.assertIn("a test here", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_fixed_strings(self):
        """Test --fixed-strings flag treats pattern as literal."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("test.file\ntest file\ntestXfile\n")
            temp_path = f.name

        try:
            result = self.run_rg("--fixed-strings", "test.file", temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            self.assertEqual(len(lines), 1)
            self.assertIn("test.file", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_context_lines(self):
        """Test -C flag shows context lines."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\nmatch\nline4\nline5\n")
            temp_path = f.name

        try:
            result = self.run_rg("-C", "1", "match", temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("line2", result.stdout)
            self.assertIn("match", result.stdout)
            self.assertIn("line4", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_json_output(self):
        """Test --json flag outputs valid JSON."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("hello world\nfoo bar\n")
            temp_path = f.name

        try:
            result = self.run_rg("--json", "hello", temp_path)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertIsInstance(data, list)
            self.assertEqual(len(data), 1)
            self.assertEqual(data[0]["file"], temp_path)
            self.assertEqual(data[0]["line"], 1)
            self.assertIn("column", data[0])
            self.assertEqual(data[0]["text"], "hello world")
        finally:
            os.unlink(temp_path)

    def test_json_multiple_matches(self):
        """Test JSON output with multiple matches."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("hello one\nworld\nhello two\n")
            temp_path = f.name

        try:
            result = self.run_rg("--json", "hello", temp_path)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data), 2)
            self.assertEqual(data[0]["line"], 1)
            self.assertEqual(data[1]["line"], 3)
        finally:
            os.unlink(temp_path)

    def test_multiple_files(self):
        """Test searching multiple files."""
        with tempfile.TemporaryDirectory() as tmpdir:
            file1 = Path(tmpdir, "file1.txt")
            file2 = Path(tmpdir, "file2.txt")
            file1.write_text("match in file1\n")
            file2.write_text("match in file2\n")

            result = self.run_rg("match", str(file1), str(file2))
            self.assertEqual(result.returncode, 0)
            self.assertIn("file1", result.stdout)
            self.assertIn("file2", result.stdout)

    def test_multiple_files_json(self):
        """Test JSON output with multiple files."""
        with tempfile.TemporaryDirectory() as tmpdir:
            file1 = Path(tmpdir, "file1.txt")
            file2 = Path(tmpdir, "file2.txt")
            file1.write_text("match in file1\n")
            file2.write_text("match in file2\n")

            result = self.run_rg("--json", "match", str(file1), str(file2))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data), 2)
            files = {d["file"] for d in data}
            self.assertIn(str(file1), files)
            self.assertIn(str(file2), files)

    def test_no_match(self):
        """Test exit code when no match is found."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("hello world\n")
            temp_path = f.name

        try:
            result = self.run_rg("nonexistent", temp_path)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(result.stdout.strip(), "")
        finally:
            os.unlink(temp_path)

    def test_no_match_json(self):
        """Test JSON output when no match is found."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("hello world\n")
            temp_path = f.name

        try:
            result = self.run_rg("--json", "nonexistent", temp_path)
            self.assertEqual(result.returncode, 1)
            data = json.loads(result.stdout)
            self.assertEqual(data, [])
        finally:
            os.unlink(temp_path)

    def test_nonexistent_file(self):
        """Test error handling for nonexistent file."""
        result = self.run_rg("pattern", "/nonexistent/path/file.txt")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No such file", result.stderr)

    def test_nonexistent_file_json(self):
        """Test JSON error output for nonexistent file."""
        result = self.run_rg("--json", "pattern", "/nonexistent/path/file.txt")
        data = json.loads(result.stdout)
        self.assertEqual(len(data), 1)
        self.assertIn("error", data[0])

    def test_stdin_search(self):
        """Test searching stdin when no file is provided."""
        result = self.run_rg("hello", input_data="hello world\nfoo bar\n")
        self.assertEqual(result.returncode, 0)
        self.assertIn("hello world", result.stdout)
        self.assertNotIn("foo bar", result.stdout)

    def test_stdin_json(self):
        """Test JSON output when reading from stdin."""
        result = self.run_rg("--json", "hello", input_data="hello world\n")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["file"], "(stdin)")

    def test_invalid_regex(self):
        """Test error handling for invalid regex pattern."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("test content\n")
            temp_path = f.name

        try:
            result = self.run_rg("[invalid", temp_path)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("invalid pattern", result.stderr)
        finally:
            os.unlink(temp_path)

    def test_empty_file(self):
        """Test searching an empty file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            temp_path = f.name

        try:
            result = self.run_rg("pattern", temp_path)
            self.assertEqual(result.returncode, 1)
        finally:
            os.unlink(temp_path)

    def test_special_characters_in_content(self):
        """Test handling of special characters in file content."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write('line with "quotes"\nline with\ttab\n')
            temp_path = f.name

        try:
            result = self.run_rg("--json", "quotes", temp_path)
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            self.assertEqual(len(data), 1)
            self.assertIn("quotes", data[0]["text"])
        finally:
            os.unlink(temp_path)

    def test_column_number_json(self):
        """Test that column number is correct in JSON output."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("prefix match suffix\n")
            temp_path = f.name

        try:
            result = self.run_rg("--json", "match", temp_path)
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            self.assertEqual(len(data), 1)
            self.assertEqual(data[0]["column"], 8)
        finally:
            os.unlink(temp_path)

    def test_context_separator(self):
        """Test that context groups are separated."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nmatch1\nline3\nline4\nline5\nline6\nmatch2\nline8\n")
            temp_path = f.name

        try:
            result = self.run_rg("-C", "1", "match", temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("--", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_combined_flags(self):
        """Test combining multiple flags."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("TEST one\ntest two\nTEST three\n")
            temp_path = f.name

        try:
            result = self.run_rg("-i", "-n", "test", temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            self.assertEqual(len(lines), 3)
            self.assertIn("1:", result.stdout)
            self.assertIn("2:", result.stdout)
            self.assertIn("3:", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_regex_special_chars_fixed_strings(self):
        """Test that --fixed-strings escapes regex special characters."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("test.*pattern\ntest pattern\ntestXpattern\n")
            temp_path = f.name

        try:
            result = self.run_rg("--fixed-strings", "test.*pattern", temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            self.assertEqual(len(lines), 1)
            self.assertIn("test.*pattern", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_large_file(self):
        """Test searching a larger file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            for i in range(10000):
                f.write(f"line {i}\n")
            f.write("unique match here\n")
            temp_path = f.name

        try:
            result = self.run_rg("unique match", temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("unique match here", result.stdout)
        finally:
            os.unlink(temp_path)


if __name__ == "__main__":
    unittest.main()
