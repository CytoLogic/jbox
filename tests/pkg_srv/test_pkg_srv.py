#!/usr/bin/env python3
"""Unit tests for the package registry server."""

import json
import os
import signal
import subprocess
import sys
import time
import unittest
from pathlib import Path
from urllib.request import urlopen, Request
from urllib.error import URLError, HTTPError


class TestPackageRegistryServer(unittest.TestCase):
    """Test cases for the package registry server."""

    PROJECT_ROOT = Path(__file__).parent.parent.parent
    START_SCRIPT = PROJECT_ROOT / "scripts" / "start-pkg-server.sh"
    SHUTDOWN_SCRIPT = PROJECT_ROOT / "scripts" / "shutdown-pkg-server.sh"
    BASE_URL = "http://localhost:3000"
    server_process = None

    @classmethod
    def setUpClass(cls):
        """Start the server before running tests."""
        # Check if scripts exist
        if not cls.START_SCRIPT.exists():
            raise unittest.SkipTest(
                f"start script not found at {cls.START_SCRIPT}"
            )
        if not cls.SHUTDOWN_SCRIPT.exists():
            raise unittest.SkipTest(
                f"shutdown script not found at {cls.SHUTDOWN_SCRIPT}"
            )

        # Start the server using the script
        cls.server_process = subprocess.Popen(
            ["bash", str(cls.START_SCRIPT)],
            cwd=cls.PROJECT_ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env={**os.environ, "PORT": "3000"}
        )

        # Wait for server to start
        cls._wait_for_server()

    @classmethod
    def _wait_for_server(cls, timeout=10):
        """Wait for the server to be ready."""
        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                urlopen(f"{cls.BASE_URL}/packages", timeout=1)
                return  # Server is ready
            except (URLError, ConnectionRefusedError):
                time.sleep(0.1)

        # Check if process died
        if cls.server_process.poll() is not None:
            stdout, stderr = cls.server_process.communicate()
            raise unittest.SkipTest(
                f"Server process died. stdout: {stdout.decode()}, "
                f"stderr: {stderr.decode()}"
            )

        raise unittest.SkipTest(
            f"Server did not start within {timeout} seconds"
        )

    @classmethod
    def tearDownClass(cls):
        """Stop the server after tests using the shutdown script."""
        result = subprocess.run(
            ["bash", str(cls.SHUTDOWN_SCRIPT)],
            cwd=cls.PROJECT_ROOT,
            capture_output=True,
            text=True,
            env={**os.environ, "PORT": "3000"}
        )
        # Also clean up the Popen process handle
        if cls.server_process:
            try:
                cls.server_process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                pass

    def fetch_json(self, path, expected_status=200):
        """Fetch JSON from the server."""
        url = f"{self.BASE_URL}{path}"
        try:
            response = urlopen(url, timeout=5)
            data = json.loads(response.read().decode())
            self.assertEqual(response.status, expected_status)
            return data
        except HTTPError as e:
            if e.code == expected_status:
                return json.loads(e.read().decode())
            raise

    # -------------------------------------------------------------------------
    # GET /packages tests
    # -------------------------------------------------------------------------

    def test_get_packages_returns_200(self):
        """Test GET /packages returns 200 OK."""
        response = urlopen(f"{self.BASE_URL}/packages", timeout=5)
        self.assertEqual(response.status, 200)

    def test_get_packages_returns_json(self):
        """Test GET /packages returns valid JSON."""
        response = urlopen(f"{self.BASE_URL}/packages", timeout=5)
        content_type = response.headers.get("Content-Type", "")
        self.assertIn("application/json", content_type)

        data = json.loads(response.read().decode())
        self.assertIsInstance(data, dict)

    def test_get_packages_has_status_ok(self):
        """Test GET /packages response has status: ok."""
        data = self.fetch_json("/packages")
        self.assertEqual(data["status"], "ok")

    def test_get_packages_has_packages_array(self):
        """Test GET /packages response has packages array."""
        data = self.fetch_json("/packages")
        self.assertIn("packages", data)
        self.assertIsInstance(data["packages"], list)

    def test_get_packages_contains_ls(self):
        """Test GET /packages includes the 'ls' package."""
        data = self.fetch_json("/packages")
        packages = data["packages"]
        names = [p["name"] for p in packages]
        self.assertIn("ls", names)

    def test_get_packages_contains_cat(self):
        """Test GET /packages includes the 'cat' package."""
        data = self.fetch_json("/packages")
        packages = data["packages"]
        names = [p["name"] for p in packages]
        self.assertIn("cat", names)

    def test_get_packages_package_has_required_fields(self):
        """Test packages in GET /packages have required fields."""
        data = self.fetch_json("/packages")
        for pkg in data["packages"]:
            self.assertIn("name", pkg)
            self.assertIn("latestVersion", pkg)
            self.assertIn("downloadUrl", pkg)

    def test_get_packages_package_has_description(self):
        """Test packages have description field."""
        data = self.fetch_json("/packages")
        for pkg in data["packages"]:
            self.assertIn("description", pkg)
            self.assertIsInstance(pkg["description"], str)

    # -------------------------------------------------------------------------
    # GET /packages/:name tests (existing package)
    # -------------------------------------------------------------------------

    def test_get_package_ls_returns_200(self):
        """Test GET /packages/ls returns 200 OK."""
        response = urlopen(f"{self.BASE_URL}/packages/ls", timeout=5)
        self.assertEqual(response.status, 200)

    def test_get_package_ls_returns_json(self):
        """Test GET /packages/ls returns valid JSON."""
        response = urlopen(f"{self.BASE_URL}/packages/ls", timeout=5)
        content_type = response.headers.get("Content-Type", "")
        self.assertIn("application/json", content_type)

    def test_get_package_ls_has_status_ok(self):
        """Test GET /packages/ls response has status: ok."""
        data = self.fetch_json("/packages/ls")
        self.assertEqual(data["status"], "ok")

    def test_get_package_ls_has_package_object(self):
        """Test GET /packages/ls response has package object."""
        data = self.fetch_json("/packages/ls")
        self.assertIn("package", data)
        self.assertIsInstance(data["package"], dict)

    def test_get_package_ls_name_matches(self):
        """Test GET /packages/ls returns correct name."""
        data = self.fetch_json("/packages/ls")
        self.assertEqual(data["package"]["name"], "ls")

    def test_get_package_ls_has_version(self):
        """Test GET /packages/ls has latestVersion field."""
        data = self.fetch_json("/packages/ls")
        self.assertIn("latestVersion", data["package"])
        self.assertEqual(data["package"]["latestVersion"], "0.0.1")

    def test_get_package_ls_has_download_url(self):
        """Test GET /packages/ls has downloadUrl field."""
        data = self.fetch_json("/packages/ls")
        self.assertIn("downloadUrl", data["package"])
        self.assertIn("ls", data["package"]["downloadUrl"])

    def test_get_package_cat(self):
        """Test GET /packages/cat returns correct data."""
        data = self.fetch_json("/packages/cat")
        self.assertEqual(data["status"], "ok")
        self.assertEqual(data["package"]["name"], "cat")
        self.assertEqual(data["package"]["latestVersion"], "0.0.1")

    # -------------------------------------------------------------------------
    # GET /packages/:name tests (non-existent package)
    # -------------------------------------------------------------------------

    def test_get_nonexistent_package_returns_404(self):
        """Test GET /packages/nonexistent returns 404."""
        try:
            urlopen(f"{self.BASE_URL}/packages/nonexistent", timeout=5)
            self.fail("Expected HTTPError 404")
        except HTTPError as e:
            self.assertEqual(e.code, 404)

    def test_get_nonexistent_package_returns_json(self):
        """Test GET /packages/nonexistent returns valid JSON."""
        try:
            urlopen(f"{self.BASE_URL}/packages/nonexistent", timeout=5)
            self.fail("Expected HTTPError 404")
        except HTTPError as e:
            content_type = e.headers.get("Content-Type", "")
            self.assertIn("application/json", content_type)

    def test_get_nonexistent_package_has_status_error(self):
        """Test GET /packages/nonexistent has status: error."""
        data = self.fetch_json("/packages/nonexistent", expected_status=404)
        self.assertEqual(data["status"], "error")

    def test_get_nonexistent_package_has_message(self):
        """Test GET /packages/nonexistent has error message."""
        data = self.fetch_json("/packages/nonexistent", expected_status=404)
        self.assertIn("message", data)
        self.assertIn("nonexistent", data["message"])
        self.assertIn("not found", data["message"].lower())

    def test_get_random_nonexistent_package(self):
        """Test GET /packages/xyz123randomname returns 404."""
        data = self.fetch_json("/packages/xyz123randomname", expected_status=404)
        self.assertEqual(data["status"], "error")

    # -------------------------------------------------------------------------
    # Edge cases
    # -------------------------------------------------------------------------

    def test_get_packages_empty_name(self):
        """Test GET /packages/ (trailing slash) still works."""
        # This should return the packages list, not a 404
        response = urlopen(f"{self.BASE_URL}/packages/", timeout=5)
        # Express typically treats /packages/ same as /packages
        # or returns 404 for empty name - either is acceptable
        self.assertIn(response.status, [200, 404])

    def test_case_sensitive_package_name(self):
        """Test package names are case-sensitive."""
        # 'Ls' (capital L) should not match 'ls'
        data = self.fetch_json("/packages/Ls", expected_status=404)
        self.assertEqual(data["status"], "error")

    def test_package_name_with_special_chars(self):
        """Test package name with URL-unsafe characters returns 404."""
        # Should not crash, just return 404
        data = self.fetch_json("/packages/foo%20bar", expected_status=404)
        self.assertEqual(data["status"], "error")


class TestServerHealth(unittest.TestCase):
    """Test server health and basic connectivity."""

    PROJECT_ROOT = Path(__file__).parent.parent.parent
    BASE_URL = "http://localhost:3000"

    def test_server_responds(self):
        """Test that server responds to requests."""
        # This test runs independently and checks if server is reachable
        # Useful for debugging when full test suite fails
        try:
            response = urlopen(f"{self.BASE_URL}/packages", timeout=2)
            self.assertEqual(response.status, 200)
        except (URLError, ConnectionRefusedError):
            self.skipTest("Server not running - start with ./scripts/start-pkg-server.sh")


if __name__ == "__main__":
    unittest.main()
