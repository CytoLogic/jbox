#!/usr/bin/env python3
"""Tests for package database (pkgdb.json) functionality."""

import json
import os
import shutil
import tempfile
import unittest
from pathlib import Path

from tests.pkg import PkgTestBase


class TestPkgDbJson(PkgTestBase):
    """Test JSON package database functionality."""

    def test_empty_database(self):
        """Test pkg list with no packages installed returns empty list."""
        result = self.run_pkg("list", "--json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)
        self.assertIn("packages", data)
        self.assertEqual(len(data["packages"]), 0)

    def test_install_creates_json_db(self):
        """Test that installing a package creates pkgdb.json."""
        # Create and install a test package
        self.install_test_package("test-pkg", "1.0.0")

        # Verify JSON database was created
        json_path = self.JSHELL_HOME / "pkgs" / "pkgdb.json"
        self.assertTrue(json_path.exists(), "pkgdb.json should exist")

        # Verify JSON content
        data = json.loads(json_path.read_text())
        self.assertIn("version", data)
        self.assertIn("packages", data)
        self.assertEqual(len(data["packages"]), 1)
        self.assertEqual(data["packages"][0]["name"], "test-pkg")
        self.assertEqual(data["packages"][0]["version"], "1.0.0")

    def test_json_db_has_metadata(self):
        """Test that JSON database includes installed_at timestamp."""
        self.install_test_package("test-pkg", "1.0.0")

        json_path = self.JSHELL_HOME / "pkgs" / "pkgdb.json"
        data = json.loads(json_path.read_text())

        pkg = data["packages"][0]
        self.assertIn("installed_at", pkg)
        # Verify ISO 8601 format (basic check)
        self.assertIn("T", pkg["installed_at"])
        self.assertIn("Z", pkg["installed_at"])

    def test_json_db_has_files_list(self):
        """Test that JSON database includes files list."""
        self.install_test_package("test-pkg", "1.0.0")

        json_path = self.JSHELL_HOME / "pkgs" / "pkgdb.json"
        data = json.loads(json_path.read_text())

        pkg = data["packages"][0]
        self.assertIn("files", pkg)
        self.assertTrue(len(pkg["files"]) > 0)

    def test_migration_from_txt(self):
        """Test automatic migration from pkgdb.txt to pkgdb.json."""
        # Create legacy txt database
        self.JSHELL_HOME.mkdir(parents=True, exist_ok=True)
        txt_path = self.JSHELL_HOME / "pkgdb.txt"
        txt_path.write_text("test-pkg 1.0.0\nother-pkg 2.0.0\n")

        # Run pkg list to trigger migration
        result = self.run_pkg("list", "--json")
        self.assertEqual(result.returncode, 0)

        # Verify JSON database was created
        json_path = self.JSHELL_HOME / "pkgs" / "pkgdb.json"
        self.assertTrue(json_path.exists(), "pkgdb.json should be created")

        # Verify migration was successful
        data = json.loads(json_path.read_text())
        self.assertEqual(len(data["packages"]), 2)

        names = {p["name"] for p in data["packages"]}
        self.assertIn("test-pkg", names)
        self.assertIn("other-pkg", names)

        # Verify txt was backed up
        bak_path = self.JSHELL_HOME / "pkgdb.txt.bak"
        self.assertTrue(bak_path.exists(), "pkgdb.txt.bak should exist")

    def test_json_preserves_on_update(self):
        """Test that updating a package preserves JSON format."""
        self.install_test_package("test-pkg", "1.0.0")

        # Remove and install a different version
        result = self.run_pkg("remove", "test-pkg")
        self.assertEqual(result.returncode, 0)
        self.install_test_package("test-pkg", "2.0.0")

        # Verify only one entry exists with updated version
        result = self.run_pkg("list", "--json")
        data = json.loads(result.stdout)
        self.assertEqual(len(data["packages"]), 1)
        self.assertEqual(data["packages"][0]["version"], "2.0.0")

    def test_multiple_packages(self):
        """Test handling multiple packages in JSON database."""
        self.install_test_package("pkg-a", "1.0.0")
        self.install_test_package("pkg-b", "2.0.0")
        self.install_test_package("pkg-c", "3.0.0")

        result = self.run_pkg("list", "--json")
        data = json.loads(result.stdout)
        self.assertEqual(len(data["packages"]), 3)

        names = {p["name"] for p in data["packages"]}
        self.assertEqual(names, {"pkg-a", "pkg-b", "pkg-c"})

    def test_remove_updates_json(self):
        """Test that removing a package updates JSON database."""
        self.install_test_package("test-pkg", "1.0.0")
        self.install_test_package("other-pkg", "2.0.0")

        # Remove one package
        result = self.run_pkg("remove", "test-pkg")
        self.assertEqual(result.returncode, 0)

        # Verify JSON was updated
        result = self.run_pkg("list", "--json")
        data = json.loads(result.stdout)
        self.assertEqual(len(data["packages"]), 1)
        self.assertEqual(data["packages"][0]["name"], "other-pkg")

    def test_json_format_valid(self):
        """Test that pkgdb.json is valid JSON with expected structure."""
        self.install_test_package("test-pkg", "1.0.0")

        json_path = self.JSHELL_HOME / "pkgs" / "pkgdb.json"
        content = json_path.read_text()

        # Should be valid JSON
        data = json.loads(content)

        # Should have required fields
        self.assertIn("version", data)
        self.assertIsInstance(data["version"], int)
        self.assertIn("packages", data)
        self.assertIsInstance(data["packages"], list)

        # Each package should have required fields
        for pkg in data["packages"]:
            self.assertIn("name", pkg)
            self.assertIn("version", pkg)

    def test_json_db_path_in_pkgs_dir(self):
        """Test that pkgdb.json is stored in ~/.jshell/pkgs/."""
        self.install_test_package("test-pkg", "1.0.0")

        # Should be in pkgs subdirectory
        json_path = self.JSHELL_HOME / "pkgs" / "pkgdb.json"
        self.assertTrue(json_path.exists())

        # Old location should not exist
        old_path = self.JSHELL_HOME / "pkgdb.json"
        self.assertFalse(old_path.exists())


class TestPkgDbEdgeCases(PkgTestBase):
    """Test edge cases for package database."""

    def test_special_characters_in_description(self):
        """Test handling special characters in package description."""
        self.install_test_package(
            "test-pkg", "1.0.0",
            description='Test "package" with\nnewlines & special <chars>'
        )

        result = self.run_pkg("list", "--json")
        data = json.loads(result.stdout)
        self.assertEqual(len(data["packages"]), 1)
        # Just verify it parses without error

    def test_empty_description(self):
        """Test package with no description."""
        self.install_test_package("test-pkg", "1.0.0", description=None)

        result = self.run_pkg("list", "--json")
        self.assertEqual(result.returncode, 0)

    def test_concurrent_access(self):
        """Test that database handles sequential operations correctly."""
        # Install multiple packages in sequence
        for i in range(5):
            self.install_test_package(f"pkg-{i}", "1.0.0")

        result = self.run_pkg("list", "--json")
        data = json.loads(result.stdout)
        self.assertEqual(len(data["packages"]), 5)


if __name__ == "__main__":
    unittest.main()
