#!/usr/bin/env python3
"""Unit tests for the edit-replace-line builtin command."""

import json
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestEditReplaceLineBuiltin(unittest.TestCase):
    """Test cases for the edit-replace-line builtin command."""

    JBOX = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX}")

    def run_cmd(self, *args):
        """Run the builtin command via jbox and return result."""
        cmd_str = "edit-replace-line " + " ".join(f'"{arg}"' for arg in args)
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
        # Shell doesn't propagate exit codes, so check for error output
        self.assertTrue(
            "error" in result.stderr.lower() or "error" in result.stdout.lower()
            or "missing" in result.stderr.lower() or "required" in result.stderr.lower()
        )

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
            # Check for error message in output
            self.assertTrue(
                "error" in result.stderr.lower() or ">= 1" in result.stderr
            )
        finally:
            os.unlink(temp_path)

    def test_line_exceeds_file_length(self):
        """Test error when line number exceeds file length."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".txt") as f:
            f.write("line1\nline2\n")
            temp_path = f.name

        try:
            result = self.run_cmd(temp_path, "5", "text")
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
            data = json.loads(result.stdout)
            self.assertEqual(data["status"], "error")
            self.assertIn("exceeds", data["message"])
        finally:
            os.unlink(temp_path)

    def test_nonexistent_file(self):
        """Test error when file doesn't exist."""
        result = self.run_cmd("/nonexistent/path/file.txt", "1", "text")
        self.assertIn("No such file", result.stderr)

    def test_nonexistent_file_json(self):
        """Test JSON error when file doesn't exist."""
        result = self.run_cmd("--json", "/nonexistent/path/file.txt", "1", "text")
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


if __name__ == "__main__":
    unittest.main()
