#!/usr/bin/env python3
"""Unit tests for the rmdir command."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestRmdirCommand(unittest.TestCase):
    """Test cases for the rmdir command."""

    RMDIR_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "rmdir"

    @classmethod
    def setUpClass(cls):
        """Verify the rmdir binary exists before running tests."""
        if not cls.RMDIR_BIN.exists():
            raise unittest.SkipTest(f"rmdir binary not found at {cls.RMDIR_BIN}")

    def run_rmdir(self, *args):
        """Run the rmdir command with given arguments and return result."""
        cmd = [str(self.RMDIR_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_rmdir("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: rmdir", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_rmdir("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: rmdir", result.stdout)

    def test_missing_arguments(self):
        """Test error when no arguments are provided."""
        result = self.run_rmdir()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr.lower())

    def test_remove_empty_directory(self):
        """Test removing an empty directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            empty_dir = Path(tmpdir, "emptydir")
            empty_dir.mkdir()

            result = self.run_rmdir(str(empty_dir))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(empty_dir.exists())

    def test_remove_multiple_directories(self):
        """Test removing multiple directories."""
        with tempfile.TemporaryDirectory() as tmpdir:
            dir1 = Path(tmpdir, "dir1")
            dir2 = Path(tmpdir, "dir2")
            dir1.mkdir()
            dir2.mkdir()

            result = self.run_rmdir(str(dir1), str(dir2))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(dir1.exists())
            self.assertFalse(dir2.exists())

    def test_remove_nonempty_directory(self):
        """Test error when removing non-empty directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            nonempty = Path(tmpdir, "nonempty")
            nonempty.mkdir()
            (nonempty / "file.txt").write_text("content")

            result = self.run_rmdir(str(nonempty))
            self.assertNotEqual(result.returncode, 0)
            self.assertTrue(nonempty.exists())

    def test_remove_nonexistent_directory(self):
        """Test error when directory doesn't exist."""
        result = self.run_rmdir("/nonexistent/directory")
        self.assertNotEqual(result.returncode, 0)

    def test_remove_file(self):
        """Test error when trying to remove a file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "file.txt")
            test_file.write_text("content")

            result = self.run_rmdir(str(test_file))
            self.assertNotEqual(result.returncode, 0)
            self.assertTrue(test_file.exists())

    def test_json_output_success(self):
        """Test JSON output on success."""
        with tempfile.TemporaryDirectory() as tmpdir:
            empty_dir = Path(tmpdir, "emptydir")
            empty_dir.mkdir()

            result = self.run_rmdir("--json", str(empty_dir))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertIsInstance(data, list)
            self.assertEqual(len(data), 1)
            self.assertEqual(data[0]["path"], str(empty_dir))
            self.assertEqual(data[0]["status"], "ok")

    def test_json_output_multiple(self):
        """Test JSON output with multiple directories."""
        with tempfile.TemporaryDirectory() as tmpdir:
            dir1 = Path(tmpdir, "dir1")
            dir2 = Path(tmpdir, "dir2")
            dir1.mkdir()
            dir2.mkdir()

            result = self.run_rmdir("--json", str(dir1), str(dir2))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data), 2)
            self.assertEqual(data[0]["status"], "ok")
            self.assertEqual(data[1]["status"], "ok")

    def test_json_output_error(self):
        """Test JSON output on error."""
        result = self.run_rmdir("--json", "/nonexistent/directory")
        self.assertNotEqual(result.returncode, 0)

        data = json.loads(result.stdout)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["status"], "error")
        self.assertIn("message", data[0])

    def test_json_output_partial_failure(self):
        """Test JSON output with partial failure."""
        with tempfile.TemporaryDirectory() as tmpdir:
            empty_dir = Path(tmpdir, "emptydir")
            empty_dir.mkdir()

            result = self.run_rmdir(
                "--json",
                str(empty_dir),
                "/nonexistent/directory"
            )
            self.assertNotEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data), 2)
            self.assertEqual(data[0]["status"], "ok")
            self.assertEqual(data[1]["status"], "error")

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_rmdir("-z", "/tmp/somedir")
        self.assertNotEqual(result.returncode, 0)

    def test_directory_with_spaces_in_name(self):
        """Test removing directory with spaces in name."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_dir = Path(tmpdir, "dir with spaces")
            test_dir.mkdir()

            result = self.run_rmdir(str(test_dir))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(test_dir.exists())


if __name__ == "__main__":
    unittest.main()
