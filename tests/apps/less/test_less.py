#!/usr/bin/env python3
"""Unit tests for the less command."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestLessCommand(unittest.TestCase):
    """Test cases for the less command."""

    LESS_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "less"

    @classmethod
    def setUpClass(cls):
        """Verify the less binary exists before running tests."""
        if not cls.LESS_BIN.exists():
            raise unittest.SkipTest(f"less binary not found at {cls.LESS_BIN}")

    def run_less(self, *args, stdin_data=None):
        """Run the less command with given arguments and return result."""
        cmd = [str(self.LESS_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            input=stdin_data,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_less("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: less", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("-N", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_less("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: less", result.stdout)

    def test_help_navigation_info(self):
        """Test that help includes navigation instructions."""
        result = self.run_less("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Navigation:", result.stdout)
        self.assertIn("Scroll", result.stdout)
        self.assertIn("Quit", result.stdout)

    def test_noninteractive_single_file(self):
        """Test reading a single file in non-interactive mode (stdout piped)."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = self.run_less(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("line1", result.stdout)
            self.assertIn("line2", result.stdout)
            self.assertIn("line3", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_noninteractive_with_line_numbers(self):
        """Test -N flag adds line numbers in non-interactive mode."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("first\nsecond\nthird\n")
            temp_path = f.name

        try:
            result = self.run_less("-N", temp_path)
            self.assertEqual(result.returncode, 0)
            lines = result.stdout.strip().split("\n")
            # Line numbers should appear at start of output lines
            self.assertTrue(any("1" in line and "first" in line for line in lines))
            self.assertTrue(any("2" in line and "second" in line for line in lines))
            self.assertTrue(any("3" in line and "third" in line for line in lines))
        finally:
            os.unlink(temp_path)

    def test_nonexistent_file(self):
        """Test error handling for nonexistent file."""
        result = self.run_less("/nonexistent/path/that/does/not/exist")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No such file", result.stderr)

    def test_empty_file(self):
        """Test reading an empty file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            temp_path = f.name

        try:
            result = self.run_less(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout.strip(), "")
        finally:
            os.unlink(temp_path)

    def test_large_file(self):
        """Test reading a larger file in non-interactive mode."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            content_lines = [f"line {i}" for i in range(100)]
            f.write("\n".join(content_lines) + "\n")
            temp_path = f.name

        try:
            result = self.run_less(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("line 0", result.stdout)
            self.assertIn("line 50", result.stdout)
            self.assertIn("line 99", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_special_characters_in_path(self):
        """Test handling of special characters in file paths."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "file with spaces.txt")
            test_file.write_text("content with spaces in path\n")

            result = self.run_less(str(test_file))
            self.assertEqual(result.returncode, 0)
            self.assertIn("content with spaces in path", result.stdout)

    def test_file_without_trailing_newline(self):
        """Test file without trailing newline."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("no newline at end")
            temp_path = f.name

        try:
            result = self.run_less(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("no newline at end", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_less("-z", "/tmp/somefile")
        self.assertNotEqual(result.returncode, 0)

    def test_unicode_content(self):
        """Test reading file with unicode content."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, encoding="utf-8") as f:
            f.write("Hello\nWorld\n")
            temp_path = f.name

        try:
            result = self.run_less(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("Hello", result.stdout)
            self.assertIn("World", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_tabs_in_content(self):
        """Test reading file with tab characters."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("col1\tcol2\tcol3\n")
            temp_path = f.name

        try:
            result = self.run_less(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("col1", result.stdout)
            self.assertIn("col2", result.stdout)
        finally:
            os.unlink(temp_path)


if __name__ == "__main__":
    unittest.main()
