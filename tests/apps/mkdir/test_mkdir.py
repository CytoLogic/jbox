#!/usr/bin/env python3
"""Unit tests for the mkdir command."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestMkdirCommand(unittest.TestCase):
    """Test cases for the mkdir command."""

    MKDIR_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "mkdir"

    @classmethod
    def setUpClass(cls):
        """Verify the mkdir binary exists before running tests."""
        if not cls.MKDIR_BIN.exists():
            raise unittest.SkipTest(f"mkdir binary not found at {cls.MKDIR_BIN}")

    def run_mkdir(self, *args):
        """Run the mkdir command with given arguments and return result."""
        cmd = [str(self.MKDIR_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_mkdir("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: mkdir", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)
        self.assertIn("-p", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_mkdir("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: mkdir", result.stdout)

    def test_missing_arguments(self):
        """Test error when no arguments are provided."""
        result = self.run_mkdir()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr.lower())

    def test_create_single_directory(self):
        """Test creating a single directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            new_dir = Path(tmpdir, "newdir")

            result = self.run_mkdir(str(new_dir))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(new_dir.exists())
            self.assertTrue(new_dir.is_dir())

    def test_create_multiple_directories(self):
        """Test creating multiple directories."""
        with tempfile.TemporaryDirectory() as tmpdir:
            dir1 = Path(tmpdir, "dir1")
            dir2 = Path(tmpdir, "dir2")

            result = self.run_mkdir(str(dir1), str(dir2))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(dir1.exists())
            self.assertTrue(dir2.exists())

    def test_create_existing_directory(self):
        """Test error when directory already exists."""
        with tempfile.TemporaryDirectory() as tmpdir:
            existing = Path(tmpdir, "existing")
            existing.mkdir()

            result = self.run_mkdir(str(existing))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("exist", result.stderr.lower())

    def test_create_nested_without_parents(self):
        """Test error when creating nested directory without -p."""
        with tempfile.TemporaryDirectory() as tmpdir:
            nested = Path(tmpdir, "a", "b", "c")

            result = self.run_mkdir(str(nested))
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(nested.exists())

    def test_create_nested_with_parents(self):
        """Test creating nested directories with -p."""
        with tempfile.TemporaryDirectory() as tmpdir:
            nested = Path(tmpdir, "a", "b", "c")

            result = self.run_mkdir("-p", str(nested))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(nested.exists())
            self.assertTrue(nested.is_dir())

    def test_parents_existing_directory(self):
        """Test -p flag with existing directory doesn't fail."""
        with tempfile.TemporaryDirectory() as tmpdir:
            existing = Path(tmpdir, "existing")
            existing.mkdir()

            result = self.run_mkdir("-p", str(existing))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(existing.exists())

    def test_json_output_success(self):
        """Test JSON output on success."""
        with tempfile.TemporaryDirectory() as tmpdir:
            new_dir = Path(tmpdir, "newdir")

            result = self.run_mkdir("--json", str(new_dir))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertIsInstance(data, list)
            self.assertEqual(len(data), 1)
            self.assertEqual(data[0]["path"], str(new_dir))
            self.assertEqual(data[0]["status"], "ok")

    def test_json_output_multiple(self):
        """Test JSON output with multiple directories."""
        with tempfile.TemporaryDirectory() as tmpdir:
            dir1 = Path(tmpdir, "dir1")
            dir2 = Path(tmpdir, "dir2")

            result = self.run_mkdir("--json", str(dir1), str(dir2))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data), 2)
            self.assertEqual(data[0]["status"], "ok")
            self.assertEqual(data[1]["status"], "ok")

    def test_json_output_error(self):
        """Test JSON output on error."""
        with tempfile.TemporaryDirectory() as tmpdir:
            nested = Path(tmpdir, "a", "b", "c")

            result = self.run_mkdir("--json", str(nested))
            self.assertNotEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data), 1)
            self.assertEqual(data[0]["status"], "error")
            self.assertIn("message", data[0])

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_mkdir("-z", "/tmp/somedir")
        self.assertNotEqual(result.returncode, 0)

    def test_directory_with_spaces_in_name(self):
        """Test creating directory with spaces in name."""
        with tempfile.TemporaryDirectory() as tmpdir:
            new_dir = Path(tmpdir, "dir with spaces")

            result = self.run_mkdir(str(new_dir))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(new_dir.exists())


if __name__ == "__main__":
    unittest.main()
