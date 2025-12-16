#!/usr/bin/env python3
"""Unit tests for the FTP client (ftp command)."""

import json
import os
import shutil
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


class FtpClientTestCase(unittest.TestCase):
    """Test cases for the FTP client command."""

    FTP_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "standalone-apps" / "ftp"
    FTPD_BIN = Path(__file__).parent.parent.parent.parent / "bin" / "ftpd"
    SERVER_PORT = 21621  # Use unique port for client tests
    server_proc = None
    test_root = None
    test_local = None

    @classmethod
    def setUpClass(cls):
        """Start the FTP server with a temporary root directory."""
        if not cls.FTP_BIN.exists():
            raise unittest.SkipTest(f"ftp client not found at {cls.FTP_BIN}")
        if not cls.FTPD_BIN.exists():
            raise unittest.SkipTest(f"ftpd server not found at {cls.FTPD_BIN}")

        # Create temporary FTP root
        cls.test_root = tempfile.mkdtemp(prefix="ftp_client_server_")

        # Create temporary local directory for client tests
        cls.test_local = tempfile.mkdtemp(prefix="ftp_client_local_")

        # Create test files on server
        (Path(cls.test_root) / "serverfile.txt").write_text("Server content\n")
        (Path(cls.test_root) / "subdir").mkdir()
        (Path(cls.test_root) / "subdir" / "nested.txt").write_text("Nested\n")

        # Start server
        cls.server_proc = subprocess.Popen(
            [str(cls.FTPD_BIN), "-p", str(cls.SERVER_PORT), "-r", cls.test_root],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )

        # Wait for server to start
        time.sleep(0.3)

        # Check if server started successfully
        if cls.server_proc.poll() is not None:
            stdout, stderr = cls.server_proc.communicate()
            raise unittest.SkipTest(
                f"ftpd failed to start: {stderr.decode()}"
            )

    @classmethod
    def tearDownClass(cls):
        """Stop the FTP server and clean up."""
        if cls.server_proc:
            cls.server_proc.terminate()
            try:
                cls.server_proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                cls.server_proc.kill()
                cls.server_proc.wait()

        # Clean up directories
        if cls.test_root:
            shutil.rmtree(cls.test_root, ignore_errors=True)
        if cls.test_local:
            shutil.rmtree(cls.test_local, ignore_errors=True)

    def run_ftp(self, *args, input_data=None, timeout=10):
        """Run the FTP client with given arguments."""
        cmd = [str(self.FTP_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            input=input_data,
            capture_output=True,
            text=True,
            timeout=timeout,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def run_ftp_interactive(self, commands, json_output=False):
        """Run the FTP client with interactive commands."""
        args = ["-H", "localhost", "-p", str(self.SERVER_PORT)]
        if json_output:
            args.append("--json")
        # Add commands and quit
        input_data = "\n".join(commands) + "\nquit\n"
        return self.run_ftp(*args, input_data=input_data)

    # === Help tests ===

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_ftp("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: ftp", result.stdout)
        self.assertIn("--help", result.stdout)
        self.assertIn("--json", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_ftp("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: ftp", result.stdout)
        self.assertIn("host", result.stdout.lower())

    # === Connection tests ===

    def test_connect_default_host(self):
        """Test connecting to localhost (default)."""
        result = self.run_ftp_interactive(["pwd"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Connected", result.stdout)
        self.assertIn("Logged in", result.stdout)

    def test_connect_explicit_host(self):
        """Test connecting with explicit host argument."""
        result = self.run_ftp_interactive(["pwd"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Connected", result.stdout)

    def test_connect_json_output(self):
        """Test connection with JSON output."""
        result = self.run_ftp_interactive(["pwd"], json_output=True)
        self.assertEqual(result.returncode, 0)
        # Check JSON output in stdout
        self.assertIn('"action":"connect"', result.stdout)
        self.assertIn('"status":"ok"', result.stdout)
        self.assertIn('"action":"login"', result.stdout)

    def test_connect_bad_port(self):
        """Test connecting to invalid port fails gracefully."""
        result = self.run_ftp("-H", "localhost", "-p", "99999", input_data="quit\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("invalid port", result.stderr.lower())

    # === Interactive command tests ===

    def test_pwd_command(self):
        """Test pwd command shows working directory."""
        result = self.run_ftp_interactive(["pwd"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("/", result.stdout)

    def test_pwd_json(self):
        """Test pwd command with JSON output."""
        result = self.run_ftp_interactive(["pwd"], json_output=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('"action":"pwd"', result.stdout)
        self.assertIn('"status":"ok"', result.stdout)
        self.assertIn('"path":', result.stdout)

    def test_ls_command(self):
        """Test ls command lists directory contents."""
        result = self.run_ftp_interactive(["ls"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("serverfile.txt", result.stdout)
        self.assertIn("subdir", result.stdout)

    def test_ls_json(self):
        """Test ls command with JSON output."""
        result = self.run_ftp_interactive(["ls"], json_output=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('"action":"ls"', result.stdout)
        self.assertIn('"status":"ok"', result.stdout)
        self.assertIn("serverfile.txt", result.stdout)

    def test_ls_subdir(self):
        """Test ls command with subdirectory path."""
        result = self.run_ftp_interactive(["ls subdir"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("nested.txt", result.stdout)

    def test_cd_command(self):
        """Test cd command changes directory."""
        result = self.run_ftp_interactive(["cd subdir", "pwd"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("subdir", result.stdout)

    def test_cd_json(self):
        """Test cd command with JSON output."""
        result = self.run_ftp_interactive(["cd subdir"], json_output=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('"action":"cd"', result.stdout)
        self.assertIn('"status":"ok"', result.stdout)

    def test_cd_missing_arg(self):
        """Test cd command without argument shows error."""
        result = self.run_ftp_interactive(["cd"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("missing path", result.stderr.lower())

    def test_help_interactive(self):
        """Test help command in interactive mode."""
        result = self.run_ftp_interactive(["help"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("ls", result.stdout)
        self.assertIn("cd", result.stdout)
        self.assertIn("get", result.stdout)
        self.assertIn("put", result.stdout)

    def test_help_interactive_json(self):
        """Test help command with JSON output."""
        result = self.run_ftp_interactive(["help"], json_output=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('"commands":', result.stdout)

    # === File transfer tests ===

    def test_get_command(self):
        """Test get command downloads file."""
        # Change to local directory first
        local_file = Path(self.test_local) / "downloaded.txt"
        result = self.run_ftp_interactive(
            [f"get serverfile.txt {local_file}"],
            json_output=False
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("Downloaded", result.stdout)
        self.assertTrue(local_file.exists())
        self.assertEqual(local_file.read_text(), "Server content\n")

    def test_get_json(self):
        """Test get command with JSON output."""
        local_file = Path(self.test_local) / "downloaded_json.txt"
        result = self.run_ftp_interactive(
            [f"get serverfile.txt {local_file}"],
            json_output=True
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn('"action":"get"', result.stdout)
        self.assertIn('"status":"ok"', result.stdout)

    def test_get_missing_arg(self):
        """Test get command without argument shows error."""
        result = self.run_ftp_interactive(["get"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr.lower())

    def test_put_command(self):
        """Test put command uploads file."""
        # Create local file to upload
        local_file = Path(self.test_local) / "upload_test.txt"
        local_file.write_text("Upload content\n")
        result = self.run_ftp_interactive(
            [f"put {local_file} uploaded.txt"],
            json_output=False
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("Uploaded", result.stdout)
        # Verify file on server
        server_file = Path(self.test_root) / "uploaded.txt"
        self.assertTrue(server_file.exists())
        self.assertEqual(server_file.read_text(), "Upload content\n")

    def test_put_json(self):
        """Test put command with JSON output."""
        local_file = Path(self.test_local) / "upload_json.txt"
        local_file.write_text("JSON upload\n")
        result = self.run_ftp_interactive(
            [f"put {local_file} uploaded_json.txt"],
            json_output=True
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn('"action":"put"', result.stdout)
        self.assertIn('"status":"ok"', result.stdout)

    def test_put_missing_arg(self):
        """Test put command without argument shows error."""
        result = self.run_ftp_interactive(["put"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr.lower())

    # === Directory operations ===

    def test_mkdir_command(self):
        """Test mkdir command creates directory."""
        result = self.run_ftp_interactive(["mkdir newdir"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Created directory", result.stdout)
        # Verify directory on server
        server_dir = Path(self.test_root) / "newdir"
        self.assertTrue(server_dir.is_dir())

    def test_mkdir_json(self):
        """Test mkdir command with JSON output."""
        result = self.run_ftp_interactive(["mkdir jsondir"], json_output=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('"action":"mkdir"', result.stdout)
        self.assertIn('"status":"ok"', result.stdout)

    def test_mkdir_missing_arg(self):
        """Test mkdir command without argument shows error."""
        result = self.run_ftp_interactive(["mkdir"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr.lower())

    # === Error handling ===

    def test_unknown_command(self):
        """Test unknown command shows error."""
        result = self.run_ftp_interactive(["badcmd"], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("unknown command", result.stderr.lower())

    def test_unknown_command_json(self):
        """Test unknown command with JSON output."""
        result = self.run_ftp_interactive(["badcmd"], json_output=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('"status":"error"', result.stdout)
        self.assertIn("unknown command", result.stdout.lower())

    # === Quit/exit tests ===

    def test_quit_command(self):
        """Test quit command exits cleanly."""
        result = self.run_ftp_interactive([], json_output=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("Goodbye", result.stdout)

    def test_exit_command(self):
        """Test exit command also exits cleanly."""
        args = ["-H", "localhost", "-p", str(self.SERVER_PORT)]
        result = self.run_ftp(*args, input_data="exit\n")
        self.assertEqual(result.returncode, 0)

    def test_quit_json(self):
        """Test quit with JSON output."""
        result = self.run_ftp_interactive([], json_output=True)
        self.assertEqual(result.returncode, 0)
        self.assertIn('"action":"quit"', result.stdout)
        self.assertIn('"status":"ok"', result.stdout)


if __name__ == "__main__":
    unittest.main()
