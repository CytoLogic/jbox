"""Helper module for running jshell commands in tests."""

import json
import os
import re
import subprocess
from pathlib import Path
from typing import Optional


class JShellRunner:
    """Helper for running jshell commands in tests."""

    JSHELL = Path(__file__).parent.parent.parent / "bin" / "jshell"

    @classmethod
    def run(cls, command: str, env: Optional[dict] = None,
            cwd: Optional[str] = None,
            timeout: Optional[int] = None) -> subprocess.CompletedProcess:
        """Run a command via jshell -c and return result.

        Args:
            command: The command string to execute
            env: Optional additional environment variables
            cwd: Optional working directory
            timeout: Optional timeout in seconds

        Returns:
            CompletedProcess with stdout/stderr cleaned of debug output
        """
        run_env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        if env:
            run_env.update(env)

        result = subprocess.run(
            [str(cls.JSHELL), "-c", command],
            capture_output=True,
            text=True,
            env=run_env,
            cwd=cwd,
            timeout=timeout
        )

        result.stdout = cls._clean_output(result.stdout)
        result.stderr = cls._clean_output(result.stderr)

        return result

    @classmethod
    def run_json(cls, command: str, env: Optional[dict] = None,
                 cwd: Optional[str] = None,
                 timeout: Optional[int] = None) -> dict:
        """Run a command and parse JSON output.

        Args:
            command: The command string to execute (should produce JSON)
            env: Optional additional environment variables
            cwd: Optional working directory
            timeout: Optional timeout in seconds

        Returns:
            Parsed JSON output as a dictionary

        Raises:
            json.JSONDecodeError: If output is not valid JSON
        """
        result = cls.run(command, env=env, cwd=cwd, timeout=timeout)
        return json.loads(result.stdout)

    @classmethod
    def run_multi(cls, commands: list[str], env: Optional[dict] = None,
                  cwd: Optional[str] = None) -> subprocess.CompletedProcess:
        """Run multiple commands (semicolon-separated).

        Args:
            commands: List of commands to execute
            env: Optional additional environment variables
            cwd: Optional working directory

        Returns:
            CompletedProcess with stdout/stderr cleaned of debug output
        """
        command = "; ".join(commands)
        return cls.run(command, env=env, cwd=cwd)

    @staticmethod
    def _clean_output(output: str) -> str:
        """Remove debug output and other noise from command output."""
        output = re.sub(r'\[DEBUG\]:.*\n', '', output)
        output = output.strip()
        return output

    @classmethod
    def exists(cls) -> bool:
        """Check if jshell binary exists."""
        return cls.JSHELL.exists()
