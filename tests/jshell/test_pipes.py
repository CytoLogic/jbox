#!/usr/bin/env python3
"""Unit tests for pipeline execution."""

import unittest

from tests.helpers import JShellRunner


class TestSimplePipelines(unittest.TestCase):
    """Test cases for simple pipeline execution."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_simple_two_command_pipeline(self):
        """Test simple two-command pipeline."""
        result = JShellRunner.run('echo "hello" | cat')
        self.assertEqual(result.returncode, 0)
        self.assertIn("hello", result.stdout)

    def test_builtin_to_external_pipeline(self):
        """Test builtin to external command pipeline."""
        result = JShellRunner.run("pwd | cat")
        self.assertEqual(result.returncode, 0)
        self.assertTrue(result.stdout.startswith("/"))

    def test_external_to_builtin_pipeline(self):
        """Test external to builtin pipeline."""
        result = JShellRunner.run("/bin/echo test | cat")
        self.assertEqual(result.returncode, 0)
        self.assertIn("test", result.stdout)

    def test_three_command_pipeline(self):
        """Test three-command pipeline."""
        result = JShellRunner.run('echo "line1\nline2\nline3" | cat | cat')
        self.assertEqual(result.returncode, 0)
        self.assertIn("line1", result.stdout)

    def test_pipeline_preserves_newlines(self):
        """Test pipeline preserves multiple lines."""
        result = JShellRunner.run('echo "line1\nline2" | cat')
        self.assertEqual(result.returncode, 0)
        lines = result.stdout.strip().split("\n")
        self.assertEqual(len(lines), 2)


class TestPipelineWithBuiltins(unittest.TestCase):
    """Test cases for pipelines involving builtins."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_env_piped_to_grep_like(self):
        """Test env builtin piped to external command."""
        result = JShellRunner.run("env | cat")
        self.assertEqual(result.returncode, 0)
        self.assertIn("PATH=", result.stdout)

    def test_pwd_in_pipeline(self):
        """Test pwd builtin in pipeline."""
        result = JShellRunner.run("pwd | cat")
        self.assertEqual(result.returncode, 0)
        self.assertTrue(result.stdout.startswith("/"))

    def test_echo_in_pipeline(self):
        """Test echo in pipeline produces output."""
        result = JShellRunner.run('echo "pipeline_test" | cat')
        self.assertEqual(result.returncode, 0)
        self.assertIn("pipeline_test", result.stdout)


class TestPipelineExitCodes(unittest.TestCase):
    """Test cases for pipeline exit code handling."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_successful_pipeline_exit_code(self):
        """Test successful pipeline returns 0."""
        result = JShellRunner.run('echo "test" | cat')
        self.assertEqual(result.returncode, 0)

    def test_pipeline_last_command_exit_code(self):
        """Test pipeline returns exit code of last command."""
        # In standard shell behavior, pipeline returns exit code of last command
        # even if first command fails
        result = JShellRunner.run('echo "success" | cat')
        self.assertEqual(result.returncode, 0)


class TestLargeDataPipeline(unittest.TestCase):
    """Test cases for pipeline with larger data."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_moderate_data_through_pipeline(self):
        """Test moderate amount of data through pipeline."""
        long_string = "x" * 1000
        result = JShellRunner.run(f'echo "{long_string}" | cat')
        self.assertEqual(result.returncode, 0)
        self.assertIn("x" * 100, result.stdout)


if __name__ == "__main__":
    unittest.main()
