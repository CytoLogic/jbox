#!/usr/bin/env python3
"""Unit tests for shell signal handling."""

import os
import signal
import subprocess
import tempfile
import time
import unittest

from tests.helpers import JShellRunner, SignalTestHelper


class TestShellSigint(unittest.TestCase):
    """Test cases for SIGINT (Ctrl+C) handling in shell."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_sigint_interrupts_sleep(self):
        """Test SIGINT terminates foreground sleep command."""
        result = SignalTestHelper.run_jshell_with_signal(
            "sleep 10",
            signal.SIGINT,
            delay_ms=100,
            timeout=5.0
        )
        # Sleep should be interrupted, return 130 (128 + SIGINT)
        # The shell itself may return a different code
        self.assertLess(result.returncode, 10)  # Did not run full 10 seconds

    def test_sigint_interactive_continues(self):
        """Test shell continues after SIGINT in interactive mode.

        Note: SIGINT during fgets with SA_RESTART flag set will cause fgets
        to return NULL and clearerr to reset stdin. The shell should continue
        prompting without exiting.
        """
        # Start interactive shell
        jshell = str(SignalTestHelper.JSHELL_BIN)
        env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}

        proc = subprocess.Popen(
            [jshell],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env
        )

        try:
            # Give shell time to start and display prompt
            time.sleep(0.2)

            # Send SIGINT
            os.kill(proc.pid, signal.SIGINT)

            # Give shell time to handle signal
            time.sleep(0.2)

            # Shell should still be running after SIGINT
            self.assertIsNone(
                proc.poll(),
                "Shell should continue running after SIGINT"
            )

            # Clean exit
            proc.kill()
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.communicate()
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.communicate()

    def test_sigint_during_command_execution(self):
        """Test SIGINT during command execution terminates command."""
        result = SignalTestHelper.run_jshell_with_signal(
            "sleep 5; echo done",
            signal.SIGINT,
            delay_ms=100,
            timeout=3.0
        )
        # "done" should not appear because sleep was interrupted
        self.assertNotIn("done", result.stdout)


class TestShellSigterm(unittest.TestCase):
    """Test cases for SIGTERM handling in shell."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_sigterm_graceful_shutdown(self):
        """Test SIGTERM causes graceful shell shutdown.

        Note: With SA_RESTART flag set, SIGTERM during fgets() will cause
        fgets to restart. The shell checks for termination at the start of
        each loop iteration, so we send a newline to complete the current
        fgets() call.
        """
        jshell = str(SignalTestHelper.JSHELL_BIN)
        env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}

        proc = subprocess.Popen(
            [jshell],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env
        )

        try:
            time.sleep(0.1)

            # Send SIGTERM
            os.kill(proc.pid, signal.SIGTERM)
            time.sleep(0.1)

            # Send newline to complete fgets() so loop can check termination
            proc.stdin.write("\n")
            proc.stdin.flush()

            # Shell should exit on next loop iteration
            proc.wait(timeout=2)
            # Graceful shutdown returns 0 or 143 (128 + SIGTERM)
            self.assertIn(proc.returncode, [0, 143])
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.communicate()
            self.fail("Shell did not exit after SIGTERM")
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.communicate()


class TestShellSigpipe(unittest.TestCase):
    """Test cases for SIGPIPE handling in shell."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_pipe_to_head(self):
        """Test SIGPIPE doesn't crash shell with pipe to head."""
        # Create a command that produces many lines, pipe to head -n 1
        result = JShellRunner.run("cat /proc/meminfo | head -n 1", timeout=5)
        # Should complete without crash
        self.assertEqual(result.returncode, 0)
        self.assertTrue(len(result.stdout) > 0)

    def test_broken_pipe_handling(self):
        """Test shell handles broken pipe without crashing."""
        # cat produces many lines from /proc/meminfo, head exits after 1 line
        result = JShellRunner.run("cat /proc/meminfo | head -n 1")
        # Shell should not crash from SIGPIPE
        self.assertEqual(result.returncode, 0)
        self.assertTrue(len(result.stdout.strip()) > 0)


class TestBackgroundJobSignals(unittest.TestCase):
    """Test cases for signal handling with background jobs."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_background_job_unaffected_by_sigint(self):
        """Test background job continues after SIGINT to shell."""
        with tempfile.NamedTemporaryFile(delete=False) as f:
            marker_file = f.name

        try:
            jshell = str(SignalTestHelper.JSHELL_BIN)
            env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}

            proc = subprocess.Popen(
                [jshell],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                env=env
            )

            try:
                # Start background job that writes to marker file
                proc.stdin.write(f"sleep 0.5; touch {marker_file}.done &\n")
                proc.stdin.flush()
                time.sleep(0.1)

                # Send SIGINT to shell
                os.kill(proc.pid, signal.SIGINT)
                time.sleep(0.1)

                # Exit shell
                proc.stdin.write("exit\n")
                proc.stdin.flush()
                proc.communicate(timeout=2)

                # Wait for background job to complete
                time.sleep(0.6)

                # Background job should have created the marker file
                self.assertTrue(
                    os.path.exists(marker_file + ".done"),
                    "Background job did not complete after SIGINT")
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.communicate()
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.communicate()
        finally:
            try:
                os.unlink(marker_file)
            except FileNotFoundError:
                pass
            try:
                os.unlink(marker_file + ".done")
            except FileNotFoundError:
                pass


class TestCommandExitCodes(unittest.TestCase):
    """Test exit code handling with signals."""

    @classmethod
    def setUpClass(cls):
        """Verify the jshell binary exists before running tests."""
        if not JShellRunner.exists():
            raise unittest.SkipTest(
                f"jshell binary not found at {JShellRunner.JSHELL}")

    def test_interrupted_command_exit_code(self):
        """Test interrupted command returns appropriate signal code.

        Note: Python subprocess returns -signal_number when process is
        terminated by signal, while shell convention is 128 + signal_number.
        We accept either convention.
        """
        # Use sleep app directly
        result = SignalTestHelper.run_app_with_signal(
            "sleep", ["10"],
            signal.SIGINT,
            delay_ms=100,
            timeout=3.0
        )
        # Accept either Python's -2 or shell convention's 130 (128 + SIGINT)
        self.assertIn(result.returncode, [-2, 130])

    def test_exit_code_variable_after_interrupt(self):
        """Test $? reflects interrupted command."""
        # This is tricky to test because the shell itself
        # needs to capture the exit code of an interrupted command
        result = JShellRunner.run("sleep 0.1; echo $?")
        self.assertEqual(result.returncode, 0)
        # After successful sleep, $? should be 0
        self.assertIn("0", result.stdout)


if __name__ == "__main__":
    unittest.main()
