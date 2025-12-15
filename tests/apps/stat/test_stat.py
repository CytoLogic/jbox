#!/usr/bin/env python3
"""Unit tests for the stat command."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestStatCommand(unittest.TestCase):
    """Test cases for the stat command."""

    STAT_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "stat"

    @classmethod
    def setUpClass(cls):
        """Verify the stat binary exists before running tests."""
        if not cls.STAT_BIN.exists():
            raise unittest.SkipTest(f"stat binary not found at {cls.STAT_BIN}")

    def run_stat(self, *args):
        """Run the stat command with given arguments and return result."""
        cmd = [str(self.STAT_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_stat("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: stat", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_stat("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: stat", result.stdout)

    def test_missing_file_argument(self):
        """Test error when no file argument is provided."""
        result = self.run_stat()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr.lower())

    def test_regular_file(self):
        """Test stat on a regular file."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"test content")
            temp_path = f.name

        try:
            result = self.run_stat(temp_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("File:", result.stdout)
            self.assertIn(temp_path, result.stdout)
            self.assertIn("Size:", result.stdout)
            self.assertIn("regular file", result.stdout)
        finally:
            os.unlink(temp_path)

    def test_directory(self):
        """Test stat on a directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self.run_stat(tmpdir)
            self.assertEqual(result.returncode, 0)
            self.assertIn("File:", result.stdout)
            self.assertIn("directory", result.stdout)

    def test_json_output(self):
        """Test --json flag outputs valid JSON."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"hello world")
            temp_path = f.name

        try:
            result = self.run_stat("--json", temp_path)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertIsInstance(data, dict)
            self.assertEqual(data["path"], temp_path)
            self.assertIn("type", data)
            self.assertIn("size", data)
            self.assertIn("mode", data)
            self.assertIn("mtime", data)
            self.assertIn("atime", data)
            self.assertIn("ctime", data)
        finally:
            os.unlink(temp_path)

    def test_json_has_required_fields(self):
        """Test JSON output contains all required fields."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"test")
            temp_path = f.name

        try:
            result = self.run_stat("--json", temp_path)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            required_fields = [
                "path", "type", "size", "mode", "uid", "gid",
                "owner", "group", "nlink", "inode", "dev",
                "atime", "mtime", "ctime"
            ]
            for field in required_fields:
                self.assertIn(field, data, f"Missing field: {field}")
        finally:
            os.unlink(temp_path)

    def test_json_file_types(self):
        """Test JSON output correctly identifies file types."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Test regular file
            test_file = Path(tmpdir, "regular.txt")
            test_file.touch()

            result = self.run_stat("--json", str(test_file))
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            self.assertEqual(data["type"], "regular file")

            # Test directory
            result = self.run_stat("--json", tmpdir)
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            self.assertEqual(data["type"], "directory")

    def test_nonexistent_file(self):
        """Test error handling for nonexistent file."""
        result = self.run_stat("/nonexistent/path/that/does/not/exist")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("cannot stat", result.stderr)

    def test_nonexistent_file_json(self):
        """Test JSON error output for nonexistent file."""
        result = self.run_stat("--json", "/nonexistent/path/file.txt")
        self.assertNotEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIn("error", data)
        self.assertIn("path", data)

    def test_file_size(self):
        """Test that file size is correctly reported."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            content = b"exact size test"
            f.write(content)
            temp_path = f.name

        try:
            result = self.run_stat("--json", temp_path)
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            self.assertEqual(data["size"], len(content))
        finally:
            os.unlink(temp_path)

    def test_timestamps_are_integers(self):
        """Test that timestamps are returned as integers (epoch seconds)."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"test")
            temp_path = f.name

        try:
            result = self.run_stat("--json", temp_path)
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            self.assertIsInstance(data["atime"], int)
            self.assertIsInstance(data["mtime"], int)
            self.assertIsInstance(data["ctime"], int)
        finally:
            os.unlink(temp_path)

    def test_mode_is_octal_string(self):
        """Test that mode is returned as an octal string."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"test")
            temp_path = f.name

        try:
            result = self.run_stat("--json", temp_path)
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            # Mode should be a 4-digit octal string
            self.assertIsInstance(data["mode"], str)
            self.assertEqual(len(data["mode"]), 4)
            # All characters should be valid octal digits
            self.assertTrue(all(c in "01234567" for c in data["mode"]))
        finally:
            os.unlink(temp_path)

    def test_symlink(self):
        """Test stat on a symbolic link."""
        with tempfile.TemporaryDirectory() as tmpdir:
            target = Path(tmpdir, "target.txt")
            target.touch()
            link = Path(tmpdir, "link.txt")
            link.symlink_to(target)

            result = self.run_stat("--json", str(link))
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            self.assertEqual(data["type"], "symbolic link")

    def test_human_readable_output_format(self):
        """Test human-readable output has expected structure."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"test")
            temp_path = f.name

        try:
            result = self.run_stat(temp_path)
            self.assertEqual(result.returncode, 0)

            # Check for expected labels in output
            expected_labels = ["File:", "Size:", "Device:", "Access:", "Modify:", "Change:"]
            for label in expected_labels:
                self.assertIn(label, result.stdout)
        finally:
            os.unlink(temp_path)

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_stat("-z", "/tmp")
        self.assertNotEqual(result.returncode, 0)

    def test_special_characters_in_path(self):
        """Test handling of special characters in file paths."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "file with spaces.txt")
            test_file.touch()

            result = self.run_stat(str(test_file))
            self.assertEqual(result.returncode, 0)
            self.assertIn("file with spaces.txt", result.stdout)

    def test_json_escapes_special_characters(self):
        """Test JSON output properly escapes special characters."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, 'file"with"quotes.txt')
            try:
                test_file.touch()
                result = self.run_stat("--json", str(test_file))
                self.assertEqual(result.returncode, 0)
                # Should be valid JSON even with special chars
                data = json.loads(result.stdout)
                self.assertIn("quotes", data["path"])
            except OSError:
                self.skipTest("Filesystem doesn't support quotes in filenames")


if __name__ == "__main__":
    unittest.main()
