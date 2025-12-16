#!/usr/bin/env python3
"""Unit tests for the cp command."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestCpCommand(unittest.TestCase):
    """Test cases for the cp command."""

    CP_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "cp"

    @classmethod
    def setUpClass(cls):
        """Verify the cp binary exists before running tests."""
        if not cls.CP_BIN.exists():
            raise unittest.SkipTest(f"cp binary not found at {cls.CP_BIN}")

    def run_cp(self, *args):
        """Run the cp command with given arguments and return result."""
        cmd = [str(self.CP_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_cp("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: cp", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)
        self.assertIn("-r", result.stdout)
        self.assertIn("-f", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_cp("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: cp", result.stdout)

    def test_missing_arguments(self):
        """Test error when no arguments are provided."""
        result = self.run_cp()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr.lower())

    def test_missing_dest(self):
        """Test error when destination is missing."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            temp_path = f.name

        try:
            result = self.run_cp(temp_path)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("missing", result.stderr.lower())
        finally:
            os.unlink(temp_path)

    def test_copy_single_file(self):
        """Test copying a single file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest = Path(tmpdir, "dest.txt")
            src.write_text("hello world")

            result = self.run_cp(str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(dest.exists())
            self.assertEqual(dest.read_text(), "hello world")

    def test_copy_file_to_directory(self):
        """Test copying a file into a directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest_dir = Path(tmpdir, "destdir")
            dest_dir.mkdir()
            src.write_text("content")

            result = self.run_cp(str(src), str(dest_dir))
            self.assertEqual(result.returncode, 0)
            copied_file = dest_dir / "source.txt"
            self.assertTrue(copied_file.exists())
            self.assertEqual(copied_file.read_text(), "content")

    def test_copy_preserves_content(self):
        """Test that file content is preserved exactly."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest = Path(tmpdir, "dest.txt")
            content = "line1\nline2\nline3\n"
            src.write_text(content)

            result = self.run_cp(str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertEqual(dest.read_text(), content)

    def test_copy_binary_file(self):
        """Test copying a binary file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.bin")
            dest = Path(tmpdir, "dest.bin")
            binary_content = bytes([0, 1, 2, 3, 255, 254, 253])
            src.write_bytes(binary_content)

            result = self.run_cp(str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertEqual(dest.read_bytes(), binary_content)

    def test_copy_nonexistent_file(self):
        """Test error when source file doesn't exist."""
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self.run_cp("/nonexistent/file.txt", tmpdir)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("cannot copy", result.stderr.lower())

    def test_copy_directory_without_r(self):
        """Test error when copying directory without -r flag."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src_dir = Path(tmpdir, "srcdir")
            src_dir.mkdir()
            dest = Path(tmpdir, "destdir")

            result = self.run_cp(str(src_dir), str(dest))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("-r not specified", result.stderr)

    def test_copy_directory_recursive(self):
        """Test recursive directory copy."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src_dir = Path(tmpdir, "srcdir")
            src_dir.mkdir()
            (src_dir / "file1.txt").write_text("content1")
            (src_dir / "file2.txt").write_text("content2")
            dest_dir = Path(tmpdir, "destdir")

            result = self.run_cp("-r", str(src_dir), str(dest_dir))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(dest_dir.exists())
            self.assertEqual((dest_dir / "file1.txt").read_text(), "content1")
            self.assertEqual((dest_dir / "file2.txt").read_text(), "content2")

    def test_copy_nested_directory(self):
        """Test copying nested directories."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src_dir = Path(tmpdir, "srcdir")
            src_dir.mkdir()
            subdir = src_dir / "subdir"
            subdir.mkdir()
            (subdir / "nested.txt").write_text("nested content")
            dest_dir = Path(tmpdir, "destdir")

            result = self.run_cp("-r", str(src_dir), str(dest_dir))
            self.assertEqual(result.returncode, 0)
            self.assertTrue((dest_dir / "subdir" / "nested.txt").exists())
            self.assertEqual(
                (dest_dir / "subdir" / "nested.txt").read_text(),
                "nested content"
            )

    def test_overwrite_without_force(self):
        """Test error when overwriting without -f flag."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest = Path(tmpdir, "dest.txt")
            src.write_text("new content")
            dest.write_text("old content")

            result = self.run_cp(str(src), str(dest))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("already exists", result.stderr)
            # Original content should be preserved
            self.assertEqual(dest.read_text(), "old content")

    def test_overwrite_with_force(self):
        """Test overwriting with -f flag."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest = Path(tmpdir, "dest.txt")
            src.write_text("new content")
            dest.write_text("old content")

            result = self.run_cp("-f", str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertEqual(dest.read_text(), "new content")

    def test_json_output_success(self):
        """Test JSON output on success."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest = Path(tmpdir, "dest.txt")
            src.write_text("content")

            result = self.run_cp("--json", str(src), str(dest))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["status"], "ok")
            self.assertEqual(data["source"], str(src))
            self.assertEqual(data["dest"], str(dest))

    def test_json_output_error(self):
        """Test JSON output on error."""
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self.run_cp(
                "--json",
                "/nonexistent/file.txt",
                tmpdir
            )
            self.assertNotEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["status"], "error")
            self.assertIn("message", data)

    def test_json_output_with_directory(self):
        """Test JSON output when copying to directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest_dir = Path(tmpdir, "destdir")
            dest_dir.mkdir()
            src.write_text("content")

            result = self.run_cp("--json", str(src), str(dest_dir))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["status"], "ok")
            self.assertIn("source.txt", data["dest"])

    def test_copy_empty_file(self):
        """Test copying an empty file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "empty.txt")
            dest = Path(tmpdir, "dest.txt")
            src.touch()

            result = self.run_cp(str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(dest.exists())
            self.assertEqual(dest.read_text(), "")

    def test_copy_large_file(self):
        """Test copying a larger file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "large.txt")
            dest = Path(tmpdir, "dest.txt")
            content = "x" * 100000
            src.write_text(content)

            result = self.run_cp(str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertEqual(dest.read_text(), content)

    def test_copy_empty_directory(self):
        """Test copying an empty directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src_dir = Path(tmpdir, "emptydir")
            src_dir.mkdir()
            dest_dir = Path(tmpdir, "destdir")

            result = self.run_cp("-r", str(src_dir), str(dest_dir))
            self.assertEqual(result.returncode, 0)
            self.assertTrue(dest_dir.exists())
            self.assertTrue(dest_dir.is_dir())

    def test_copy_file_with_spaces_in_name(self):
        """Test copying file with spaces in name."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "file with spaces.txt")
            dest = Path(tmpdir, "dest with spaces.txt")
            src.write_text("content")

            result = self.run_cp(str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertEqual(dest.read_text(), "content")

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_cp("-z", "/tmp/src", "/tmp/dest")
        self.assertNotEqual(result.returncode, 0)

    def test_preserves_permissions(self):
        """Test that file permissions are preserved."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest = Path(tmpdir, "dest.txt")
            src.write_text("content")
            os.chmod(src, 0o755)

            result = self.run_cp(str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            # Check permissions match
            src_mode = os.stat(src).st_mode & 0o777
            dest_mode = os.stat(dest).st_mode & 0o777
            self.assertEqual(src_mode, dest_mode)


if __name__ == "__main__":
    unittest.main()
