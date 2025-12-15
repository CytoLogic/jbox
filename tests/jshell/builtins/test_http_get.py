#!/usr/bin/env python3
"""Unit tests for the http-get builtin command."""

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


class TestHttpGetHelp(unittest.TestCase):
    """Test cases for http-get help functionality."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_help_short(self):
        """Test -h flag shows help."""
        result = JShellRunner.run("http-get -h", timeout=30)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: http-get", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = JShellRunner.run("http-get --help", timeout=30)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: http-get", result.stdout)

    def test_help_shows_header_option(self):
        """Test that help mentions the -H header option."""
        result = JShellRunner.run("http-get -h", timeout=30)
        self.assertEqual(result.returncode, 0)
        self.assertIn("-H", result.stdout)
        self.assertIn("header", result.stdout.lower())

    def test_help_shows_examples(self):
        """Test that help includes examples."""
        result = JShellRunner.run("http-get -h", timeout=30)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Examples:", result.stdout)

    def test_help_shows_url_parameter(self):
        """Test that help shows URL parameter."""
        result = JShellRunner.run("http-get -h", timeout=30)
        self.assertEqual(result.returncode, 0)
        self.assertIn("URL", result.stdout)

    def test_missing_url_shows_error(self):
        """Test error when no URL argument is provided."""
        result = JShellRunner.run("http-get", timeout=30)
        output = (result.stdout + result.stderr).lower()
        has_error_info = ("url" in output or "error" in output or
                         "usage" in output or "required" in output)
        self.assertTrue(has_error_info,
                       f"Expected error about missing URL, got: {result.stdout}")


@unittest.skipUnless(TEST_URL_AVAILABLE, f"{TEST_URL} is not available")
class TestHttpGetNetwork(unittest.TestCase):
    """Test cases for http-get network functionality."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_fetch_simple_url(self):
        """Test fetching a simple URL."""
        result = JShellRunner.run(f"http-get {TEST_URL}", timeout=30)
        self.assertEqual(result.returncode, 0)
        # archlinux.org returns HTML
        self.assertIn("html", result.stdout.lower())

    def test_fetch_with_json_output(self):
        """Test fetching with --json output format."""
        result = JShellRunner.run(f"http-get --json {TEST_URL}", timeout=30)
        self.assertEqual(result.returncode, 0)
        # Parse the JSON output - find line starting with {
        json_line = None
        for line in result.stdout.split('\n'):
            if line.strip().startswith('{'):
                json_line = line
                break
        self.assertIsNotNone(json_line, "No JSON line found in output")
        try:
            data = json.loads(json_line)
            self.assertEqual(data["status"], "ok")
            self.assertEqual(data["http_code"], 200)
            self.assertIn("body", data)
            self.assertIn("headers", data)
        except json.JSONDecodeError:
            self.fail(f"Output is not valid JSON: {json_line}")

    def test_json_output_contains_content_type(self):
        """Test that JSON output includes content type."""
        result = JShellRunner.run(f"http-get --json {TEST_URL}", timeout=30)
        self.assertEqual(result.returncode, 0)
        # Find JSON line
        json_line = None
        for line in result.stdout.split('\n'):
            if line.strip().startswith('{'):
                json_line = line
                break
        self.assertIsNotNone(json_line, "No JSON line found in output")
        data = json.loads(json_line)
        self.assertIn("content_type", data)
        # archlinux.org returns text/html
        self.assertIn("text/html", data["content_type"].lower())

    def test_json_output_body_contains_html(self):
        """Test that JSON output body contains HTML content."""
        result = JShellRunner.run(f"http-get --json {TEST_URL}", timeout=30)
        self.assertEqual(result.returncode, 0)
        # Find JSON line
        json_line = None
        for line in result.stdout.split('\n'):
            if line.strip().startswith('{'):
                json_line = line
                break
        self.assertIsNotNone(json_line, "No JSON line found in output")
        data = json.loads(json_line)
        self.assertIn("body", data)
        # Body should contain HTML
        self.assertIn("<!doctype html>", data["body"].lower())


class TestHttpGetErrors(unittest.TestCase):
    """Test cases for http-get error handling."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_json_output_on_dns_error(self):
        """Test that --json works even on DNS errors."""
        result = JShellRunner.run(
            "http-get --json https://this-host-does-not-exist-xyz123abc.com/",
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
            "http-get https://this-host-does-not-exist-xyz123abc.com/",
            timeout=30
        )
        # Output should contain some error indication
        output = (result.stdout + result.stderr).lower()
        has_error = ("error" in output or "could not" in output or
                    "failed" in output or "resolve" in output)
        self.assertTrue(has_error,
                       f"Expected error message, got: {result.stdout}")


@unittest.skipUnless(TEST_URL_AVAILABLE, f"{TEST_URL} is not available")
class TestHttpGetHeaders(unittest.TestCase):
    """Test cases for http-get custom headers functionality."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_custom_header_request_succeeds(self):
        """Test that request with custom header succeeds."""
        result = JShellRunner.run(
            f'http-get -H "X-Custom-Test: hello" {TEST_URL}',
            timeout=30
        )
        self.assertEqual(result.returncode, 0)
        # Should return HTML content
        self.assertIn("html", result.stdout.lower())

    def test_accept_header_request_succeeds(self):
        """Test that request with Accept header succeeds."""
        result = JShellRunner.run(
            f'http-get -H "Accept: text/html" {TEST_URL}',
            timeout=30
        )
        self.assertEqual(result.returncode, 0)
        # Should return HTML content
        self.assertIn("html", result.stdout.lower())


if __name__ == "__main__":
    unittest.main()
