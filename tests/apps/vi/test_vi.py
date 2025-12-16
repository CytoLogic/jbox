#!/usr/bin/env python3
"""Unit tests for the vi command."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


class TestViCommand(unittest.TestCase):
    """Test cases for the vi command."""

    VI_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "vi"

    @classmethod
    def setUpClass(cls):
        """Verify the vi binary exists before running tests."""
        if not cls.VI_BIN.exists():
            raise unittest.SkipTest(f"vi binary not found at {cls.VI_BIN}")

    def run_vi(self, *args, stdin_data=None):
        """Run the vi command with given arguments and return result."""
        cmd = [str(self.VI_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            input=stdin_data,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_vi("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: vi", result.stdout)
        self.assertIn("--help", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_vi("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: vi", result.stdout)

    def test_help_shows_commands(self):
        """Test that help includes command information."""
        result = self.run_vi("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Normal mode commands:", result.stdout)
        self.assertIn("Command mode:", result.stdout)
        self.assertIn(":w", result.stdout)
        self.assertIn(":q", result.stdout)

    def test_help_shows_navigation(self):
        """Test that help includes navigation keys."""
        result = self.run_vi("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("h, LEFT", result.stdout)
        self.assertIn("j, DOWN", result.stdout)
        self.assertIn("k, UP", result.stdout)
        self.assertIn("l, RIGHT", result.stdout)

    def test_help_shows_editing(self):
        """Test that help includes editing commands."""
        result = self.run_vi("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Enter insert mode", result.stdout)
        self.assertIn("Delete", result.stdout)
        self.assertIn("Yank", result.stdout)
        self.assertIn("Paste", result.stdout)

    def test_requires_terminal(self):
        """Test that vi requires a terminal for normal operation."""
        # When stdin/stdout are not a terminal, vi should fail
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("test content\n")
            temp_path = f.name

        try:
            result = self.run_vi(temp_path)
            # Should fail because stdout is piped (not a terminal)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("requires a terminal", result.stderr)
        finally:
            os.unlink(temp_path)

    def test_invalid_option(self):
        """Test error handling for invalid option."""
        result = self.run_vi("-z")
        self.assertNotEqual(result.returncode, 0)

    def test_help_with_file_arg(self):
        """Test that help still works even with file argument."""
        result = self.run_vi("-h", "/some/file")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: vi", result.stdout)


class TestViHelpContent(unittest.TestCase):
    """Test the content of vi help output in detail."""

    VI_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "vi"

    @classmethod
    def setUpClass(cls):
        """Verify the vi binary exists before running tests."""
        if not cls.VI_BIN.exists():
            raise unittest.SkipTest(f"vi binary not found at {cls.VI_BIN}")

    def get_help_output(self):
        """Get the help output from vi."""
        cmd = [str(self.VI_BIN), "-h"]
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result.stdout

    def test_help_mentions_insert_mode(self):
        """Test that help mentions insert mode entry."""
        help_text = self.get_help_output()
        # Should mention 'i' for insert mode
        self.assertIn("i", help_text)
        self.assertIn("insert mode", help_text.lower())

    def test_help_mentions_command_mode(self):
        """Test that help mentions command mode."""
        help_text = self.get_help_output()
        self.assertIn(":", help_text)
        self.assertIn("command mode", help_text.lower())

    def test_help_mentions_save_commands(self):
        """Test that help mentions save commands."""
        help_text = self.get_help_output()
        self.assertIn(":w", help_text)
        self.assertIn(":wq", help_text)

    def test_help_mentions_quit_commands(self):
        """Test that help mentions quit commands."""
        help_text = self.get_help_output()
        self.assertIn(":q", help_text)
        self.assertIn(":q!", help_text)

    def test_help_mentions_line_navigation(self):
        """Test that help mentions line navigation."""
        help_text = self.get_help_output()
        self.assertIn("gg", help_text)  # Go to first line
        self.assertIn("G", help_text)   # Go to last line

    def test_help_mentions_word_navigation(self):
        """Test that help mentions word navigation."""
        help_text = self.get_help_output()
        self.assertIn("w", help_text)  # Move forward by word
        self.assertIn("b", help_text)  # Move backward by word

    def test_help_mentions_line_operations(self):
        """Test that help mentions line operations."""
        help_text = self.get_help_output()
        self.assertIn("dd", help_text)  # Delete line
        self.assertIn("yy", help_text)  # Yank line
        self.assertIn("o", help_text)   # Open line below
        self.assertIn("O", help_text)   # Open line above

    def test_help_mentions_search(self):
        """Test that help mentions search."""
        help_text = self.get_help_output()
        self.assertIn("/", help_text)  # Search forward


if __name__ == "__main__":
    unittest.main()
