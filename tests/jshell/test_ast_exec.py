#!/usr/bin/env python3
"""Unit tests for AST execution in jshell."""

import os
import tempfile
import unittest

from tests.helpers import JShellRunner


class TestSimpleCommandExecution(unittest.TestCase):
    """Test cases for simple command execution."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_simple_command(self):
        """Test simple command execution."""
        result = JShellRunner.run("pwd")
        self.assertEqual(result.returncode, 0)
        self.assertTrue(result.stdout.startswith("/"))

    def test_command_with_argument(self):
        """Test command with single argument."""
        result = JShellRunner.run('echo hello')
        self.assertEqual(result.returncode, 0)
        self.assertIn("hello", result.stdout)

    def test_command_with_multiple_arguments(self):
        """Test command with multiple arguments."""
        result = JShellRunner.run('echo one two three')
        self.assertEqual(result.returncode, 0)
        self.assertIn("one", result.stdout)
        self.assertIn("two", result.stdout)
        self.assertIn("three", result.stdout)

    def test_builtin_command(self):
        """Test builtin command execution."""
        result = JShellRunner.run("pwd")
        self.assertEqual(result.returncode, 0)
        self.assertTrue(len(result.stdout.strip()) > 0)

    def test_external_command(self):
        """Test external command execution."""
        result = JShellRunner.run("/bin/echo test")
        self.assertEqual(result.returncode, 0)
        self.assertIn("test", result.stdout)


class TestQuotedArguments(unittest.TestCase):
    """Test cases for quoted arguments."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_double_quoted_string(self):
        """Test double-quoted string argument."""
        result = JShellRunner.run('echo "hello world"')
        self.assertEqual(result.returncode, 0)
        self.assertIn("hello world", result.stdout)

    def test_single_quoted_string(self):
        """Test single-quoted string argument."""
        result = JShellRunner.run("echo 'hello world'")
        self.assertEqual(result.returncode, 0)
        self.assertIn("hello world", result.stdout)

    def test_mixed_quotes(self):
        """Test mixed quote styles."""
        result = JShellRunner.run("echo 'single' \"double\"")
        self.assertEqual(result.returncode, 0)
        self.assertIn("single", result.stdout)
        self.assertIn("double", result.stdout)

    def test_quoted_special_chars(self):
        """Test quoted special characters."""
        result = JShellRunner.run('echo "hello | world"')
        self.assertEqual(result.returncode, 0)
        self.assertIn("hello | world", result.stdout)


class TestVariableExpansion(unittest.TestCase):
    """Test cases for variable expansion."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_env_variable_expansion(self):
        """Test expansion of environment variable."""
        result = JShellRunner.run('echo $HOME')
        self.assertEqual(result.returncode, 0)
        self.assertTrue(result.stdout.strip().startswith("/"))

    def test_path_variable_expansion(self):
        """Test expansion of PATH variable."""
        result = JShellRunner.run('echo $PATH')
        self.assertEqual(result.returncode, 0)
        self.assertIn("/", result.stdout)

    def test_undefined_variable_expansion(self):
        """Test expansion of undefined variable (should be empty)."""
        result = JShellRunner.run('echo $UNDEFINED_VAR_12345')
        self.assertEqual(result.returncode, 0)
        # Empty variable expands to nothing
        self.assertEqual(result.stdout.strip(), "")

    def test_variable_in_double_quotes(self):
        """Test variable expansion in double quotes."""
        result = JShellRunner.run('echo "HOME=$HOME"')
        self.assertEqual(result.returncode, 0)
        self.assertIn("HOME=/", result.stdout)


class TestTildeExpansion(unittest.TestCase):
    """Test cases for tilde expansion."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_tilde_expansion(self):
        """Test tilde expands to home directory."""
        result = JShellRunner.run('echo ~')
        self.assertEqual(result.returncode, 0)
        self.assertTrue(result.stdout.strip().startswith("/"))

    def test_tilde_with_path(self):
        """Test tilde expansion with path component."""
        result = JShellRunner.run('echo ~/.jshell')
        self.assertEqual(result.returncode, 0)
        self.assertTrue(result.stdout.strip().endswith(".jshell"))


class TestGlobExpansion(unittest.TestCase):
    """Test cases for glob expansion."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")
        # Create a temp directory with test files
        cls.temp_dir = tempfile.mkdtemp()
        for name in ["test1.txt", "test2.txt", "other.log"]:
            open(os.path.join(cls.temp_dir, name), "w").close()

    @classmethod
    def tearDownClass(cls):
        """Clean up temp directory."""
        import shutil
        shutil.rmtree(cls.temp_dir, ignore_errors=True)

    def test_star_glob(self):
        """Test * glob pattern."""
        result = JShellRunner.run(f'ls {self.temp_dir}/*.txt')
        self.assertEqual(result.returncode, 0)
        self.assertIn("test1.txt", result.stdout)
        self.assertIn("test2.txt", result.stdout)

    def test_question_glob(self):
        """Test ? glob pattern."""
        result = JShellRunner.run(f'ls {self.temp_dir}/test?.txt')
        self.assertEqual(result.returncode, 0)
        self.assertIn("test1.txt", result.stdout)
        self.assertIn("test2.txt", result.stdout)


class TestInputRedirection(unittest.TestCase):
    """Test cases for input redirection."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")
        # Create a temp file with test content
        cls.temp_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
        cls.temp_file.write("line1\nline2\nline3\n")
        cls.temp_file.close()

    @classmethod
    def tearDownClass(cls):
        """Clean up temp file."""
        os.unlink(cls.temp_file.name)

    def test_input_redirection(self):
        """Test input redirection from file."""
        # Use /bin/cat since the builtin cat requires file arguments
        result = JShellRunner.run(f'/bin/cat < {self.temp_file.name}')
        self.assertEqual(result.returncode, 0)
        self.assertIn("line1", result.stdout)
        self.assertIn("line2", result.stdout)

    def test_input_redirection_nonexistent(self):
        """Test input redirection from nonexistent file."""
        result = JShellRunner.run('/bin/cat < /nonexistent/file/path')
        # Command should fail
        self.assertNotEqual(result.returncode, 0)


class TestOutputRedirection(unittest.TestCase):
    """Test cases for output redirection."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_output_redirection(self):
        """Test output redirection to file."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
            temp_path = f.name
        try:
            result = JShellRunner.run(f'echo "test output" > {temp_path}')
            self.assertEqual(result.returncode, 0)
            with open(temp_path) as f:
                content = f.read()
            self.assertIn("test output", content)
        finally:
            os.unlink(temp_path)

    def test_output_redirection_overwrites(self):
        """Test output redirection overwrites existing file."""
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
            f.write("original content")
            temp_path = f.name
        try:
            result = JShellRunner.run(f'echo "new content" > {temp_path}')
            self.assertEqual(result.returncode, 0)
            with open(temp_path) as f:
                content = f.read()
            self.assertIn("new content", content)
            self.assertNotIn("original", content)
        finally:
            os.unlink(temp_path)

    def test_output_redirection_creates_file(self):
        """Test output redirection creates new file."""
        temp_path = tempfile.mktemp()
        try:
            result = JShellRunner.run(f'echo "created" > {temp_path}')
            self.assertEqual(result.returncode, 0)
            self.assertTrue(os.path.exists(temp_path))
            with open(temp_path) as f:
                content = f.read()
            self.assertIn("created", content)
        finally:
            if os.path.exists(temp_path):
                os.unlink(temp_path)


class TestBackgroundJobs(unittest.TestCase):
    """Test cases for background job execution."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_background_job_syntax(self):
        """Test background job syntax is recognized."""
        # Run a quick background job, then exit
        result = JShellRunner.run('sleep 0.01 &')
        # Should complete without error
        self.assertEqual(result.returncode, 0)


class TestVariableAssignment(unittest.TestCase):
    """Test cases for variable assignment."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_command_substitution_assignment(self):
        """Test variable assignment with command substitution."""
        result = JShellRunner.run('MYVAR=pwd; echo $MYVAR')
        self.assertEqual(result.returncode, 0)
        # MYVAR should contain the pwd output
        self.assertTrue(result.stdout.strip().startswith("/"))


class TestExitCodes(unittest.TestCase):
    """Test cases for exit code handling."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_successful_command_exit_code(self):
        """Test successful command returns 0."""
        result = JShellRunner.run("pwd")
        self.assertEqual(result.returncode, 0)

    def test_command_not_found_exit_code(self):
        """Test command not found returns 127."""
        result = JShellRunner.run("nonexistent_command_12345")
        self.assertEqual(result.returncode, 127)

    def test_builtin_error_exit_code(self):
        """Test builtin error returns non-zero."""
        result = JShellRunner.run("cd /nonexistent/path/12345")
        self.assertNotEqual(result.returncode, 0)


class TestErrorMessages(unittest.TestCase):
    """Test cases for error message formatting."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_command_not_found_message(self):
        """Test command not found error message."""
        result = JShellRunner.run("nonexistent_cmd_xyz")
        self.assertIn("command not found", result.stderr.lower())

    def test_file_not_found_message(self):
        """Test file not found error message."""
        result = JShellRunner.run("cat /nonexistent/file/path/xyz")
        # stderr should contain some error message
        self.assertTrue(len(result.stderr) > 0 or result.returncode != 0)


class TestMultipleCommands(unittest.TestCase):
    """Test cases for multiple command execution."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_semicolon_separated_commands(self):
        """Test semicolon-separated commands execute in sequence."""
        result = JShellRunner.run('echo first; echo second')
        self.assertEqual(result.returncode, 0)
        self.assertIn("first", result.stdout)
        self.assertIn("second", result.stdout)

    def test_multiple_commands_order(self):
        """Test multiple commands execute in order."""
        result = JShellRunner.run('echo 1; echo 2; echo 3')
        self.assertEqual(result.returncode, 0)
        lines = result.stdout.strip().split('\n')
        self.assertEqual(len(lines), 3)
        self.assertEqual(lines[0].strip(), "1")
        self.assertEqual(lines[1].strip(), "2")
        self.assertEqual(lines[2].strip(), "3")


if __name__ == "__main__":
    unittest.main()
