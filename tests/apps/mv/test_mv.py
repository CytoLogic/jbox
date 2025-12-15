#!/usr/bin/env python3
"""Unit tests for the mv command."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestMvCommand(unittest.TestCase):
    """Test cases for the mv command."""

    MV_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "mv"

    @classmethod
    def setUpClass(cls):
        """Verify the mv binary exists before running tests."""
        if not cls.MV_BIN.exists():
            raise unittest.SkipTest(f"mv binary not found at {cls.MV_BIN}")

    def run_mv(self, *args):
        """Run the mv command with given arguments and return result."""
        cmd = [str(self.MV_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_mv("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: mv", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)
        self.assertIn("-f", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_mv("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: mv", result.stdout)

    def test_missing_arguments(self):
        """Test error when no arguments are provided."""
        result = self.run_mv()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr.lower())

    def test_missing_dest(self):
        """Test error when destination is missing."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            temp_path = f.name

        try:
            result = self.run_mv(temp_path)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("missing", result.stderr.lower())
        finally:
            if os.path.exists(temp_path):
                os.unlink(temp_path)

    def test_rename_file(self):
        """Test renaming a file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest = Path(tmpdir, "dest.txt")
            src.write_text("hello world")

            result = self.run_mv(str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(src.exists())
            self.assertTrue(dest.exists())
            self.assertEqual(dest.read_text(), "hello world")

    def test_move_file_to_directory(self):
        """Test moving a file into a directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest_dir = Path(tmpdir, "destdir")
            dest_dir.mkdir()
            src.write_text("content")

            result = self.run_mv(str(src), str(dest_dir))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(src.exists())
            moved_file = dest_dir / "source.txt"
            self.assertTrue(moved_file.exists())
            self.assertEqual(moved_file.read_text(), "content")

    def test_move_directory(self):
        """Test moving a directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src_dir = Path(tmpdir, "srcdir")
            src_dir.mkdir()
            (src_dir / "file.txt").write_text("content")
            dest_dir = Path(tmpdir, "destdir")

            result = self.run_mv(str(src_dir), str(dest_dir))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(src_dir.exists())
            self.assertTrue(dest_dir.exists())
            self.assertEqual((dest_dir / "file.txt").read_text(), "content")

    def test_move_nonexistent_file(self):
        """Test error when source file doesn't exist."""
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self.run_mv("/nonexistent/file.txt", tmpdir)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("no such file", result.stderr.lower())

    def test_overwrite_without_force(self):
        """Test error when overwriting without -f flag."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest = Path(tmpdir, "dest.txt")
            src.write_text("new content")
            dest.write_text("old content")

            result = self.run_mv(str(src), str(dest))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("already exists", result.stderr)
            # Both files should still exist
            self.assertTrue(src.exists())
            self.assertEqual(dest.read_text(), "old content")

    def test_overwrite_with_force(self):
        """Test overwriting with -f flag."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest = Path(tmpdir, "dest.txt")
            src.write_text("new content")
            dest.write_text("old content")

            result = self.run_mv("-f", str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(src.exists())
            self.assertEqual(dest.read_text(), "new content")

    def test_json_output_success(self):
        """Test JSON output on success."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest = Path(tmpdir, "dest.txt")
            src.write_text("content")

            result = self.run_mv("--json", str(src), str(dest))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["status"], "ok")
            self.assertEqual(data["source"], str(src))
            self.assertEqual(data["dest"], str(dest))

    def test_json_output_error(self):
        """Test JSON output on error."""
        with tempfile.TemporaryDirectory() as tmpdir:
            result = self.run_mv(
                "--json",
                "/nonexistent/file.txt",
                tmpdir
            )
            self.assertNotEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["status"], "error")
            self.assertIn("message", data)

    def test_json_output_with_directory(self):
        """Test JSON output when moving to directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest_dir = Path(tmpdir, "destdir")
            dest_dir.mkdir()
            src.write_text("content")

            result = self.run_mv("--json", str(src), str(dest_dir))
            self.assertEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["status"], "ok")
            self.assertIn("source.txt", data["dest"])

    def test_move_empty_file(self):
        """Test moving an empty file."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "empty.txt")
            dest = Path(tmpdir, "dest.txt")
            src.touch()

            result = self.run_mv(str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(src.exists())
            self.assertTrue(dest.exists())
            self.assertEqual(dest.read_text(), "")

    def test_move_file_with_spaces_in_name(self):
        """Test moving file with spaces in name."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "file with spaces.txt")
            dest = Path(tmpdir, "dest with spaces.txt")
            src.write_text("content")

            result = self.run_mv(str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(src.exists())
            self.assertEqual(dest.read_text(), "content")

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_mv("-z", "/tmp/src", "/tmp/dest")
        self.assertNotEqual(result.returncode, 0)

    def test_move_empty_directory(self):
        """Test moving an empty directory."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src_dir = Path(tmpdir, "emptydir")
            src_dir.mkdir()
            dest_dir = Path(tmpdir, "destdir")

            result = self.run_mv(str(src_dir), str(dest_dir))
            self.assertEqual(result.returncode, 0)
            self.assertFalse(src_dir.exists())
            self.assertTrue(dest_dir.exists())
            self.assertTrue(dest_dir.is_dir())

    def test_json_overwrite_error(self):
        """Test JSON output when overwrite fails."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "source.txt")
            dest = Path(tmpdir, "dest.txt")
            src.write_text("new")
            dest.write_text("old")

            result = self.run_mv("--json", str(src), str(dest))
            self.assertNotEqual(result.returncode, 0)

            data = json.loads(result.stdout)
            self.assertEqual(data["status"], "error")
            self.assertIn("overwrite", data["message"].lower())

    def test_move_preserves_binary_content(self):
        """Test that binary content is preserved after move."""
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir, "binary.bin")
            dest = Path(tmpdir, "dest.bin")
            binary_content = bytes([0, 1, 2, 3, 255, 254, 253])
            src.write_bytes(binary_content)

            result = self.run_mv(str(src), str(dest))
            self.assertEqual(result.returncode, 0)
            self.assertEqual(dest.read_bytes(), binary_content)


if __name__ == "__main__":
    unittest.main()
