#!/usr/bin/env python3
"""Unit tests for the http-get builtin command."""

import json
import os
import subprocess
import unittest
import urllib.request
from pathlib import Path


def httpbin_available():
    """Check if httpbin.org is available."""
    try:
        urllib.request.urlopen("https://httpbin.org/get", timeout=10)
        return True
    except Exception:
        return False


# Cache the result at module load time
HTTPBIN_AVAILABLE = httpbin_available()


class TestHttpGetHelp(unittest.TestCase):
    """Test cases for http-get help functionality."""

    JBOX_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX_BIN.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX_BIN}")

    def run_http_get(self, *args):
        """Run the http-get command via jbox and return result."""
        cmd_str = "http-get " + " ".join(args) if args else "http-get"
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
        result = self.run_http_get("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: http-get", result.filtered_stdout)
        self.assertIn("--help", result.filtered_stdout)
        self.assertIn("--json", result.filtered_stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_http_get("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: http-get", result.filtered_stdout)

    def test_help_shows_header_option(self):
        """Test that help mentions the -H header option."""
        result = self.run_http_get("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("-H", result.filtered_stdout)
        self.assertIn("header", result.filtered_stdout.lower())

    def test_help_shows_examples(self):
        """Test that help includes examples."""
        result = self.run_http_get("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Examples:", result.filtered_stdout)

    def test_help_shows_url_parameter(self):
        """Test that help shows URL parameter."""
        result = self.run_http_get("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("URL", result.filtered_stdout)

    def test_missing_url_shows_error(self):
        """Test error when no URL argument is provided."""
        result = self.run_http_get()
        # Should show usage or error
        # The filtered output should contain either "URL" or "error" or usage info
        output = result.filtered_stdout.lower()
        has_error_info = ("url" in output or "error" in output or
                         "usage" in output or "required" in output)
        self.assertTrue(has_error_info,
                       f"Expected error about missing URL, got: {result.filtered_stdout}")


@unittest.skipUnless(HTTPBIN_AVAILABLE, "httpbin.org is not available")
class TestHttpGetNetwork(unittest.TestCase):
    """Test cases for http-get network functionality."""

    JBOX_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX_BIN.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX_BIN}")

    def run_http_get(self, *args):
        """Run the http-get command via jbox and return result."""
        cmd_str = "http-get " + " ".join(args)
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

    def test_fetch_simple_url(self):
        """Test fetching a simple URL."""
        result = self.run_http_get("https://httpbin.org/get")
        self.assertEqual(result.returncode, 0)
        # httpbin.org returns JSON with these fields
        self.assertIn("headers", result.filtered_stdout)
        self.assertIn("url", result.filtered_stdout)

    def test_fetch_with_json_output(self):
        """Test fetching with --json output format."""
        result = self.run_http_get("--json", "https://httpbin.org/get")
        self.assertEqual(result.returncode, 0)
        # Parse the JSON output
        try:
            data = json.loads(result.filtered_stdout)
            self.assertEqual(data["status"], "ok")
            self.assertEqual(data["http_code"], 200)
            self.assertIn("body", data)
            self.assertIn("headers", data)
        except json.JSONDecodeError:
            self.fail(f"Output is not valid JSON: {result.filtered_stdout}")

    def test_json_output_contains_content_type(self):
        """Test that JSON output includes content type."""
        result = self.run_http_get("--json", "https://httpbin.org/get")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.filtered_stdout)
        self.assertIn("content_type", data)
        # httpbin returns application/json
        self.assertIn("json", data["content_type"].lower())

    def test_custom_user_agent(self):
        """Test that custom User-Agent is sent."""
        result = self.run_http_get("https://httpbin.org/user-agent")
        self.assertEqual(result.returncode, 0)
        self.assertIn("jbox-http-get", result.filtered_stdout)


class TestHttpGetErrors(unittest.TestCase):
    """Test cases for http-get error handling."""

    JBOX_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX_BIN.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX_BIN}")

    def run_http_get(self, *args):
        """Run the http-get command via jbox and return result."""
        cmd_str = "http-get " + " ".join(args)
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
        result = self.run_http_get(
            "--json", "https://this-host-does-not-exist-xyz123abc.com/"
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
        result = self.run_http_get("https://this-host-does-not-exist-xyz123abc.com/")
        # Output should contain some error indication
        output = result.filtered_stdout.lower()
        has_error = ("error" in output or "could not" in output or
                    "failed" in output or "resolve" in output)
        self.assertTrue(has_error,
                       f"Expected error message, got: {result.filtered_stdout}")


@unittest.skipUnless(HTTPBIN_AVAILABLE, "httpbin.org is not available")
class TestHttpGetHeaders(unittest.TestCase):
    """Test cases for http-get custom headers functionality."""

    JBOX_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "jbox"

    @classmethod
    def setUpClass(cls):
        """Verify the jbox binary exists before running tests."""
        if not cls.JBOX_BIN.exists():
            raise unittest.SkipTest(f"jbox binary not found at {cls.JBOX_BIN}")

    def run_http_get(self, cmd_str):
        """Run the http-get command via jbox and return result."""
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

    def test_single_custom_header(self):
        """Test sending a single custom header."""
        result = self.run_http_get(
            'http-get -H "X-Custom-Test: hello" https://httpbin.org/headers'
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("X-Custom-Test", result.filtered_stdout)

    def test_accept_header(self):
        """Test setting Accept header."""
        result = self.run_http_get(
            'http-get -H "Accept: application/xml" https://httpbin.org/headers'
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("Accept", result.filtered_stdout)
        self.assertIn("application/xml", result.filtered_stdout)


if __name__ == "__main__":
    unittest.main()
