#!/usr/bin/env python3
"""Unit tests for the edit-replace-line builtin command."""

import json
import os
import tempfile
import unittest
from pathlib import Path

from tests.helpers import JShellRunner


class TestEditReplaceLineBuiltin(unittest.TestCase):
    """Test cases for the edit-replace-line builtin command."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_help_short(self):
        """Test -h flag shows help."""
        result = JShellRunner.run("edit-replace-line -h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: edit-replace-line", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = JShellRunner.run("edit-replace-line --help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: edit-replace-line", result.stdout)

    def test_missing_arguments(self):
        """Test error when arguments are missing."""
        result = JShellRunner.run("edit-replace-line")
        self.assertTrue(
            "error" in result.stderr.lower() or "error" in result.stdout.lower()
            or "missing" in result.stderr.lower()
            or "required" in result.stderr.lower()
        )

    def test_replace_line(self):
        """Test basic line replacement."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-replace-line "{temp_path}" "2" "replaced"')
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "line1\nreplaced\nline3\n")
        finally:
            os.unlink(temp_path)

    def test_replace_first_line(self):
        """Test replacing the first line."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-replace-line "{temp_path}" "1" "new first line"')
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "new first line\nline2\nline3\n")
        finally:
            os.unlink(temp_path)

    def test_replace_last_line(self):
        """Test replacing the last line."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-replace-line "{temp_path}" "3" "new last line"')
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "line1\nline2\nnew last line\n")
        finally:
            os.unlink(temp_path)

    def test_json_output(self):
        """Test --json flag outputs valid JSON."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\n")
            temp_path = f.name

        try:
            data = JShellRunner.run_json(
                f'edit-replace-line --json "{temp_path}" "1" "replaced"')
            self.assertEqual(data["path"], temp_path)
            self.assertEqual(data["line"], 1)
            self.assertEqual(data["status"], "ok")
        finally:
            os.unlink(temp_path)

    def test_line_number_zero(self):
        """Test error when line number is 0."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-replace-line "{temp_path}" "0" "text"')
            self.assertTrue(
                "error" in result.stderr.lower() or ">= 1" in result.stderr
            )
        finally:
            os.unlink(temp_path)

    def test_line_exceeds_file_length(self):
        """Test error when line number exceeds file length."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-replace-line "{temp_path}" "5" "text"')
            self.assertIn("exceeds", result.stderr)
        finally:
            os.unlink(temp_path)

    def test_line_exceeds_file_length_json(self):
        """Test JSON error when line number exceeds file length."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\n")
            temp_path = f.name

        try:
            data = JShellRunner.run_json(
                f'edit-replace-line --json "{temp_path}" "5" "text"')
            self.assertEqual(data["status"], "error")
            self.assertIn("exceeds", data["message"])
        finally:
            os.unlink(temp_path)

    def test_nonexistent_file(self):
        """Test error when file doesn't exist."""
        result = JShellRunner.run(
            'edit-replace-line "/nonexistent/path/file.txt" "1" "text"')
        self.assertIn("No such file", result.stderr)

    def test_nonexistent_file_json(self):
        """Test JSON error when file doesn't exist."""
        data = JShellRunner.run_json(
            'edit-replace-line --json "/nonexistent/path/file.txt" "1" "text"')
        self.assertEqual(data["status"], "error")

    def test_replace_with_empty_string(self):
        """Test replacing with empty string."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-replace-line "{temp_path}" "2" ""')
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "line1\n\nline3\n")
        finally:
            os.unlink(temp_path)

    def test_single_line_file(self):
        """Test editing a single-line file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("only line\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-replace-line "{temp_path}" "1" "replaced line"')
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "replaced line\n")
        finally:
            os.unlink(temp_path)

    def test_preserve_other_lines(self):
        """Test that other lines are preserved."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\nline3\nline4\nline5\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-replace-line "{temp_path}" "3" "REPLACED"')
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
