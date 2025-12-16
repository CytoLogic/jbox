#!/usr/bin/env python3
"""Unit tests for the rm command."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestRmCommand(unittest.TestCase):
    """Test cases for the rm command."""

    RM_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "rm"

    @classmethod
    def setUpClass(cls):
        """Verify the rm binary exists before running tests."""
        if not cls.RM_BIN.exists():
            raise unittest.SkipTest(f"rm binary not found at {cls.RM_BIN}")

    def run_rm(self, *args):
        """Run the rm command with given arguments and return result."""
        cmd = [str(self.RM_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_rm("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: rm", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)
        self.assertIn("-r", result.stdout)
        self.assertIn("-f", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_rm("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: rm", result.stdout)

    def test_missing_arguments(self):
        """Test error when no arguments are provided."""
        result = self.run_rm()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr.lower())

    def test_remove_single_file(self):
        """Test removing a single file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "test.txt")
            test_file.write_text("content")

            result = self.run_rm(str(test_file))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(test_file.exists())

    def test_remove_multiple_files(self):
        """Test removing multiple files."""
        with tempfile.TemporaryDirectory() as tmpdir:
            file1 = Path(tmpdir, "file1.txt")
            file2 = Path(tmpdir, "file2.txt")
            file1.write_text("content1")
            file2.write_text("content2")

            result = self.run_rm(str(file1), str(file2))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(file1.exists())
            self.assertFalse(file2.exists())

    def test_remove_nonexistent_file(self):
        """Test error when file doesn't exist."""
        result = self.run_rm("/nonexistent/file.txt")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("no such file", result.stderr.lower())

    def test_remove_nonexistent_file_force(self):
        """Test -f flag ignores nonexistent files."""
        result = self.run_rm("-f", "/nonexistent/file.txt")
        self.assertEqual(result.returncode, 0)

    def test_remove_directory_without_r(self):
        """Test error when removing directory without -r."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_dir = Path(tmpdir, "testdir")
            test_dir.mkdir()

            result = self.run_rm(str(test_dir))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("directory", result.stderr.lower())
            self.assertTrue(test_dir.exists())

    def test_remove_empty_directory_recursive(self):
        """Test removing empty directory with -r."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_dir = Path(tmpdir, "testdir")
            test_dir.mkdir()

            result = self.run_rm("-r", str(test_dir))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(test_dir.exists())

    def test_remove_directory_with_contents(self):
        """Test removing directory with contents using -r."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_dir = Path(tmpdir, "testdir")
            test_dir.mkdir()
            (test_dir / "file1.txt").write_text("content1")
            (test_dir / "file2.txt").write_text("content2")

            result = self.run_rm("-r", str(test_dir))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(test_dir.exists())

    def test_remove_nested_directories(self):
        """Test removing nested directories."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_dir = Path(tmpdir, "testdir")
            test_dir.mkdir()
            subdir = test_dir / "subdir"
            subdir.mkdir()
            (subdir / "file.txt").write_text("content")

            result = self.run_rm("-r", str(test_dir))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(test_dir.exists())

    def test_json_output_success(self):
        """Test JSON output on success."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "test.txt")
            test_file.write_text("content")

            result = self.run_rm("--json", str(test_file))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertIsInstance(data, list)
            self.assertEqual(len(data), 1)
            self.assertEqual(data[0]["path"], str(test_file))
            self.assertEqual(data[0]["status"], "ok")

    def test_json_output_multiple_files(self):
        """Test JSON output with multiple files."""
        with tempfile.TemporaryDirectory() as tmpdir:
            file1 = Path(tmpdir, "file1.txt")
            file2 = Path(tmpdir, "file2.txt")
            file1.write_text("content1")
            file2.write_text("content2")

            result = self.run_rm("--json", str(file1), str(file2))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data), 2)
            self.assertEqual(data[0]["status"], "ok")
            self.assertEqual(data[1]["status"], "ok")

    def test_json_output_error(self):
        """Test JSON output on error."""
        result = self.run_rm("--json", "/nonexistent/file.txt")
        self.assertNotEqual(result.returncode, 0)

        data = json.loads(result.stdout)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["status"], "error")
        self.assertIn("message", data[0])

    def test_json_output_partial_failure(self):
        """Test JSON output with partial failure."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "test.txt")
            test_file.write_text("content")

            result = self.run_rm(
                "--json",
                str(test_file),
                "/nonexistent/file.txt"
            )
            self.assertNotEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data), 2)
            # First should succeed
            self.assertEqual(data[0]["status"], "ok")
            # Second should fail
            self.assertEqual(data[1]["status"], "error")

    def test_remove_empty_file(self):
        """Test removing an empty file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "empty.txt")
            test_file.touch()

            result = self.run_rm(str(test_file))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(test_file.exists())

    def test_remove_file_with_spaces_in_name(self):
        """Test removing file with spaces in name."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "file with spaces.txt")
            test_file.write_text("content")

            result = self.run_rm(str(test_file))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(test_file.exists())

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_rm("-z", "/tmp/somefile")
        self.assertNotEqual(result.returncode, 0)

    def test_combined_flags(self):
        """Test combining -r and -f flags."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_dir = Path(tmpdir, "testdir")
            test_dir.mkdir()
            (test_dir / "file.txt").write_text("content")

            result = self.run_rm("-rf", str(test_dir))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(test_dir.exists())

    def test_force_with_existing_file(self):
        """Test -f flag with existing file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "test.txt")
            test_file.write_text("content")

            result = self.run_rm("-f", str(test_file))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(test_file.exists())

    def test_recursive_preserves_parent(self):
        """Test that -r doesn't remove parent directories."""
        with tempfile.TemporaryDirectory() as tmpdir:
            parent = Path(tmpdir, "parent")
            parent.mkdir()
            child = parent / "child"
            child.mkdir()
            (child / "file.txt").write_text("content")

            result = self.run_rm("-r", str(child))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(child.exists())
            self.assertTrue(parent.exists())


if __name__ == "__main__":
    unittest.main()
