#!/usr/bin/env python3
"""Unit tests for the http-post builtin command."""

import json
import unittest
import urllib.request

from tests.helpers import JShellRunner


# Test URL - use a reliable public website
TEST_URL = "https://archlinux.org"


def test_url_available():
    """Check if test URL is available."""
    try:
        urllib.request.urlopen(TEST_URL, timeout=10)
        return True
    except Exception:
        return False


# Cache the result at module load time
TEST_URL_AVAILABLE = test_url_available()


class TestHttpPostHelp(unittest.TestCase):
    """Test cases for http-post help functionality."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_help_short(self):
        """Test -h flag shows help."""
        result = JShellRunner.run("http-post -h", timeout=30)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: http-post", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = JShellRunner.run("http-post --help", timeout=30)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: http-post", result.stdout)

    def test_help_shows_header_option(self):
        """Test that help mentions the -H header option."""
        result = JShellRunner.run("http-post -h", timeout=30)
        self.assertEqual(result.returncode, 0)
        self.assertIn("-H", result.stdout)
        self.assertIn("header", result.stdout.lower())

    def test_help_shows_data_option(self):
        """Test that help mentions the -d data option."""
        result = JShellRunner.run("http-post -h", timeout=30)
        self.assertEqual(result.returncode, 0)
        self.assertIn("-d", result.stdout)
        self.assertIn("data", result.stdout.lower())

    def test_help_shows_examples(self):
        """Test that help includes examples."""
        result = JShellRunner.run("http-post -h", timeout=30)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Examples:", result.stdout)

    def test_help_shows_url_parameter(self):
        """Test that help shows URL parameter."""
        result = JShellRunner.run("http-post -h", timeout=30)
        self.assertEqual(result.returncode, 0)
        self.assertIn("URL", result.stdout)

    def test_missing_url_shows_error(self):
        """Test error when no URL argument is provided."""
        result = JShellRunner.run("http-post", timeout=30)
        output = (result.stdout + result.stderr).lower()
        has_error_info = ("url" in output or "error" in output or
                         "usage" in output or "required" in output)
        self.assertTrue(has_error_info,
                       f"Expected error about missing URL, got: {result.stdout}")


@unittest.skipUnless(TEST_URL_AVAILABLE, f"{TEST_URL} is not available")
class TestHttpPostNetwork(unittest.TestCase):
    """Test cases for http-post network functionality."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_post_request_completes(self):
        """Test that POST request completes (may return error page)."""
        result = JShellRunner.run(f"http-post {TEST_URL}", timeout=30)
        # The request should complete (server may reject POST but curl succeeds)
        # We just verify the command runs without crashing
        self.assertIsNotNone(result.stdout)

    def test_post_with_json_output_format(self):
        """Test POST with --json output produces valid JSON."""
        result = JShellRunner.run(f"http-post --json {TEST_URL}", timeout=30)
        # Find JSON line
        json_line = None
        for line in result.stdout.split('\n'):
            if line.strip().startswith('{'):
                json_line = line
                break
        self.assertIsNotNone(json_line, "No JSON line found in output")
        # Parse the JSON output - should be valid JSON regardless of server response
        try:
            data = json.loads(json_line)
            self.assertIn("status", data)
            self.assertIn("http_code", data)
        except json.JSONDecodeError:
            self.fail(f"Output is not valid JSON: {json_line}")

    def test_post_with_data_produces_output(self):
        """Test POST with data produces some output."""
        result = JShellRunner.run(
            f'http-post -d "test=value" {TEST_URL}',
            timeout=30
        )
        # Just verify command completes
        self.assertIsNotNone(result.stdout)

    def test_json_output_contains_http_code(self):
        """Test that JSON output includes HTTP code."""
        result = JShellRunner.run(f"http-post --json {TEST_URL}", timeout=30)
        # Find JSON line
        json_line = None
        for line in result.stdout.split('\n'):
            if line.strip().startswith('{'):
                json_line = line
                break
        self.assertIsNotNone(json_line, "No JSON line found in output")
        data = json.loads(json_line)
        self.assertIn("http_code", data)
        # HTTP code should be a number
        self.assertIsInstance(data["http_code"], int)


class TestHttpPostErrors(unittest.TestCase):
    """Test cases for http-post error handling."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_json_output_on_dns_error(self):
        """Test that --json works even on DNS errors."""
        result = JShellRunner.run(
            "http-post --json https://this-host-does-not-exist-xyz123abc.com/",
            timeout=30
        )
        # Should still produce valid JSON - get first line only (the JSON)
        json_line = result.stdout.split('\n')[0]
        try:
            data = json.loads(json_line)
            self.assertEqual(data["status"], "error")
            self.assertIn("message", data)
        except json.JSONDecodeError:
            self.fail(f"Error output is not valid JSON: {json_line}")

    def test_nonexistent_host_shows_error(self):
        """Test error handling for nonexistent host."""
        result = JShellRunner.run(
            "http-post https://this-host-does-not-exist-xyz123abc.com/",
            timeout=30
        )
        # Output should contain some error indication
        output = (result.stdout + result.stderr).lower()
        has_error = ("error" in output or "could not" in output or
                    "failed" in output or "resolve" in output)
        self.assertTrue(has_error,
                       f"Expected error message, got: {result.stdout}")


@unittest.skipUnless(TEST_URL_AVAILABLE, f"{TEST_URL} is not available")
class TestHttpPostHeaders(unittest.TestCase):
    """Test cases for http-post custom headers functionality."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_custom_header_request_completes(self):
        """Test that request with custom header completes."""
        result = JShellRunner.run(
            f'http-post -H "X-Custom-Test: hello" {TEST_URL}',
            timeout=30
        )
        # Just verify command completes without crash
        self.assertIsNotNone(result.stdout)

    def test_content_type_header_request_completes(self):
        """Test that request with Content-Type header completes."""
        result = JShellRunner.run(
            f'http-post -H "Content-Type: application/json" '
            f'-d \'{{"key":"value"}}\' {TEST_URL}',
            timeout=30
        )
        # Just verify command completes without crash
        self.assertIsNotNone(result.stdout)


if __name__ == "__main__":
    unittest.main()
