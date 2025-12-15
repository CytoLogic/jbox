#!/usr/bin/env python3
"""Unit tests for the edit-insert-line builtin command."""

import json
import os
import tempfile
import unittest
from pathlib import Path

from tests.helpers import JShellRunner


class TestEditInsertLineBuiltin(unittest.TestCase):
    """Test cases for the edit-insert-line builtin command."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_help_short(self):
        """Test -h flag shows help."""
        result = JShellRunner.run("edit-insert-line -h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: edit-insert-line", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = JShellRunner.run("edit-insert-line --help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: edit-insert-line", result.stdout)

    def test_missing_arguments(self):
        """Test error when arguments are missing."""
        result = JShellRunner.run("edit-insert-line")
        self.assertTrue(
            "error" in result.stderr.lower() or "error" in result.stdout.lower()
            or "missing" in result.stderr.lower()
            or "required" in result.stderr.lower()
        )

    def test_insert_line(self):
        """Test basic line insertion."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-insert-line "{temp_path}" "2" "inserted"')
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "line1\ninserted\nline2\nline3\n")
        finally:
            os.unlink(temp_path)

    def test_insert_at_beginning(self):
        """Test inserting at the beginning."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-insert-line "{temp_path}" "1" "new first"')
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "new first\nline1\nline2\n")
        finally:
            os.unlink(temp_path)

    def test_insert_at_end(self):
        """Test inserting at the end (append)."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-insert-line "{temp_path}" "3" "new last"')
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "line1\nline2\nnew last\n")
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
                f'edit-insert-line --json "{temp_path}" "1" "inserted"')
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
                f'edit-insert-line "{temp_path}" "0" "text"')
            self.assertTrue(
                "error" in result.stderr.lower() or ">= 1" in result.stderr
            )
        finally:
            os.unlink(temp_path)

    def test_line_exceeds_file_length(self):
        """Test error when line number exceeds file length + 1."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-insert-line "{temp_path}" "5" "text"')
            self.assertIn("exceeds", result.stderr)
        finally:
            os.unlink(temp_path)

    def test_line_exceeds_file_length_json(self):
        """Test JSON error when line number exceeds file length + 1."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\n")
            temp_path = f.name

        try:
            data = JShellRunner.run_json(
                f'edit-insert-line --json "{temp_path}" "5" "text"')
            self.assertEqual(data["status"], "error")
        finally:
            os.unlink(temp_path)

    def test_nonexistent_file(self):
        """Test error when file doesn't exist."""
        result = JShellRunner.run(
            'edit-insert-line "/nonexistent/path/file.txt" "1" "text"')
        self.assertIn("No such file", result.stderr)

    def test_nonexistent_file_json(self):
        """Test JSON error when file doesn't exist."""
        data = JShellRunner.run_json(
            'edit-insert-line --json "/nonexistent/path/file.txt" "1" "text"')
        self.assertEqual(data["status"], "error")

    def test_insert_into_empty_file(self):
        """Test inserting into an empty file."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-insert-line "{temp_path}" "1" "first line"')
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "first line\n")
        finally:
            os.unlink(temp_path)

    def test_insert_with_empty_string(self):
        """Test inserting an empty string."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-insert-line "{temp_path}" "2" ""')
            self.assertEqual(result.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "line1\n\nline2\n")
        finally:
            os.unlink(temp_path)

    def test_preserve_other_lines(self):
        """Test that other lines are preserved."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("line1\nline2\nline3\n")
            temp_path = f.name

        try:
            result = JShellRunner.run(
                f'edit-insert-line "{temp_path}" "2" "INSERTED"')
            self.assertEqual(result.returncode, 0)

            lines = Path(temp_path).read_text().split("\n")
            self.assertEqual(lines[0], "line1")
            self.assertEqual(lines[1], "INSERTED")
            self.assertEqual(lines[2], "line2")
            self.assertEqual(lines[3], "line3")
        finally:
            os.unlink(temp_path)

    def test_multiple_insertions(self):
        """Test multiple consecutive insertions."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False,
                                          suffix=".txt") as f:
            f.write("original\n")
            temp_path = f.name

        try:
            result1 = JShellRunner.run(
                f'edit-insert-line "{temp_path}" "1" "first insert"')
            self.assertEqual(result1.returncode, 0)

            result2 = JShellRunner.run(
                f'edit-insert-line "{temp_path}" "1" "second insert"')
            self.assertEqual(result2.returncode, 0)

            content = Path(temp_path).read_text()
            self.assertEqual(content, "second insert\nfirst insert\noriginal\n")
        finally:
            os.unlink(temp_path)


if __name__ == "__main__":
    unittest.main()
