#!/usr/bin/env python3
"""Tests for the sleep command."""

import subprocess
import time
import unittest
import os

BIN_PATH = os.path.join(os.path.dirname(__file__), '../../../bin/sleep')


class TestSleepHelp(unittest.TestCase):
    """Test help flags."""

    def test_help_short(self):
        """Test -h flag shows help."""
        result = subprocess.run([BIN_PATH, '-h'],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('Usage:', result.stdout)
        self.assertIn('SECONDS', result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = subprocess.run([BIN_PATH, '--help'],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('Usage:', result.stdout)
        self.assertIn('SECONDS', result.stdout)


class TestSleepBasic(unittest.TestCase):
    """Test basic sleep functionality."""

    def test_sleep_integer(self):
        """Test sleeping for an integer number of seconds."""
        start = time.time()
        result = subprocess.run([BIN_PATH, '1'],
                                capture_output=True, text=True)
        elapsed = time.time() - start
        self.assertEqual(result.returncode, 0)
        self.assertGreaterEqual(elapsed, 0.9)
        self.assertLess(elapsed, 2.0)

    def test_sleep_fractional(self):
        """Test sleeping for a fractional number of seconds."""
        start = time.time()
        result = subprocess.run([BIN_PATH, '0.5'],
                                capture_output=True, text=True)
        elapsed = time.time() - start
        self.assertEqual(result.returncode, 0)
        self.assertGreaterEqual(elapsed, 0.4)
        self.assertLess(elapsed, 1.0)

    def test_sleep_zero(self):
        """Test sleeping for zero seconds."""
        start = time.time()
        result = subprocess.run([BIN_PATH, '0'],
                                capture_output=True, text=True)
        elapsed = time.time() - start
        self.assertEqual(result.returncode, 0)
        self.assertLess(elapsed, 0.5)


class TestSleepErrors(unittest.TestCase):
    """Test error handling."""

    def test_no_arguments(self):
        """Test error when no arguments provided."""
        result = subprocess.run([BIN_PATH],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('sleep', result.stderr.lower())

    def test_negative_seconds(self):
        """Test error with negative seconds."""
        result = subprocess.run([BIN_PATH, '-5'],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)

    def test_invalid_argument(self):
        """Test error with non-numeric argument."""
        result = subprocess.run([BIN_PATH, 'abc'],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
