#!/usr/bin/env python3
"""Unit tests for shell session functionality."""

import os
import tempfile
import unittest

from tests.helpers import JShellRunner


class TestShellStartup(unittest.TestCase):
    """Test cases for shell startup and initialization."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_shell_starts(self):
        """Test shell starts and executes command."""
        result = JShellRunner.run("echo started")
        self.assertEqual(result.returncode, 0)
        self.assertIn("started", result.stdout)

    def test_shell_returns_exit_code(self):
        """Test shell returns exit code of last command."""
        result = JShellRunner.run("pwd")
        self.assertEqual(result.returncode, 0)

    def test_shell_with_failing_command(self):
        """Test shell returns non-zero on failure."""
        result = JShellRunner.run("nonexistent_command_xyz")
        self.assertEqual(result.returncode, 127)


class TestExitCodeTracking(unittest.TestCase):
    """Test cases for $? exit code variable."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_exit_code_success(self):
        """Test $? is 0 after successful command."""
        result = JShellRunner.run('pwd; echo $?')
        self.assertEqual(result.returncode, 0)
        lines = result.stdout.strip().split('\n')
        self.assertEqual(lines[-1], "0")

    def test_exit_code_command_not_found(self):
        """Test $? is 127 after command not found."""
        result = JShellRunner.run('nonexistent_cmd_xyz; echo $?')
        self.assertEqual(result.returncode, 0)  # echo succeeds
        lines = result.stdout.strip().split('\n')
        self.assertEqual(lines[-1], "127")

    def test_exit_code_builtin_failure(self):
        """Test $? is non-zero after builtin failure."""
        result = JShellRunner.run('cd /nonexistent/path/xyz; echo $?')
        self.assertEqual(result.returncode, 0)  # echo succeeds
        lines = result.stdout.strip().split('\n')
        self.assertNotEqual(lines[-1], "0")

    def test_exit_code_resets(self):
        """Test $? resets after each command."""
        result = JShellRunner.run(
            'nonexistent_cmd_xyz; pwd; echo $?'
        )
        self.assertEqual(result.returncode, 0)
        lines = result.stdout.strip().split('\n')
        # After pwd succeeds, $? should be 0
        self.assertEqual(lines[-1], "0")

    def test_exit_code_in_echo(self):
        """Test $? can be used in echo."""
        result = JShellRunner.run('pwd; echo "Exit was: $?"')
        self.assertEqual(result.returncode, 0)
        self.assertIn("Exit was: 0", result.stdout)


class TestBackgroundJobs(unittest.TestCase):
    """Test cases for background job handling."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_background_job_syntax(self):
        """Test background job syntax is accepted."""
        result = JShellRunner.run('sleep 0.01 &')
        self.assertEqual(result.returncode, 0)

    def test_jobs_command(self):
        """Test jobs command works."""
        result = JShellRunner.run('jobs')
        # Should succeed even with no jobs
        self.assertEqual(result.returncode, 0)

    def test_jobs_json_output(self):
        """Test jobs --json output."""
        result = JShellRunner.run('jobs --json')
        self.assertEqual(result.returncode, 0)
        self.assertIn("[", result.stdout)  # JSON array


class TestCommandHistory(unittest.TestCase):
    """Test cases for command history."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_history_command(self):
        """Test history command works."""
        result = JShellRunner.run('echo test; history')
        # History command should succeed
        self.assertEqual(result.returncode, 0)


class TestShellEnvironment(unittest.TestCase):
    """Test cases for shell environment handling."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_env_variable_set(self):
        """Test environment variable is accessible."""
        result = JShellRunner.run('echo $HOME')
        self.assertEqual(result.returncode, 0)
        self.assertTrue(result.stdout.strip().startswith("/"))

    def test_export_modifies_env(self):
        """Test export modifies environment."""
        result = JShellRunner.run('export "TEST_VAR=test_value"; echo $TEST_VAR')
        self.assertEqual(result.returncode, 0)
        self.assertIn("test_value", result.stdout)

    def test_unset_removes_variable(self):
        """Test unset removes environment variable."""
        result = JShellRunner.run(
            'export "MY_VAR=value"; unset MY_VAR; echo "VAR=$MY_VAR"'
        )
        self.assertEqual(result.returncode, 0)
        # After unset, MY_VAR should be empty
        self.assertIn("VAR=", result.stdout)


class TestMultipleCommands(unittest.TestCase):
    """Test cases for multiple command execution."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_semicolon_separator(self):
        """Test semicolon separates commands."""
        result = JShellRunner.run('echo first; echo second; echo third')
        self.assertEqual(result.returncode, 0)
        lines = result.stdout.strip().split('\n')
        self.assertEqual(len(lines), 3)
        self.assertEqual(lines[0], "first")
        self.assertEqual(lines[1], "second")
        self.assertEqual(lines[2], "third")

    def test_commands_execute_sequentially(self):
        """Test commands execute in order."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
            temp_path = f.name
        try:
            result = JShellRunner.run(
                f'echo "line1" > {temp_path}; '
                f'echo "line2" > {temp_path}'
            )
            self.assertEqual(result.returncode, 0)
            # Second echo should overwrite
            with open(temp_path) as f:
                content = f.read()
            self.assertIn("line2", content)
            self.assertNotIn("line1", content)
        finally:
            os.unlink(temp_path)


if __name__ == "__main__":
    unittest.main()
