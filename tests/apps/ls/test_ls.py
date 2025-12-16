#!/usr/bin/env python3
"""Unit tests for the ls command."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestLsCommand(unittest.TestCase):
    """Test cases for the ls command."""

    LS_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "ls"

    @classmethod
    def setUpClass(cls):
        """Verify the ls binary exists before running tests."""
        if not cls.LS_BIN.exists():
            raise unittest.SkipTest(f"ls binary not found at {cls.LS_BIN}")

    def run_ls(self, *args, cwd=None):
        """Run the ls command with given arguments and return result."""
        cmd = [str(self.LS_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            cwd=cwd,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_ls("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: ls", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_ls("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: ls", result.stdout)

    def test_current_directory(self):
        """Test listing current directory (no arguments)."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create test files
            Path(tmpdir, "file1.txt").touch()
            Path(tmpdir, "file2.txt").touch()
            Path(tmpdir, "subdir").mkdir()

            result = self.run_ls(cwd=tmpdir)
            self.assertEqual(result.returncode, 0)
            self.assertIn("file1.txt", result.stdout)
            self.assertIn("file2.txt", result.stdout)
            self.assertIn("subdir", result.stdout)

    def test_specific_directory(self):
        """Test listing a specific directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            Path(tmpdir, "testfile.txt").touch()

            result = self.run_ls(tmpdir)
            self.assertEqual(result.returncode, 0)
            self.assertIn("testfile.txt", result.stdout)

    def test_hidden_files_excluded_by_default(self):
        """Test that hidden files are excluded by default."""
        with tempfile.TemporaryDirectory() as tmpdir:
            Path(tmpdir, ".hidden").touch()
            Path(tmpdir, "visible").touch()

            result = self.run_ls(tmpdir)
            self.assertEqual(result.returncode, 0)
            self.assertNotIn(".hidden", result.stdout)
            self.assertIn("visible", result.stdout)

    def test_hidden_files_with_a_flag(self):
        """Test -a flag includes hidden files."""
        with tempfile.TemporaryDirectory() as tmpdir:
            Path(tmpdir, ".hidden").touch()
            Path(tmpdir, "visible").touch()

            result = self.run_ls("-a", tmpdir)
            self.assertEqual(result.returncode, 0)
            self.assertIn(".hidden", result.stdout)
            self.assertIn("visible", result.stdout)
            # Should also show . and ..
            self.assertIn(".", result.stdout)

    def test_long_format(self):
        """Test -l flag shows long format."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "testfile.txt")
            test_file.write_text("hello world")

            result = self.run_ls("-l", tmpdir)
            self.assertEqual(result.returncode, 0)
            # Long format should include permissions, size, date
            self.assertIn("testfile.txt", result.stdout)
            # Check for permission pattern (e.g., -rw-r--r--)
            lines = result.stdout.strip().split("\n")
            self.assertTrue(any(line.startswith("-") for line in lines))

    def test_json_output(self):
        """Test --json flag outputs valid JSON."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "testfile.txt")
            test_file.write_text("hello")

            result = self.run_ls("--json", tmpdir)
            self.assertEqual(result.returncode, 0)

            # Parse JSON output
            data = json.loads(result.stdout)
            self.assertIsInstance(data, list)
            self.assertEqual(len(data), 1)
            self.assertEqual(data[0]["name"], "testfile.txt")
            self.assertIn("type", data[0])
            self.assertIn("size", data[0])
            self.assertIn("mtime", data[0])

    def test_json_with_long_format(self):
        """Test --json -l includes additional fields."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "testfile.txt")
            test_file.write_text("hello")

            result = self.run_ls("--json", "-l", tmpdir)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(len(data), 1)
            # Long format JSON should include mode, nlink, owner, group
            self.assertIn("mode", data[0])
            self.assertIn("nlink", data[0])
            self.assertIn("owner", data[0])
            self.assertIn("group", data[0])

    def test_json_file_types(self):
        """Test JSON output correctly identifies file types."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Regular file
            Path(tmpdir, "regular.txt").touch()
            # Directory
            Path(tmpdir, "subdir").mkdir()

            result = self.run_ls("--json", "-a", tmpdir)
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            types = {item["name"]: item["type"] for item in data}

            self.assertEqual(types.get("regular.txt"), "file")
            self.assertEqual(types.get("subdir"), "directory")

    def test_nonexistent_path(self):
        """Test error handling for nonexistent path."""
        result = self.run_ls("/nonexistent/path/that/does/not/exist")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("cannot access", result.stderr)

    def test_multiple_paths(self):
        """Test listing multiple paths."""
        with tempfile.TemporaryDirectory() as tmpdir:
            dir1 = Path(tmpdir, "dir1")
            dir2 = Path(tmpdir, "dir2")
            dir1.mkdir()
            dir2.mkdir()
            Path(dir1, "file1.txt").touch()
            Path(dir2, "file2.txt").touch()

            result = self.run_ls(str(dir1), str(dir2))
            self.assertEqual(result.returncode, 0)
            self.assertIn("file1.txt", result.stdout)
            self.assertIn("file2.txt", result.stdout)

    def test_file_argument(self):
        """Test listing a specific file (not directory)."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "testfile.txt")
            test_file.touch()

            result = self.run_ls(str(test_file))
            self.assertEqual(result.returncode, 0)
            self.assertIn("testfile.txt", result.stdout)

    def test_file_argument_long_format(self):
        """Test listing a specific file with -l flag."""
        with tempfile.TemporaryDirectory() as tmpdir:
            test_file = Path(tmpdir, "testfile.txt")
            test_file.write_text("test content")

            result = self.run_ls("-l", str(test_file))
            self.assertEqual(result.returncode, 0)
            self.assertIn("testfile.txt", result.stdout)
            # Should show file size
            self.assertIn("12", result.stdout)  # "test content" is 12 bytes

    def test_empty_directory(self):
        """Test listing an empty directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self.run_ls(tmpdir)
            self.assertEqual(result.returncode, 0)
            # Should be empty output (no files)
            self.assertEqual(result.stdout.strip(), "")

    def test_empty_directory_json(self):
        """Test listing an empty directory with --json."""
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self.run_ls("--json", tmpdir)
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            self.assertEqual(data, [])

    def test_special_characters_in_filename(self):
        """Test handling of special characters in filenames."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create file with spaces
            test_file = Path(tmpdir, "file with spaces.txt")
            test_file.touch()

            result = self.run_ls(tmpdir)
            self.assertEqual(result.returncode, 0)
            self.assertIn("file with spaces.txt", result.stdout)

    def test_special_characters_json(self):
        """Test JSON escaping of special characters."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create file with quotes (if possible on filesystem)
            test_file = Path(tmpdir, 'file"with"quotes.txt')
            try:
                test_file.touch()
                result = self.run_ls("--json", tmpdir)
                self.assertEqual(result.returncode, 0)
                # Should be valid JSON even with special chars
                data = json.loads(result.stdout)
                self.assertEqual(len(data), 1)
            except OSError:
                self.skipTest("Filesystem doesn't support quotes in filenames")

    def test_combined_flags(self):
        """Test combining multiple flags."""
        with tempfile.TemporaryDirectory() as tmpdir:
            Path(tmpdir, ".hidden").touch()
            Path(tmpdir, "visible").touch()

            result = self.run_ls("-al", tmpdir)
            self.assertEqual(result.returncode, 0)
            self.assertIn(".hidden", result.stdout)
            self.assertIn("visible", result.stdout)
            # Should be in long format
            lines = [l for l in result.stdout.strip().split("\n") if l]
            self.assertTrue(all(l[0] in "-dlcbps" for l in lines))

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_ls("-z")
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
