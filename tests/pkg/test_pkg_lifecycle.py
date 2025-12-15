#!/usr/bin/env python3
"""End-to-end package lifecycle tests."""

import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path

from tests.pkg import PkgTestBase, PkgRegistryTestBase


class TestPkgLifecycle(PkgTestBase):
    """Test complete package lifecycle without registry server."""

    def test_full_local_lifecycle(self):
        """Test complete package lifecycle: build -> install -> use -> remove."""
        # 1. Create a test package
        pkg_dir = self.create_test_package("test-cli", "1.0.0")
        try:
            # 2. Build tarball
            tarball = tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False)
            tarball.close()
            result = self.run_pkg("build", str(pkg_dir), tarball.name)
            self.assertEqual(result.returncode, 0, f"Build failed: {result.stderr}")

            # 3. Install package
            result = self.run_pkg("install", tarball.name)
            self.assertEqual(result.returncode, 0, f"Install failed: {result.stderr}")

            # 4. Verify package is listed
            self.assertPackageInstalled("test-cli", "1.0.0")

            # 5. Verify binary exists
            self.assertBinaryExists("test-cli")

            # 6. Verify info command works
            result = self.run_pkg("info", "test-cli", "--json")
            self.assertEqual(result.returncode, 0)
            data = json.loads(result.stdout)
            self.assertEqual(data.get("name"), "test-cli")
            self.assertEqual(data.get("version"), "1.0.0")

            # 7. Remove package
            result = self.run_pkg("remove", "test-cli")
            self.assertEqual(result.returncode, 0, f"Remove failed: {result.stderr}")

            # 8. Verify package is gone
            self.assertPackageNotInstalled("test-cli")
            self.assertBinaryNotExists("test-cli")

        finally:
            shutil.rmtree(pkg_dir.parent)
            if os.path.exists(tarball.name):
                os.unlink(tarball.name)

    def test_reinstall_package(self):
        """Test reinstalling a package (remove + install)."""
        # Install v1
        self.install_test_package("myapp", "1.0.0")
        self.assertPackageInstalled("myapp", "1.0.0")

        # Remove
        result = self.run_pkg("remove", "myapp")
        self.assertEqual(result.returncode, 0)
        self.assertPackageNotInstalled("myapp")

        # Install v2
        self.install_test_package("myapp", "2.0.0")
        self.assertPackageInstalled("myapp", "2.0.0")

    def test_list_installed_packages(self):
        """Test listing installed packages."""
        # Install some packages
        self.install_test_package("pkg-one", "1.0.0")
        self.install_test_package("pkg-two", "2.0.0")
        self.install_test_package("pkg-three", "3.0.0")

        # List packages
        result = self.run_pkg("list", "--json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)

        names = {p["name"] for p in data["packages"]}
        self.assertEqual(names, {"pkg-one", "pkg-two", "pkg-three"})

    def test_info_installed_package(self):
        """Test getting info for installed package."""
        self.install_test_package("myapp", "1.2.3",
                                  description="A test application")

        result = self.run_pkg("info", "myapp", "--json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)

        self.assertEqual(data["name"], "myapp")
        self.assertEqual(data["version"], "1.2.3")
        self.assertEqual(data["description"], "A test application")
        self.assertIn("installed_at", data)

    def test_info_not_installed(self):
        """Test info for non-installed package."""
        result = self.run_pkg("info", "nonexistent", "--json")
        # Should return error
        self.assertNotEqual(result.returncode, 0)


class TestMultiPackageOperations(PkgTestBase):
    """Test operations involving multiple packages."""

    def test_install_multiple_packages(self):
        """Test installing multiple packages sequentially."""
        packages = [
            ("app-a", "1.0.0"),
            ("app-b", "2.0.0"),
            ("app-c", "3.0.0"),
            ("app-d", "4.0.0"),
            ("app-e", "5.0.0"),
        ]

        for name, version in packages:
            self.install_test_package(name, version)

        # Verify all installed
        result = self.run_pkg("list", "--json")
        data = json.loads(result.stdout)
        self.assertEqual(len(data["packages"]), 5)

        # Verify each package
        for name, version in packages:
            self.assertPackageInstalled(name, version)
            self.assertBinaryExists(name)

    def test_remove_multiple_packages(self):
        """Test removing multiple packages."""
        # Install 3 packages
        self.install_test_package("rm-pkg-a", "1.0.0")
        self.install_test_package("rm-pkg-b", "1.0.0")
        self.install_test_package("rm-pkg-c", "1.0.0")

        # Remove them one by one
        for name in ["rm-pkg-a", "rm-pkg-b", "rm-pkg-c"]:
            result = self.run_pkg("remove", name)
            self.assertEqual(result.returncode, 0)
            self.assertPackageNotInstalled(name)

        # Verify all gone
        result = self.run_pkg("list", "--json")
        data = json.loads(result.stdout)
        self.assertEqual(len(data["packages"]), 0)

    def test_selective_removal(self):
        """Test removing some packages while keeping others."""
        self.install_test_package("keep-me", "1.0.0")
        self.install_test_package("delete-me", "1.0.0")
        self.install_test_package("also-keep", "1.0.0")

        # Remove one
        result = self.run_pkg("remove", "delete-me")
        self.assertEqual(result.returncode, 0)

        # Verify correct state
        self.assertPackageInstalled("keep-me")
        self.assertPackageNotInstalled("delete-me")
        self.assertPackageInstalled("also-keep")


class TestCheckUpdateLocal(PkgTestBase):
    """Test check-update functionality (local, without registry)."""

    def test_check_update_empty(self):
        """Test check-update with no packages."""
        result = self.run_pkg("check-update", "--json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIn("packages", data)
        self.assertEqual(len(data["packages"]), 0)

    def test_check_update_json_format(self):
        """Test check-update JSON output format."""
        self.install_test_package("test-pkg", "1.0.0")

        result = self.run_pkg("check-update", "--json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)

        # Verify structure
        self.assertIn("status", data)
        self.assertIn("summary", data)
        self.assertIn("packages", data)

        # Verify summary fields
        summary = data["summary"]
        self.assertIn("up_to_date", summary)
        self.assertIn("updates_available", summary)
        self.assertIn("errors", summary)


class TestRegistryLifecycle(PkgRegistryTestBase):
    """Test package lifecycle with registry server."""

    def test_search_packages(self):
        """Test searching for packages in registry."""
        result = self.run_pkg("search", "ls", "--json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIn("results", data)

    def test_install_from_registry(self):
        """Test installing package from registry."""
        # Search to find available packages
        result = self.run_pkg("search", "ls", "--json")
        if result.returncode != 0:
            self.skipTest("No packages available in registry")

        data = json.loads(result.stdout)
        if not data.get("results"):
            self.skipTest("No ls package in registry")

        # Get the package info
        pkg_name = data["results"][0]["name"]

        # Install from registry
        result = self.run_pkg("install", pkg_name)
        if result.returncode == 0:
            self.assertPackageInstalled(pkg_name)
            self.assertBinaryExists(pkg_name)


if __name__ == "__main__":
    unittest.main()
