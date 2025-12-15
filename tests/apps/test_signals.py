#!/usr/bin/env python3
"""Unit tests for signal handling in external apps.

Note on exit codes: Python subprocess returns -signal_number when a process
is terminated by a signal (e.g., -2 for SIGINT, -15 for SIGTERM). Shell
convention is 128 + signal_number (e.g., 130 for SIGINT, 143 for SIGTERM).
Our apps may return either, so tests accept both conventions.
"""

import os
import signal
import tempfile
import time
import unittest
from pathlib import Path

from tests.helpers import SignalTestHelper


def signal_exit_codes(sig):
    """Return acceptable exit codes for signal termination.

    Args:
        sig: Signal number (e.g., signal.SIGINT)

    Returns:
        List of acceptable exit codes (Python's -sig and shell's 128+sig)
    """
    return [-sig, 128 + sig]


class TestCatSignals(unittest.TestCase):
    """Test cases for cat signal handling."""

    @classmethod
    def setUpClass(cls):
        """Check if cat binary exists."""
        cls.cat_bin = SignalTestHelper.BIN_DIR / "cat"
        if not cls.cat_bin.exists():
            raise unittest.SkipTest(f"cat binary not found at {cls.cat_bin}")

    def test_cat_sigint_large_file(self):
        """Test cat exits cleanly on SIGINT with large file."""
        # Create a large temp file
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
            # Write ~2MB of data (should take time to output)
            for i in range(100000):
                f.write(f"Line {i}: This is some test data to make lines longer\n")
            temp_file = f.name

        try:
            result = SignalTestHelper.run_app_with_signal(
                "cat", [temp_file],
                signal.SIGINT,
                delay_ms=20,
                timeout=5.0
            )
            # Accept either signal exit code or 0 (finished before signal)
            acceptable = signal_exit_codes(signal.SIGINT) + [0]
            self.assertIn(result.returncode, acceptable)
        finally:
            os.unlink(temp_file)

    def test_cat_sigint_quick_exit(self):
        """Test cat responds quickly to SIGINT."""
        # Create a very large file
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
            for i in range(200000):
                f.write(f"Line {i}: More data\n")
            temp_file = f.name

        try:
            start = time.time()
            result = SignalTestHelper.run_app_with_signal(
                "cat", [temp_file],
                signal.SIGINT,
                delay_ms=10,
                timeout=5.0
            )
            elapsed = time.time() - start
            # Should exit quickly, not process entire file
            self.assertLess(elapsed, 3.0)
        finally:
            os.unlink(temp_file)


class TestHeadTailSignals(unittest.TestCase):
    """Test cases for head and tail signal handling."""

    @classmethod
    def setUpClass(cls):
        """Check if head and tail binaries exist."""
        cls.head_bin = SignalTestHelper.BIN_DIR / "head"
        cls.tail_bin = SignalTestHelper.BIN_DIR / "tail"
        if not cls.head_bin.exists():
            raise unittest.SkipTest(f"head binary not found at {cls.head_bin}")
        if not cls.tail_bin.exists():
            raise unittest.SkipTest(f"tail binary not found at {cls.tail_bin}")

    def test_head_sigint(self):
        """Test head exits cleanly on SIGINT."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
            for i in range(200000):
                f.write(f"Line {i}: Some extra content to make output larger\n")
            temp_file = f.name

        try:
            result = SignalTestHelper.run_app_with_signal(
                "head", ["-n", "100000", temp_file],
                signal.SIGINT,
                delay_ms=10,
                timeout=5.0
            )
            # Accept signal code or 0 (finished before signal)
            acceptable = signal_exit_codes(signal.SIGINT) + [0]
            self.assertIn(result.returncode, acceptable)
        finally:
            os.unlink(temp_file)

    def test_tail_sigint(self):
        """Test tail exits cleanly on SIGINT."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
            for i in range(200000):
                f.write(f"Line {i}: Some extra content\n")
            temp_file = f.name

        try:
            result = SignalTestHelper.run_app_with_signal(
                "tail", ["-n", "100000", temp_file],
                signal.SIGINT,
                delay_ms=10,
                timeout=5.0
            )
            # Accept signal code or 0 (finished before signal)
            acceptable = signal_exit_codes(signal.SIGINT) + [0]
            self.assertIn(result.returncode, acceptable)
        finally:
            os.unlink(temp_file)


class TestCpSignals(unittest.TestCase):
    """Test cases for cp signal handling."""

    @classmethod
    def setUpClass(cls):
        """Check if cp binary exists."""
        cls.cp_bin = SignalTestHelper.BIN_DIR / "cp"
        if not cls.cp_bin.exists():
            raise unittest.SkipTest(f"cp binary not found at {cls.cp_bin}")

    def test_cp_sigint_during_copy(self):
        """Test cp exits cleanly on SIGINT during large file copy."""
        # Create a large temp file
        with tempfile.NamedTemporaryFile(mode='wb', delete=False) as f:
            # Write ~10MB of binary data
            f.write(os.urandom(10 * 1024 * 1024))
            source_file = f.name

        dest_file = source_file + ".copy"

        try:
            result = SignalTestHelper.run_app_with_signal(
                "cp", [source_file, dest_file],
                signal.SIGINT,
                delay_ms=5,
                timeout=5.0
            )
            # Accept signal code or 0 (finished before signal)
            acceptable = signal_exit_codes(signal.SIGINT) + [0]
            self.assertIn(result.returncode, acceptable)
        finally:
            try:
                os.unlink(source_file)
            except FileNotFoundError:
                pass
            try:
                os.unlink(dest_file)
            except FileNotFoundError:
                pass

    def test_cp_sigint_recursive(self):
        """Test cp -r exits cleanly on SIGINT during recursive copy."""
        # Create a temp directory with files
        temp_dir = tempfile.mkdtemp()
        dest_dir = temp_dir + ".copy"

        try:
            # Create many files with more content
            for i in range(200):
                with open(os.path.join(temp_dir, f"file{i}.txt"), 'w') as f:
                    f.write("x" * 50000)

            result = SignalTestHelper.run_app_with_signal(
                "cp", ["-r", temp_dir, dest_dir],
                signal.SIGINT,
                delay_ms=5,
                timeout=5.0
            )
            # Accept signal code or 0 (finished before signal)
            acceptable = signal_exit_codes(signal.SIGINT) + [0]
            self.assertIn(result.returncode, acceptable)
        finally:
            import shutil
            try:
                shutil.rmtree(temp_dir)
            except FileNotFoundError:
                pass
            try:
                shutil.rmtree(dest_dir)
            except FileNotFoundError:
                pass


class TestRgSignals(unittest.TestCase):
    """Test cases for rg (ripgrep) signal handling."""

    @classmethod
    def setUpClass(cls):
        """Check if rg binary exists."""
        cls.rg_bin = SignalTestHelper.BIN_DIR / "rg"
        if not cls.rg_bin.exists():
            raise unittest.SkipTest(f"rg binary not found at {cls.rg_bin}")

    def test_rg_sigint_large_file(self):
        """Test rg exits cleanly on SIGINT when searching large file."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
            for i in range(200000):
                f.write(f"Line {i}: Some text that might match pattern\n")
            temp_file = f.name

        try:
            result = SignalTestHelper.run_app_with_signal(
                "rg", ["text", temp_file],
                signal.SIGINT,
                delay_ms=10,
                timeout=5.0
            )
            # Accept signal code or other valid exit codes (0=found, 1=not found)
            acceptable = signal_exit_codes(signal.SIGINT) + [0, 1]
            self.assertIn(result.returncode, acceptable)
        finally:
            os.unlink(temp_file)

    def test_rg_sigint_many_files(self):
        """Test rg exits cleanly on SIGINT when searching many files."""
        temp_dir = tempfile.mkdtemp()

        try:
            # Create many files with searchable content
            for i in range(100):
                with open(os.path.join(temp_dir, f"file{i}.txt"), 'w') as f:
                    for j in range(2000):
                        f.write(f"Line {j}: Some searchable pattern\n")

            result = SignalTestHelper.run_app_with_signal(
                "rg", ["searchable", temp_dir],
                signal.SIGINT,
                delay_ms=10,
                timeout=5.0
            )
            # Accept signal code or other valid exit codes (0=found, 1=not found)
            acceptable = signal_exit_codes(signal.SIGINT) + [0, 1]
            self.assertIn(result.returncode, acceptable)
        finally:
            import shutil
            shutil.rmtree(temp_dir)


class TestSleepSignals(unittest.TestCase):
    """Test cases for sleep signal handling."""

    @classmethod
    def setUpClass(cls):
        """Check if sleep binary exists."""
        cls.sleep_bin = SignalTestHelper.BIN_DIR / "sleep"
        if not cls.sleep_bin.exists():
            raise unittest.SkipTest(f"sleep binary not found at {cls.sleep_bin}")

    def test_sleep_sigint(self):
        """Test sleep exits on SIGINT."""
        result = SignalTestHelper.run_app_with_signal(
            "sleep", ["10"],
            signal.SIGINT,
            delay_ms=100,
            timeout=3.0
        )
        # Accept either Python's -2 or shell convention's 130
        self.assertIn(result.returncode, signal_exit_codes(signal.SIGINT))

    def test_sleep_exits_quickly(self):
        """Test sleep responds quickly to SIGINT."""
        start = time.time()

        result = SignalTestHelper.run_app_with_signal(
            "sleep", ["60"],
            signal.SIGINT,
            delay_ms=100,
            timeout=5.0
        )

        elapsed = time.time() - start
        # Should exit within a few seconds, not 60
        self.assertLess(elapsed, 3.0)
        # Accept either Python's -2 or shell convention's 130
        self.assertIn(result.returncode, signal_exit_codes(signal.SIGINT))


class TestSignalExitCodes(unittest.TestCase):
    """Test that apps return correct exit codes on signals."""

    @classmethod
    def setUpClass(cls):
        """Check if required binaries exist."""
        cls.sleep_bin = SignalTestHelper.BIN_DIR / "sleep"
        if not cls.sleep_bin.exists():
            raise unittest.SkipTest(f"sleep binary not found at {cls.sleep_bin}")

    def test_sigterm_exit_code(self):
        """Test SIGTERM results in appropriate exit code."""
        result = SignalTestHelper.run_app_with_signal(
            "sleep", ["10"],
            signal.SIGTERM,
            delay_ms=100,
            timeout=3.0
        )
        # Accept either Python's -15 or shell convention's 143
        self.assertIn(result.returncode, signal_exit_codes(signal.SIGTERM))

    def test_sigint_exit_code(self):
        """Test SIGINT results in appropriate exit code."""
        result = SignalTestHelper.run_app_with_signal(
            "sleep", ["10"],
            signal.SIGINT,
            delay_ms=100,
            timeout=3.0
        )
        # Accept either Python's -2 or shell convention's 130
        self.assertIn(result.returncode, signal_exit_codes(signal.SIGINT))


if __name__ == "__main__":
    unittest.main()
