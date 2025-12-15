#!/usr/bin/env python3
"""Unit tests for the edit-replace-line command."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestEditReplaceLineCommand(unittest.TestCase):
    """Test cases for the edit-replace-line command."""

    BIN = Path(__file__).parent.parent.parent.parent / "bin" / "edit-replace-line"

    @classmethod
    def setUpClass(cls):
        """Verify the binary exists before running tests."""
        if not cls.BIN.exists():
            raise unittest.SkipTest(f"edit-replace-line binary not found at {cls.BIN}")

    def run_cmd(self, *args):
        """Run the command with given arguments and return result."""
        cmd = [str(self.BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_cmd("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: edit-replace-line", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_cmd("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: edit-replace-line", result.stdout)

    def test_missing_arguments(self):
        """Test error when arguments are missing."""
        result = self.run_cmd()
        self.assertNotEqual(result.returncode, 0)

    def test_replace_line(self):
        """Test basic line replacement."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "2", "replaced")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "line1\nreplaced\nline3\n")
        finally:
            os.unlink(temp_path)

    def test_replace_first_line(self):
        """Test replacing the first line."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "1", "new first line")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "new first line\nline2\nline3\n")
        finally:
            os.unlink(temp_path)

    def test_replace_last_line(self):
        """Test replacing the last line."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "3", "new last line")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "line1\nline2\nnew last line\n")
        finally:
            os.unlink(temp_path)

    def test_json_output(self):
        """Test --json flag outputs valid JSON."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\n")
            temp_path = f.name

        try:
            result = self.run_cmd("--json", temp_path, "1", "replaced")
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["path"], temp_path)
            self.assertEqual(data["line"], 1)
            self.assertEqual(data["status"], "ok")
        finally:
            os.unlink(temp_path)

    def test_line_number_zero(self):
        """Test error when line number is 0."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "0", "text")
            self.assertNotEqual(result.returncode, 0)
        finally:
            os.unlink(temp_path)

    def test_line_number_negative(self):
        """Test error when line number is negative."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "-1", "text")
            self.assertNotEqual(result.returncode, 0)
        finally:
            os.unlink(temp_path)

    def test_line_exceeds_file_length(self):
        """Test error when line number exceeds file length."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "5", "text")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("exceeds", result.stderr)
        finally:
            os.unlink(temp_path)

    def test_line_exceeds_file_length_json(self):
        """Test JSON error when line number exceeds file length."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\n")
            temp_path = f.name

        try:
            result = self.run_cmd("--json", temp_path, "5", "text")
            self.assertNotEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            self.assertEqual(data["status"], "error")
            self.assertIn("exceeds", data["message"])
        finally:
            os.unlink(temp_path)

    def test_nonexistent_file(self):
        """Test error when file doesn't exist."""
        result = self.run_cmd("/nonexistent/path/file.txt", "1", "text")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No such file", result.stderr)

    def test_nonexistent_file_json(self):
        """Test JSON error when file doesn't exist."""
        result = self.run_cmd("--json", "/nonexistent/path/file.txt", "1", "text")
        self.assertNotEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertEqual(data["status"], "error")

    def test_replace_with_empty_string(self):
        """Test replacing with empty string."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "2", "")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "line1\n\nline3\n")
        finally:
            os.unlink(temp_path)

    def test_replace_with_special_characters(self):
        """Test replacing with special characters."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "1", 'text with "quotes" and $pecial chars')
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertIn('"quotes"', content)
        finally:
            os.unlink(temp_path)

    def test_single_line_file(self):
        """Test editing a single-line file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("only line\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "1", "replaced line")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "replaced line\n")
        finally:
            os.unlink(temp_path)

    def test_preserve_other_lines(self):
        """Test that other lines are preserved."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\nline3\nline4\nline5\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "3", "REPLACED")
            self.assertEqual(result.returncode, 0)

            lines = Path(temp_path).read_text().split("\n")
            self.assertEqual(lines[0], "line1")
            self.assertEqual(lines[1], "line2")
            self.assertEqual(lines[2], "REPLACED")
            self.assertEqual(lines[3], "line4")
            self.assertEqual(lines[4], "line5")
        finally:
            os.unlink(temp_path)

    def test_replace_long_line_with_short(self):
        """Test replacing a long line with a short one."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("this is a very long line\nshort\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "1", "x")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "x\nshort\n")
        finally:
            os.unlink(temp_path)

    def test_replace_short_line_with_long(self):
        """Test replacing a short line with a long one."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("x\nshort\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "1", "this is a much longer replacement line")
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "this is a much longer replacement line\nshort\n")
        finally:
            os.unlink(temp_path)

    def test_json_escapes_special_chars(self):
        """Test that JSON output escapes special characters."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, 'file"with"quotes.txt')
            try:
                test_file.write_text("line1\n")
                result = self.run_cmd("--json", str(test_file), "1", "replaced")
                self.assertEqual(result.returncode, 0)
                # Should be valid JSON
                data = json.loads(result.stdout)
                self.assertIn("quotes", data["path"])
            except OSError:
                self.skipTest("Filesystem doesn't support quotes in filenames")


if __name__ == "__main__":
    unittest.main()
