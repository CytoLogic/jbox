#!/usr/bin/env python3
"""Unit tests for threaded builtin execution."""

import json
import unittest

from tests.helpers import JShellRunner


class TestThreadedBuiltinExecution(unittest.TestCase):
    """Test cases for threaded builtin command execution."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_single_builtin_in_thread(self):
        """Test single builtin executes correctly in thread."""
        result = JShellRunner.run("echo hello")
        self.assertEqual(result.returncode, 0)
        self.assertIn("hello", result.stdout)

    def test_multiple_builtins_in_sequence(self):
        """Test multiple builtins execute correctly in sequence."""
        result = JShellRunner.run("echo first; echo second; echo third")
        self.assertEqual(result.returncode, 0)
        self.assertIn("first", result.stdout)
        self.assertIn("second", result.stdout)
        self.assertIn("third", result.stdout)

    def test_builtin_with_pipe(self):
        """Test builtin in pipeline works."""
        result = JShellRunner.run('echo "test_content" | cat')
        self.assertEqual(result.returncode, 0)
        self.assertIn("test_content", result.stdout)

    def test_builtin_json_output(self):
        """Test builtin with JSON output works in thread."""
        data = JShellRunner.run_json("pwd --json")
        self.assertIn("cwd", data)
        self.assertTrue(data["cwd"].startswith("/"))

    def test_env_builtin_in_thread(self):
        """Test env builtin executes in thread."""
        result = JShellRunner.run("env")
        self.assertEqual(result.returncode, 0)
        self.assertIn("PATH=", result.stdout)

    def test_type_builtin_in_thread(self):
        """Test type builtin executes in thread."""
        result = JShellRunner.run("type pwd")
        self.assertEqual(result.returncode, 0)
        self.assertIn("builtin", result.stdout)

    def test_help_builtin_in_thread(self):
        """Test help builtin executes in thread."""
        result = JShellRunner.run("help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("echo", result.stdout)


class TestMainThreadBuiltins(unittest.TestCase):
    """Test cases for builtins that must run in main thread."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_cd_runs_in_main_thread(self):
        """Test cd command modifies shell state correctly."""
        result = JShellRunner.run("cd /tmp; pwd")
        self.assertEqual(result.returncode, 0)
        self.assertIn("/tmp", result.stdout)

    def test_export_runs_in_main_thread(self):
        """Test export command modifies environment correctly."""
        result = JShellRunner.run('export "TEST_VAR=test_value"; env')
        self.assertEqual(result.returncode, 0)
        self.assertIn("TEST_VAR=test_value", result.stdout)

    def test_unset_runs_in_main_thread(self):
        """Test unset command removes environment variable."""
        result = JShellRunner.run(
            'export "UNSET_TEST=value"; unset UNSET_TEST; env')
        self.assertEqual(result.returncode, 0)
        self.assertNotIn("UNSET_TEST=", result.stdout)


class TestThreadExitCodes(unittest.TestCase):
    """Test cases for exit code handling in threads."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_successful_builtin_exit_code(self):
        """Test successful builtin returns 0."""
        result = JShellRunner.run("echo success")
        self.assertEqual(result.returncode, 0)

    def test_builtin_help_exit_code(self):
        """Test --help flag returns 0."""
        result = JShellRunner.run("echo --help")
        self.assertEqual(result.returncode, 0)

    def test_cd_nonexistent_directory_exit_code(self):
        """Test cd to nonexistent directory returns non-zero."""
        result = JShellRunner.run("cd /nonexistent_dir_xyz123")
        self.assertNotEqual(result.returncode, 0)


class TestConcurrentBuiltins(unittest.TestCase):
    """Test cases for concurrent builtin execution."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_many_sequential_builtins(self):
        """Test many sequential builtins execute correctly."""
        commands = "; ".join([f"echo {i}" for i in range(10)])
        result = JShellRunner.run(commands)
        self.assertEqual(result.returncode, 0)
        for i in range(10):
            self.assertIn(str(i), result.stdout)

    def test_builtin_after_cd(self):
        """Test threaded builtin after main-thread cd."""
        result = JShellRunner.run("cd /tmp; pwd; echo done")
        self.assertEqual(result.returncode, 0)
        self.assertIn("/tmp", result.stdout)
        self.assertIn("done", result.stdout)


if __name__ == "__main__":
    unittest.main()
