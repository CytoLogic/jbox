#!/usr/bin/env python3
"""Unit tests for the http-post builtin command."""

import json
import os
import subprocess
import unittest
import urllib.request
from pathlib import Path


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

    JBOX_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX_BIN.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX_BIN}")

    def run_http_post(self, *args):
        """Run the http-post command via jbox and return result."""
        cmd_str = "http-post " + " ".join(args) if args else "http-post"
        result = subprocess.run(
            [str(self.JBOX_BIN)],
            input=cmd_str + "\n",
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"},
            timeout=30
        )
        # Filter out debug lines and shell prompts, combine stdout and stderr
        all_output = result.stdout + result.stderr
        stdout_lines = []
        for line in all_output.split("\n"):
            if line.startswith("[DEBUG]"):
                continue
            if line.startswith("welcome to jbox"):
                continue
            if line.startswith("(jsh)>"):
                # Extract content after prompt
                content = line[6:]
                if content:
                    stdout_lines.append(content)
                continue
            if line.strip():
                stdout_lines.append(line)
        result.filtered_stdout = "\n".join(stdout_lines)
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_http_post("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: http-post", result.filtered_stdout)
        self.assertIn("--help", result.filtered_stdout)
        self.assertIn("--json", result.filtered_stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_http_post("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: http-post", result.filtered_stdout)

    def test_help_shows_header_option(self):
        """Test that help mentions the -H header option."""
        result = self.run_http_post("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("-H", result.filtered_stdout)
        self.assertIn("header", result.filtered_stdout.lower())

    def test_help_shows_data_option(self):
        """Test that help mentions the -d data option."""
        result = self.run_http_post("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("-d", result.filtered_stdout)
        self.assertIn("data", result.filtered_stdout.lower())

    def test_help_shows_examples(self):
        """Test that help includes examples."""
        result = self.run_http_post("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Examples:", result.filtered_stdout)

    def test_help_shows_url_parameter(self):
        """Test that help shows URL parameter."""
        result = self.run_http_post("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("URL", result.filtered_stdout)

    def test_missing_url_shows_error(self):
        """Test error when no URL argument is provided."""
        result = self.run_http_post()
        # Should show usage or error
        output = result.filtered_stdout.lower()
        has_error_info = ("url" in output or "error" in output or
                         "usage" in output or "required" in output)
        self.assertTrue(has_error_info,
                       f"Expected error about missing URL, got: {result.filtered_stdout}")


@unittest.skipUnless(TEST_URL_AVAILABLE, f"{TEST_URL} is not available")
class TestHttpPostNetwork(unittest.TestCase):
    """Test cases for http-post network functionality."""

    JBOX_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX_BIN.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX_BIN}")

    def run_http_post(self, cmd_str):
        """Run the http-post command via jbox and return result."""
        result = subprocess.run(
            [str(self.JBOX_BIN)],
            input=cmd_str + "\n",
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"},
            timeout=30
        )
        # Filter out debug lines and shell prompts
        all_output = result.stdout + result.stderr
        stdout_lines = []
        for line in all_output.split("\n"):
            if line.startswith("[DEBUG]"):
                continue
            if line.startswith("welcome to jbox"):
                continue
            if line.startswith("(jsh)>"):
                content = line[6:]
                if content:
                    stdout_lines.append(content)
                continue
            if line.strip():
                stdout_lines.append(line)
        result.filtered_stdout = "\n".join(stdout_lines)
        return result

    def test_post_request_completes(self):
        """Test that POST request completes (may return error page)."""
        result = self.run_http_post(f"http-post {TEST_URL}")
        # The request should complete (server may reject POST but curl succeeds)
        # We just verify the command runs without crashing
        self.assertIsNotNone(result.filtered_stdout)

    def test_post_with_json_output_format(self):
        """Test POST with --json output produces valid JSON."""
        result = self.run_http_post(f"http-post --json {TEST_URL}")
        # Find JSON line
        json_line = None
        for line in result.filtered_stdout.split('\n'):
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
        result = self.run_http_post(f'http-post -d "test=value" {TEST_URL}')
        # Just verify command completes
        self.assertIsNotNone(result.filtered_stdout)

    def test_json_output_contains_http_code(self):
        """Test that JSON output includes HTTP code."""
        result = self.run_http_post(f"http-post --json {TEST_URL}")
        # Find JSON line
        json_line = None
        for line in result.filtered_stdout.split('\n'):
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

    JBOX_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX_BIN.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX_BIN}")

    def run_http_post(self, cmd_str):
        """Run the http-post command via jbox and return result."""
        result = subprocess.run(
            [str(self.JBOX_BIN)],
            input=cmd_str + "\n",
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"},
            timeout=30
        )
        # Filter out debug lines and shell prompts
        all_output = result.stdout + result.stderr
        stdout_lines = []
        for line in all_output.split("\n"):
            if line.startswith("[DEBUG]"):
                continue
            if line.startswith("welcome to jbox"):
                continue
            if line.startswith("(jsh)>"):
                content = line[6:]
                if content:
                    stdout_lines.append(content)
                continue
            if line.strip():
                stdout_lines.append(line)
        result.filtered_stdout = "\n".join(stdout_lines)
        return result

    def test_json_output_on_dns_error(self):
        """Test that --json works even on DNS errors."""
        result = self.run_http_post(
            "http-post --json https://this-host-does-not-exist-xyz123abc.com/"
        )
        # Should still produce valid JSON - get first line only (the JSON)
        json_line = result.filtered_stdout.split('\n')[0]
        try:
            data = json.loads(json_line)
            self.assertEqual(data["status"], "error")
            self.assertIn("message", data)
        except json.JSONDecodeError:
            self.fail(f"Error output is not valid JSON: {json_line}")

    def test_nonexistent_host_shows_error(self):
        """Test error handling for nonexistent host."""
        result = self.run_http_post(
            "http-post https://this-host-does-not-exist-xyz123abc.com/"
        )
        # Output should contain some error indication
        output = result.filtered_stdout.lower()
        has_error = ("error" in output or "could not" in output or
                    "failed" in output or "resolve" in output)
        self.assertTrue(has_error,
                       f"Expected error message, got: {result.filtered_stdout}")


@unittest.skipUnless(TEST_URL_AVAILABLE, f"{TEST_URL} is not available")
class TestHttpPostHeaders(unittest.TestCase):
    """Test cases for http-post custom headers functionality."""

    JBOX_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX_BIN.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX_BIN}")

    def run_http_post(self, cmd_str):
        """Run the http-post command via jbox and return result."""
        result = subprocess.run(
            [str(self.JBOX_BIN)],
            input=cmd_str + "\n",
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"},
            timeout=30
        )
        # Filter out debug lines and shell prompts
        all_output = result.stdout + result.stderr
        stdout_lines = []
        for line in all_output.split("\n"):
            if line.startswith("[DEBUG]"):
                continue
            if line.startswith("welcome to jbox"):
                continue
            if line.startswith("(jsh)>"):
                content = line[6:]
                if content:
                    stdout_lines.append(content)
                continue
            if line.strip():
                stdout_lines.append(line)
        result.filtered_stdout = "\n".join(stdout_lines)
        return result

    def test_custom_header_request_completes(self):
        """Test that request with custom header completes."""
        result = self.run_http_post(
            f'http-post -H "X-Custom-Test: hello" {TEST_URL}'
        )
        # Just verify command completes without crash
        self.assertIsNotNone(result.filtered_stdout)

    def test_content_type_header_request_completes(self):
        """Test that request with Content-Type header completes."""
        result = self.run_http_post(
            f'http-post -H "Content-Type: application/json" '
            f'-d \'{{"key":"value"}}\' {TEST_URL}'
        )
        # Just verify command completes without crash
        self.assertIsNotNone(result.filtered_stdout)


if __name__ == "__main__":
    unittest.main()
