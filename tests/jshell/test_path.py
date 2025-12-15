#!/usr/bin/env python3
"""Unit tests for the path resolution system."""

import os
import shutil
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path

from tests.helpers import JShellRunner


class TestPathResolution(unittest.TestCase):
    """Test cases for command path resolution."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def setUp(self):
        """Set up test fixtures."""
        self.home = os.environ.get("HOME", "/tmp")
        self.jshell_bin = os.path.join(self.home, ".jshell", "bin")

    def test_jshell_bin_directory_created(self):
        """Test that ~/.jshell/bin directory is created on shell init."""
        result = JShellRunner.run("echo initialized")
        self.assertEqual(result.returncode, 0)
        self.assertTrue(os.path.isdir(self.jshell_bin))

    def test_path_contains_jshell_bin(self):
        """Test that PATH environment includes ~/.jshell/bin."""
        result = JShellRunner.run("env")
        self.assertEqual(result.returncode, 0)
        path_env = os.environ.get("PATH", "")
        self.assertIn(".jshell/bin", result.stdout)

    def test_local_bin_priority(self):
        """Test that ~/.jshell/bin takes priority over system PATH."""
        test_cmd_name = "_test_priority_cmd"
        test_cmd_path = os.path.join(self.jshell_bin, test_cmd_name)

        try:
            os.makedirs(self.jshell_bin, exist_ok=True)
            with open(test_cmd_path, "w") as f:
                f.write("#!/bin/sh\necho LOCAL_BIN\n")
            os.chmod(test_cmd_path, stat.S_IRWXU)

            result = JShellRunner.run(f"type {test_cmd_name}")
            self.assertEqual(result.returncode, 0)
            self.assertIn(self.jshell_bin, result.stdout)

        finally:
            if os.path.exists(test_cmd_path):
                os.remove(test_cmd_path)

    def test_system_path_fallback(self):
        """Test that system PATH is used when command not in local bin."""
        result = JShellRunner.run("type ls")
        self.assertEqual(result.returncode, 0)
        self.assertIn("external", result.stdout)

    def test_command_not_found(self):
        """Test handling of non-existent command."""
        result = JShellRunner.run("_nonexistent_command_xyz123")
        self.assertNotEqual(result.returncode, 0)

    def test_external_command_execution_from_local_bin(self):
        """Test that external commands from local bin execute correctly."""
        test_cmd_name = "_test_exec_cmd"
        test_cmd_path = os.path.join(self.jshell_bin, test_cmd_name)
        marker = "LOCAL_EXEC_TEST_MARKER"

        try:
            os.makedirs(self.jshell_bin, exist_ok=True)
            with open(test_cmd_path, "w") as f:
                f.write(f"#!/bin/sh\necho {marker}\n")
            os.chmod(test_cmd_path, stat.S_IRWXU)

            result = JShellRunner.run(test_cmd_name)
            self.assertEqual(result.returncode, 0)
            self.assertIn(marker, result.stdout)

        finally:
            if os.path.exists(test_cmd_path):
                os.remove(test_cmd_path)

    def test_absolute_path_execution(self):
        """Test execution of command by absolute path."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".sh",
                                          delete=False) as f:
            f.write("#!/bin/sh\necho ABSOLUTE_PATH_TEST\n")
            script_path = f.name

        try:
            os.chmod(script_path, stat.S_IRWXU)
            result = JShellRunner.run(script_path)
            self.assertEqual(result.returncode, 0)
            self.assertIn("ABSOLUTE_PATH_TEST", result.stdout)

        finally:
            os.remove(script_path)

    def test_relative_path_execution(self):
        """Test execution of command by relative path."""
        with tempfile.TemporaryDirectory() as tmpdir:
            script_path = os.path.join(tmpdir, "test_script.sh")
            with open(script_path, "w") as f:
                f.write("#!/bin/sh\necho RELATIVE_PATH_TEST\n")
            os.chmod(script_path, stat.S_IRWXU)

            result = JShellRunner.run("./test_script.sh", cwd=tmpdir)
            self.assertEqual(result.returncode, 0)
            self.assertIn("RELATIVE_PATH_TEST", result.stdout)

    def test_builtin_commands_not_affected(self):
        """Test that builtin commands still work (not resolved via PATH)."""
        result = JShellRunner.run("cd /tmp; pwd")
        self.assertEqual(result.returncode, 0)
        self.assertIn("/tmp", result.stdout)

    def test_pipeline_with_path_resolved_commands(self):
        """Test pipeline execution with path-resolved commands."""
        result = JShellRunner.run("echo hello | cat")
        self.assertEqual(result.returncode, 0)
        self.assertIn("hello", result.stdout)


class TestPathResolutionEdgeCases(unittest.TestCase):
    """Edge cases for path resolution."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_command_with_special_characters(self):
        """Test path resolution with spaces in directory names."""
        with tempfile.TemporaryDirectory() as tmpdir:
            space_dir = os.path.join(tmpdir, "dir with spaces")
            os.makedirs(space_dir)
            script_path = os.path.join(space_dir, "test_script.sh")
            with open(script_path, "w") as f:
                f.write("#!/bin/sh\necho SPECIAL_CHAR_TEST\n")
            os.chmod(script_path, stat.S_IRWXU)

            result = JShellRunner.run(f'"{script_path}"')
            self.assertEqual(result.returncode, 0)
            self.assertIn("SPECIAL_CHAR_TEST", result.stdout)

    def test_permission_denied_handling(self):
        """Test handling of non-executable files."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".sh",
                                          delete=False) as f:
            f.write("#!/bin/sh\necho test\n")
            script_path = f.name

        try:
            os.chmod(script_path, stat.S_IRUSR)

            result = JShellRunner.run(script_path)
            self.assertNotEqual(result.returncode, 0)

        finally:
            os.remove(script_path)


if __name__ == "__main__":
    unittest.main()
