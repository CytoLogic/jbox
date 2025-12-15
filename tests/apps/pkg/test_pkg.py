#!/usr/bin/env python3
"""Unit tests for the pkg command."""

import json
import os
import subprocess
import unittest
from pathlib import Path


class TestPkgCommand(unittest.TestCase):
    """Test cases for the pkg command."""

    PKG_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "pkg"

    @classmethod
    def setUpClass(cls):
        """Verify the pkg binary exists before running tests."""
        if not cls.PKG_BIN.exists():
            raise unittest.SkipTest(f"pkg binary not found at {cls.PKG_BIN}")

    def run_pkg(self, *args):
        """Run the pkg command with given arguments and return result."""
        cmd = [str(self.PKG_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_pkg("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: pkg", result.stdout)
        self.assertIn("--help", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_pkg("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: pkg", result.stdout)

    def test_help_shows_commands(self):
        """Test help shows all subcommands."""
        result = self.run_pkg("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("list", result.stdout)
        self.assertIn("info", result.stdout)
        self.assertIn("search", result.stdout)
        self.assertIn("install", result.stdout)
        self.assertIn("remove", result.stdout)
        self.assertIn("build", result.stdout)
        self.assertIn("check-update", result.stdout)
        self.assertIn("upgrade", result.stdout)
        self.assertIn("compile", result.stdout)

    def test_no_arguments_error(self):
        """Test pkg with no arguments shows error."""
        result = self.run_pkg()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Try 'pkg --help'", result.stderr)

    def test_list_not_implemented(self):
        """Test list subcommand returns not implemented."""
        result = self.run_pkg("list")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    def test_list_json(self):
        """Test list subcommand with --json flag."""
        result = self.run_pkg("list", "--json")
        self.assertNotEqual(result.returncode, 0)
        output = json.loads(result.stdout)
        self.assertEqual(output["status"], "not_implemented")
        self.assertIn("not yet implemented", output["message"])

    def test_info_requires_name(self):
        """Test info subcommand requires package name."""
        result = self.run_pkg("info")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("package name required", result.stderr)

    def test_info_requires_name_json(self):
        """Test info subcommand with --json shows error."""
        result = self.run_pkg("info", "--json")
        self.assertNotEqual(result.returncode, 0)
        output = json.loads(result.stdout)
        self.assertEqual(output["status"], "error")
        self.assertIn("package name required", output["message"])

    def test_info_not_implemented(self):
        """Test info subcommand returns not implemented."""
        result = self.run_pkg("info", "testpkg")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    def test_info_json(self):
        """Test info subcommand with --json flag."""
        result = self.run_pkg("info", "testpkg", "--json")
        self.assertNotEqual(result.returncode, 0)
        output = json.loads(result.stdout)
        self.assertEqual(output["status"], "not_implemented")
        self.assertEqual(output["package"], "testpkg")

    def test_search_requires_name(self):
        """Test search subcommand requires search term."""
        result = self.run_pkg("search")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("search term required", result.stderr)

    def test_search_not_implemented(self):
        """Test search subcommand returns not implemented."""
        result = self.run_pkg("search", "test")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    def test_search_json(self):
        """Test search subcommand with --json flag."""
        result = self.run_pkg("search", "test", "--json")
        self.assertNotEqual(result.returncode, 0)
        output = json.loads(result.stdout)
        self.assertEqual(output["status"], "not_implemented")
        self.assertEqual(output["query"], "test")

    def test_install_requires_name(self):
        """Test install subcommand requires package name."""
        result = self.run_pkg("install")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("package name required", result.stderr)

    def test_install_not_implemented(self):
        """Test install subcommand returns not implemented."""
        result = self.run_pkg("install", "testpkg")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    def test_install_json(self):
        """Test install subcommand with --json flag."""
        result = self.run_pkg("install", "testpkg", "--json")
        self.assertNotEqual(result.returncode, 0)
        output = json.loads(result.stdout)
        self.assertEqual(output["status"], "not_implemented")
        self.assertEqual(output["package"], "testpkg")

    def test_remove_requires_name(self):
        """Test remove subcommand requires package name."""
        result = self.run_pkg("remove")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("package name required", result.stderr)

    def test_remove_not_implemented(self):
        """Test remove subcommand returns not implemented."""
        result = self.run_pkg("remove", "testpkg")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    def test_remove_json(self):
        """Test remove subcommand with --json flag."""
        result = self.run_pkg("remove", "testpkg", "--json")
        self.assertNotEqual(result.returncode, 0)
        output = json.loads(result.stdout)
        self.assertEqual(output["status"], "not_implemented")
        self.assertEqual(output["package"], "testpkg")

    def test_build_not_implemented(self):
        """Test build subcommand returns not implemented."""
        result = self.run_pkg("build")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    def test_build_json(self):
        """Test build subcommand with --json flag."""
        result = self.run_pkg("build", "--json")
        self.assertNotEqual(result.returncode, 0)
        output = json.loads(result.stdout)
        self.assertEqual(output["status"], "not_implemented")

    def test_check_update_not_implemented(self):
        """Test check-update subcommand returns not implemented."""
        result = self.run_pkg("check-update")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    def test_check_update_json(self):
        """Test check-update subcommand with --json flag."""
        result = self.run_pkg("check-update", "--json")
        self.assertNotEqual(result.returncode, 0)
        output = json.loads(result.stdout)
        self.assertEqual(output["status"], "not_implemented")

    def test_upgrade_not_implemented(self):
        """Test upgrade subcommand returns not implemented."""
        result = self.run_pkg("upgrade")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    def test_upgrade_json(self):
        """Test upgrade subcommand with --json flag."""
        result = self.run_pkg("upgrade", "--json")
        self.assertNotEqual(result.returncode, 0)
        output = json.loads(result.stdout)
        self.assertEqual(output["status"], "not_implemented")

    def test_compile_not_implemented(self):
        """Test compile subcommand returns not implemented."""
        result = self.run_pkg("compile")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not yet implemented", result.stderr)

    def test_compile_json(self):
        """Test compile subcommand with --json flag."""
        result = self.run_pkg("compile", "--json")
        self.assertNotEqual(result.returncode, 0)
        output = json.loads(result.stdout)
        self.assertEqual(output["status"], "not_implemented")

    def test_unknown_command(self):
        """Test unknown subcommand shows error."""
        result = self.run_pkg("unknown")
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
