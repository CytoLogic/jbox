"""Helper module for signal testing in jbox."""

import os
import signal
import subprocess
import threading
import time
from pathlib import Path
from typing import Optional


class SignalTestHelper:
    """Helper for testing signal handling in processes."""

    BIN_DIR = Path(__file__).parent.parent.parent / "bin" / "standalone-apps"
    JSHELL_BIN = Path(__file__).parent.parent.parent / "bin" / "jshell"

    @staticmethod
    def send_signal_after_delay(pid: int, sig: int, delay_ms: int) -> None:
        """Send signal to process after delay.

        Args:
            pid: Process ID to send signal to
            sig: Signal number to send (e.g., signal.SIGINT)
            delay_ms: Delay in milliseconds before sending signal
        """
        def send():
            time.sleep(delay_ms / 1000.0)
            try:
                os.kill(pid, sig)
            except ProcessLookupError:
                pass  # Process already exited

        thread = threading.Thread(target=send, daemon=True)
        thread.start()

    @classmethod
    def run_with_signal(cls, cmd: list[str], sig: int, delay_ms: int,
                        timeout: float = 5.0,
                        stdin_data: Optional[str] = None,
                        cwd: Optional[str] = None,
                        env: Optional[dict] = None) -> subprocess.CompletedProcess:
        """Run command and send signal after delay.

        Args:
            cmd: Command and arguments to run
            sig: Signal number to send
            delay_ms: Delay in milliseconds before sending signal
            timeout: Maximum time to wait for process (seconds)
            stdin_data: Optional data to send to stdin
            cwd: Optional working directory
            env: Optional environment variables

        Returns:
            CompletedProcess with returncode, stdout, stderr
        """
        run_env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        if env:
            run_env.update(env)

        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE if stdin_data else None,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=cwd,
            env=run_env
        )

        # Schedule signal
        cls.send_signal_after_delay(proc.pid, sig, delay_ms)

        try:
            stdout, stderr = proc.communicate(
                input=stdin_data,
                timeout=timeout
            )
        except subprocess.TimeoutExpired:
            proc.kill()
            stdout, stderr = proc.communicate()

        return subprocess.CompletedProcess(
            args=cmd,
            returncode=proc.returncode,
            stdout=stdout,
            stderr=stderr
        )

    @classmethod
    def run_jshell_with_signal(cls, command: str, sig: int, delay_ms: int,
                               timeout: float = 5.0,
                               interactive: bool = False,
                               stdin_data: Optional[str] = None
                               ) -> subprocess.CompletedProcess:
        """Run jshell command and send signal after delay.

        Args:
            command: Command to run via jshell -c
            sig: Signal number to send
            delay_ms: Delay in milliseconds before sending signal
            timeout: Maximum time to wait (seconds)
            interactive: If True, start jshell without -c flag
            stdin_data: Optional data to send to stdin

        Returns:
            CompletedProcess with returncode, stdout, stderr
        """
        jshell = str(cls.JSHELL_BIN)

        if interactive:
            cmd = [jshell]
        else:
            cmd = [jshell, "-c", command]

        return cls.run_with_signal(
            cmd, sig, delay_ms,
            timeout=timeout,
            stdin_data=stdin_data
        )

    @classmethod
    def run_app_with_signal(cls, app: str, args: list[str], sig: int,
                            delay_ms: int, timeout: float = 5.0,
                            stdin_data: Optional[str] = None,
                            cwd: Optional[str] = None
                            ) -> subprocess.CompletedProcess:
        """Run a jbox app binary with signal after delay.

        Args:
            app: App name (binary in bin directory)
            args: Arguments to pass to app
            sig: Signal number to send
            delay_ms: Delay in milliseconds before sending signal
            timeout: Maximum time to wait (seconds)
            stdin_data: Optional data to send to stdin
            cwd: Optional working directory

        Returns:
            CompletedProcess with returncode, stdout, stderr
        """
        app_path = str(cls.BIN_DIR / app)
        cmd = [app_path] + args

        return cls.run_with_signal(
            cmd, sig, delay_ms,
            timeout=timeout,
            stdin_data=stdin_data,
            cwd=cwd
        )

    @staticmethod
    def wait_for_output(proc: subprocess.Popen, pattern: str,
                        timeout: float = 5.0) -> bool:
        """Wait for process to output a pattern.

        Args:
            proc: Running process with stdout pipe
            pattern: String pattern to wait for
            timeout: Maximum time to wait (seconds)

        Returns:
            True if pattern was found, False if timeout
        """
        start = time.time()
        output = ""

        while time.time() - start < timeout:
            line = proc.stdout.readline()
            if line:
                output += line
                if pattern in output:
                    return True
            else:
                time.sleep(0.01)

        return False

    @classmethod
    def start_interactive_app(cls, app: str, args: list[str],
                              cwd: Optional[str] = None,
                              env: Optional[dict] = None
                              ) -> subprocess.Popen:
        """Start an interactive app (vi, less) for signal testing.

        Args:
            app: App name (binary in bin directory)
            args: Arguments to pass to app
            cwd: Optional working directory
            env: Optional environment variables

        Returns:
            Popen object for the running process
        """
        run_env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        # Set TERM for interactive apps
        run_env["TERM"] = "xterm-256color"
        if env:
            run_env.update(env)

        app_path = str(cls.BIN_DIR / app)
        cmd = [app_path] + args

        return subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=cwd,
            env=run_env
        )

    @staticmethod
    def is_process_running(pid: int) -> bool:
        """Check if a process is still running.

        Args:
            pid: Process ID to check

        Returns:
            True if process is running, False otherwise
        """
        try:
            os.kill(pid, 0)
            return True
        except ProcessLookupError:
            return False
        except PermissionError:
            return True  # Process exists but we can't signal it
