#!/usr/bin/env python3
"""Unit tests for the touch command."""

import json
import os
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


class TestTouchCommand(unittest.TestCase):
    """Test cases for the touch command."""

    TOUCH_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "touch"

    @classmethod
    def setUpClass(cls):
        """Verify the touch binary exists before running tests."""
        if not cls.TOUCH_BIN.exists():
            raise unittest.SkipTest(f"touch binary not found at {cls.TOUCH_BIN}")

    def run_touch(self, *args):
        """Run the touch command with given arguments and return result."""
        cmd = [str(self.TOUCH_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_touch("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: touch", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_touch("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: touch", result.stdout)

    def test_missing_arguments(self):
        """Test error when no arguments are provided."""
        result = self.run_touch()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr.lower())

    def test_create_new_file(self):
        """Test creating a new file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            new_file = Path(tmpdir, "newfile.txt")

            result = self.run_touch(str(new_file))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(new_file.exists())
            self.assertEqual(new_file.read_text(), "")

    def test_create_multiple_files(self):
        """Test creating multiple files."""
        with tempfile.TemporaryDirectory() as tmpdir:
            file1 = Path(tmpdir, "file1.txt")
            file2 = Path(tmpdir, "file2.txt")

            result = self.run_touch(str(file1), str(file2))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(file1.exists())
            self.assertTrue(file2.exists())

    def test_update_existing_file_timestamp(self):
        """Test updating timestamp of existing file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            existing = Path(tmpdir, "existing.txt")
            existing.write_text("content")

            old_mtime = existing.stat().st_mtime
            time.sleep(0.1)

            result = self.run_touch(str(existing))
            self.assertEqual(result.returncode, 0)

            new_mtime = existing.stat().st_mtime
            self.assertGreater(new_mtime, old_mtime)
            # Content should be preserved
            self.assertEqual(existing.read_text(), "content")

    def test_touch_in_nonexistent_directory(self):
        """Test error when parent directory doesn't exist."""
        result = self.run_touch("/nonexistent/directory/file.txt")
        self.assertNotEqual(result.returncode, 0)

    def test_json_output_success(self):
        """Test JSON output on success."""
        with tempfile.TemporaryDirectory() as tmpdir:
            new_file = Path(tmpdir, "newfile.txt")

            result = self.run_touch("--json", str(new_file))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertIsInstance(data, list)
            self.assertEqual(len(data), 1)
            self.assertEqual(data[0]["path"], str(new_file))
            self.assertEqual(data[0]["status"], "ok")

    def test_json_output_multiple(self):
        """Test JSON output with multiple files."""
        with tempfile.TemporaryDirectory() as tmpdir:
            file1 = Path(tmpdir, "file1.txt")
            file2 = Path(tmpdir, "file2.txt")

            result = self.run_touch("--json", str(file1), str(file2))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data), 2)
            self.assertEqual(data[0]["status"], "ok")
            self.assertEqual(data[1]["status"], "ok")

    def test_json_output_error(self):
        """Test JSON output on error."""
        result = self.run_touch("--json", "/nonexistent/directory/file.txt")
        self.assertNotEqual(result.returncode, 0)

        data = json.loads(result.stdout)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["status"], "error")
        self.assertIn("message", data[0])

    def test_json_output_partial_failure(self):
        """Test JSON output with partial failure."""
        with tempfile.TemporaryDirectory() as tmpdir:
            new_file = Path(tmpdir, "newfile.txt")

            result = self.run_touch(
                "--json",
                str(new_file),
                "/nonexistent/directory/file.txt"
            )
            self.assertNotEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data), 2)
            self.assertEqual(data[0]["status"], "ok")
            self.assertEqual(data[1]["status"], "error")

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_touch("-z", "/tmp/somefile")
        self.assertNotEqual(result.returncode, 0)

    def test_file_with_spaces_in_name(self):
        """Test creating file with spaces in name."""
        with tempfile.TemporaryDirectory() as tmpdir:
            new_file = Path(tmpdir, "file with spaces.txt")

            result = self.run_touch(str(new_file))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(new_file.exists())

    def test_touch_preserves_content(self):
        """Test that touch doesn't modify file content."""
        with tempfile.TemporaryDirectory() as tmpdir:
            existing = Path(tmpdir, "existing.txt")
            content = "original content\nwith multiple lines\n"
            existing.write_text(content)

            result = self.run_touch(str(existing))
            self.assertEqual(result.returncode, 0)
            self.assertEqual(existing.read_text(), content)

    def test_touch_binary_file(self):
        """Test that touch preserves binary file content."""
        with tempfile.TemporaryDirectory() as tmpdir:
            existing = Path(tmpdir, "binary.bin")
            binary_content = bytes([0, 1, 2, 3, 255, 254, 253])
            existing.write_bytes(binary_content)

            result = self.run_touch(str(existing))
            self.assertEqual(result.returncode, 0)
            self.assertEqual(existing.read_bytes(), binary_content)


if __name__ == "__main__":
    unittest.main()
