#!/usr/bin/env python3
"""Tests for the date command."""

import subprocess
import unittest
import os
import re
from datetime import datetime

BIN_PATH = os.path.join(os.path.dirname(__file__), '../../../bin/date')


class TestDateHelp(unittest.TestCase):
    """Test help flags."""

    def test_help_short(self):
        """Test -h flag shows help."""
        result = subprocess.run([BIN_PATH, '-h'],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('Usage:', result.stdout)
        self.assertIn('date', result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = subprocess.run([BIN_PATH, '--help'],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('Usage:', result.stdout)
        self.assertIn('date', result.stdout)


class TestDateBasic(unittest.TestCase):
    """Test basic date functionality."""

    def test_date_output_format(self):
        """Test that date outputs in a reasonable format."""
        result = subprocess.run([BIN_PATH],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        output = result.stdout.strip()
        # Check for day-of-week abbreviation at start
        self.assertRegex(output, r'^(Mon|Tue|Wed|Thu|Fri|Sat|Sun)')
        # Check for month abbreviation
        self.assertRegex(output, r'(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)')
        # Check for time pattern
        self.assertRegex(output, r'\d{2}:\d{2}:\d{2}')
        # Check for year at end
        self.assertRegex(output, r'\d{4}$')

    def test_date_current_year(self):
        """Test that date shows the current year."""
        result = subprocess.run([BIN_PATH],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        current_year = datetime.now().year
        self.assertIn(str(current_year), result.stdout)

    def test_date_no_extra_output(self):
        """Test that date outputs a single line."""
        result = subprocess.run([BIN_PATH],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        lines = result.stdout.strip().split('\n')
        self.assertEqual(len(lines), 1)


class TestDateErrors(unittest.TestCase):
    """Test error handling."""

    def test_unknown_option(self):
        """Test error with unknown option."""
        result = subprocess.run([BIN_PATH, '--unknown'],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('date', result.stderr.lower())


if __name__ == '__main__':
    unittest.main()
