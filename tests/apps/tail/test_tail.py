#!/usr/bin/env python3
"""Unit tests for the tail command."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestTailCommand(unittest.TestCase):
    """Test cases for the tail command."""

    TAIL_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "tail"

    @classmethod
    def setUpClass(cls):
        """Verify the tail binary exists before running tests."""
        if not cls.TAIL_BIN.exists():
            raise unittest.SkipTest(f"tail binary not found at {cls.TAIL_BIN}")

    def run_tail(self, *args):
        """Run the tail command with given arguments and return result."""
        cmd = [str(self.TAIL_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_tail("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: tail", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)
        self.assertIn("-n", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_tail("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: tail", result.stdout)

    def test_missing_file_argument(self):
        """Test error when no file argument is provided."""
        result = self.run_tail()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr.lower())

    def test_default_lines(self):
        """Test tail with default 10 lines."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            for i in range(1, 16):
                f.write(f"line{i}\n")
            temp_path = f.name

        try:
            result = self.run_tail(temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            self.assertEqual(len(lines), 10)
            self.assertEqual(lines[0], "line6")
            self.assertEqual(lines[9], "line15")
        finally:
            os.unlink(temp_path)

    def test_custom_line_count(self):
        """Test -n option for custom line count."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            for i in range(1, 11):
                f.write(f"line{i}\n")
            temp_path = f.name

        try:
            result = self.run_tail("-n", "3", temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            self.assertEqual(len(lines), 3)
            self.assertEqual(lines[0], "line8")
            self.assertEqual(lines[1], "line9")
            self.assertEqual(lines[2], "line10")
        finally:
            os.unlink(temp_path)

    def test_n_zero_lines(self):
        """Test -n 0 outputs no lines."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = self.run_tail("-n", "0", temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout, "")
        finally:
            os.unlink(temp_path)

    def test_n_more_than_file_lines(self):
        """Test -n with count greater than file lines."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = self.run_tail("-n", "100", temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            self.assertEqual(len(lines), 3)
            self.assertEqual(lines[0], "line1")
            self.assertEqual(lines[2], "line3")
        finally:
            os.unlink(temp_path)

    def test_negative_line_count(self):
        """Test negative line count is rejected."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("line1\nline2\n")
            temp_path = f.name

        try:
            result = self.run_tail("-n", "-5", temp_path)
            self.assertNotEqual(result.returncode, 0)
        finally:
            os.unlink(temp_path)

    def test_empty_file(self):
        """Test reading an empty file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            temp_path = f.name

        try:
            result = self.run_tail(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout, "")
        finally:
            os.unlink(temp_path)

    def test_json_output(self):
        """Test --json flag outputs valid JSON."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = self.run_tail("--json", temp_path)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertIn("path", data)
            self.assertIn("lines", data)
            self.assertEqual(data["path"], temp_path)
            self.assertIsInstance(data["lines"], list)
            self.assertEqual(len(data["lines"]), 3)
            self.assertEqual(data["lines"][0], "line1")
            self.assertEqual(data["lines"][1], "line2")
            self.assertEqual(data["lines"][2], "line3")
        finally:
            os.unlink(temp_path)

    def test_json_with_custom_n(self):
        """Test --json with -n option."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            for i in range(1, 11):
                f.write(f"line{i}\n")
            temp_path = f.name

        try:
            result = self.run_tail("--json", "-n", "5", temp_path)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data["lines"]), 5)
            self.assertEqual(data["lines"][0], "line6")
            self.assertEqual(data["lines"][4], "line10")
        finally:
            os.unlink(temp_path)

    def test_json_empty_file(self):
        """Test --json with empty file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            temp_path = f.name

        try:
            result = self.run_tail("--json", temp_path)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["lines"], [])
        finally:
            os.unlink(temp_path)

    def test_json_escapes_special_characters(self):
        """Test JSON output properly escapes special characters."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write('line with "quotes"\n')
            f.write("line with\ttab\n")
            f.write("line with backslash \\\n")
            temp_path = f.name

        try:
            result = self.run_tail("--json", temp_path)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data["lines"]), 3)
            self.assertIn("quotes", data["lines"][0])
            self.assertIn("\t", data["lines"][1])
            self.assertIn("\\", data["lines"][2])
        finally:
            os.unlink(temp_path)

    def test_nonexistent_file(self):
        """Test error handling for nonexistent file."""
        result = self.run_tail("/nonexistent/path/that/does/not/exist")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No such file", result.stderr)

    def test_nonexistent_file_json(self):
        """Test JSON error output for nonexistent file."""
        result = self.run_tail("--json", "/nonexistent/path/file.txt")
        self.assertNotEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIn("error", data)
        self.assertIn("path", data)

    def test_special_characters_in_path(self):
        """Test handling of special characters in file paths."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "file with spaces.txt")
            test_file.write_text("content with spaces in path\n")

            result = self.run_tail(str(test_file))
            self.assertEqual(result.returncode, 0)
            self.assertIn("content with spaces in path", result.stdout)

    def test_json_special_characters_in_path(self):
        """Test JSON output with special characters in file path."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, 'file"with"quotes.txt')
            try:
                test_file.write_text("content\n")
                result = self.run_tail("--json", str(test_file))
                self.assertEqual(result.returncode, 0)
                data = json.loads(result.stdout)
                self.assertIn("quotes", data["path"])
            except OSError:
                self.skipTest("Filesystem doesn't support quotes in filenames")

    def test_large_file(self):
        """Test reading last lines of a larger file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            for i in range(10000):
                f.write(f"line{i}\n")
            temp_path = f.name

        try:
            result = self.run_tail(temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            self.assertEqual(len(lines), 10)
            self.assertEqual(lines[0], "line9990")
            self.assertEqual(lines[9], "line9999")
        finally:
            os.unlink(temp_path)

    def test_file_with_no_trailing_newline(self):
        """Test file without trailing newline."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("line1\nline2\nline3")
            temp_path = f.name

        try:
            result = self.run_tail("-n", "3", temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            self.assertEqual(len(lines), 3)
            self.assertEqual(lines[2], "line3")
        finally:
            os.unlink(temp_path)

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_tail("-z", "/tmp/somefile")
        self.assertNotEqual(result.returncode, 0)

    def test_single_line_file(self):
        """Test file with single line."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("only line\n")
            temp_path = f.name

        try:
            result = self.run_tail(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout.strip(), "only line")
        finally:
            os.unlink(temp_path)

    def test_json_single_line_file(self):
        """Test JSON output with single line file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("only line\n")
            temp_path = f.name

        try:
            result = self.run_tail("--json", temp_path)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data["lines"]), 1)
            self.assertEqual(data["lines"][0], "only line")
        finally:
            os.unlink(temp_path)

    def test_tail_shows_last_lines(self):
        """Test that tail specifically shows the LAST lines."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("first\nmiddle\nlast\n")
            temp_path = f.name

        try:
            result = self.run_tail("-n", "1", temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout.strip(), "last")
        finally:
            os.unlink(temp_path)

    def test_tail_n2_shows_last_two(self):
        """Test that tail -n 2 shows the last two lines."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("first\nsecond\nthird\nfourth\n")
            temp_path = f.name

        try:
            result = self.run_tail("-n", "2", temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            self.assertEqual(len(lines), 2)
            self.assertEqual(lines[0], "third")
            self.assertEqual(lines[1], "fourth")
        finally:
            os.unlink(temp_path)


if __name__ == "__main__":
    unittest.main()
