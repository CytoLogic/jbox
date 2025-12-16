#!/usr/bin/env python3
"""Unit tests for the FTP server (ftpd)."""

import os
import socket
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


class FtpdTestCase(unittest.TestCase):
    """Base test case with FTP server management utilities."""

    FTPD_BIN = Path(__file__).parent.parent.parent / "bin" / "ftpd"
    SERVER_PORT = 21521  # Use different port for tests
    server_proc = None
    test_root = None

    @classmethod
    def setUpClass(cls):
        """Start the FTP server with a temporary root directory."""
        if not cls.FTPD_BIN.exists():
            raise unittest.SkipTest(f"ftpd not found at {cls.FTPD_BIN}")

        # Create temporary FTP root
        cls.test_root = tempfile.mkdtemp(prefix="ftpd_test_")

        # Create test files
        (Path(cls.test_root) / "testfile.txt").write_text("Hello, FTP!\n")
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

        # Clean up test root
        if cls.test_root:
            import shutil
            shutil.rmtree(cls.test_root, ignore_errors=True)

    def ftp_connect(self):
        """Create a control connection to the FTP server."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect(("127.0.0.1", self.SERVER_PORT))
        return sock

    def ftp_send(self, sock, command):
        """Send an FTP command."""
        sock.send(f"{command}\r\n".encode())

    def ftp_recv(self, sock):
        """Receive an FTP response line."""
        data = b""
        while True:
            chunk = sock.recv(1024)
            if not chunk:
                break
            data += chunk
            if b"\r\n" in data:
                break
        return data.decode().strip()

    def ftp_get_code(self, response):
        """Extract the response code from an FTP response."""
        if response and len(response) >= 3:
            try:
                return int(response[:3])
            except ValueError:
                pass
        return None


class TestFtpdConnection(FtpdTestCase):
    """Test FTP connection and basic commands."""

    def test_connect_welcome(self):
        """Test that server sends 220 welcome on connect."""
        sock = self.ftp_connect()
        try:
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 220)
            self.assertIn("FTP", response)
        finally:
            sock.close()

    def test_user_command(self):
        """Test USER command returns 230."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)  # Welcome

            self.ftp_send(sock, "USER testuser")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 230)
            self.assertIn("testuser", response)
        finally:
            sock.close()

    def test_quit_command(self):
        """Test QUIT command returns 221."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)  # Welcome

            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, "QUIT")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 221)
        finally:
            sock.close()

    def test_syst_command(self):
        """Test SYST command returns 215."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)  # Welcome

            self.ftp_send(sock, "SYST")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 215)
            self.assertIn("UNIX", response)
        finally:
            sock.close()

    def test_noop_command(self):
        """Test NOOP command returns 200."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)  # Welcome

            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, "NOOP")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 200)
        finally:
            sock.close()

    def test_unknown_command(self):
        """Test unknown command returns 500."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)  # Welcome

            self.ftp_send(sock, "XYZZY")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 500)
        finally:
            sock.close()

    def test_auth_required(self):
        """Test that commands require authentication."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)  # Welcome

            self.ftp_send(sock, "PWD")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 530)
        finally:
            sock.close()


class TestFtpdNavigation(FtpdTestCase):
    """Test directory navigation commands."""

    def test_pwd_command(self):
        """Test PWD command returns current directory."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, "PWD")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 257)
            self.assertIn("/", response)
        finally:
            sock.close()

    def test_cwd_command(self):
        """Test CWD command changes directory."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, "CWD subdir")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 250)

            self.ftp_send(sock, "PWD")
            response = self.ftp_recv(sock)
            self.assertIn("subdir", response)
        finally:
            sock.close()

    def test_cwd_invalid_dir(self):
        """Test CWD to non-existent directory returns 550."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, "CWD nonexistent")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 550)
        finally:
            sock.close()


class TestFtpdPort(FtpdTestCase):
    """Test PORT command."""

    def test_port_command(self):
        """Test PORT command is accepted."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            # PORT 127,0,0,1,39,16 = port 10000
            self.ftp_send(sock, "PORT 127,0,0,1,39,16")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 200)
        finally:
            sock.close()

    def test_port_invalid_format(self):
        """Test PORT with invalid format returns 501."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, "PORT invalid")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 501)
        finally:
            sock.close()

    def test_list_requires_port(self):
        """Test LIST requires PORT first."""
        sock = self.ftp_connect()
        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, "LIST")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 425)
        finally:
            sock.close()


class TestFtpdDataTransfer(FtpdTestCase):
    """Test data transfer commands (LIST, RETR, STOR)."""

    def setup_data_listener(self):
        """Create a listening socket for data connection."""
        data_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        data_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        data_sock.bind(("127.0.0.1", 0))
        data_sock.listen(1)
        data_sock.settimeout(5)
        _, port = data_sock.getsockname()
        return data_sock, port

    def port_command_args(self, port):
        """Format PORT command arguments."""
        p1 = port >> 8
        p2 = port & 0xFF
        return f"127,0,0,1,{p1},{p2}"

    def test_list_command(self):
        """Test LIST command returns directory listing."""
        sock = self.ftp_connect()
        data_sock, data_port = self.setup_data_listener()

        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, f"PORT {self.port_command_args(data_port)}")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 200)

            self.ftp_send(sock, "LIST")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 150)

            # Accept data connection
            conn, _ = data_sock.accept()
            conn.settimeout(5)

            # Receive listing
            listing = b""
            while True:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                listing += chunk
            conn.close()

            listing_str = listing.decode()
            self.assertIn("testfile.txt", listing_str)
            self.assertIn("subdir", listing_str)

            # Should get 226 after transfer
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 226)

        finally:
            data_sock.close()
            sock.close()

    def test_retr_command(self):
        """Test RETR command downloads file."""
        sock = self.ftp_connect()
        data_sock, data_port = self.setup_data_listener()

        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, f"PORT {self.port_command_args(data_port)}")
            self.ftp_recv(sock)

            self.ftp_send(sock, "RETR testfile.txt")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 150)

            # Accept data connection
            conn, _ = data_sock.accept()
            conn.settimeout(5)

            # Receive file
            content = b""
            while True:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                content += chunk
            conn.close()

            self.assertEqual(content, b"Hello, FTP!\n")

            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 226)

        finally:
            data_sock.close()
            sock.close()

    def test_retr_nonexistent(self):
        """Test RETR of non-existent file returns 550."""
        sock = self.ftp_connect()

        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, "PORT 127,0,0,1,39,16")
            self.ftp_recv(sock)

            self.ftp_send(sock, "RETR nonexistent.txt")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 550)

        finally:
            sock.close()

    def test_stor_command(self):
        """Test STOR command uploads file."""
        sock = self.ftp_connect()
        data_sock, data_port = self.setup_data_listener()

        upload_name = f"upload_{os.getpid()}.txt"
        upload_path = Path(self.test_root) / upload_name

        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, f"PORT {self.port_command_args(data_port)}")
            self.ftp_recv(sock)

            self.ftp_send(sock, f"STOR {upload_name}")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 150)

            # Accept data connection
            conn, _ = data_sock.accept()
            conn.settimeout(5)

            # Send file content
            conn.send(b"Uploaded content\n")
            conn.close()

            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 226)

            # Verify file was created
            self.assertTrue(upload_path.exists())
            self.assertEqual(upload_path.read_text(), "Uploaded content\n")

        finally:
            data_sock.close()
            sock.close()
            if upload_path.exists():
                upload_path.unlink()


class TestFtpdMkd(FtpdTestCase):
    """Test MKD command."""

    def test_mkd_command(self):
        """Test MKD creates directory."""
        sock = self.ftp_connect()
        new_dir = f"newdir_{os.getpid()}"
        new_path = Path(self.test_root) / new_dir

        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, f"MKD {new_dir}")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 257)

            self.assertTrue(new_path.exists())
            self.assertTrue(new_path.is_dir())

        finally:
            sock.close()
            if new_path.exists():
                new_path.rmdir()

    def test_mkd_existing(self):
        """Test MKD on existing directory returns 550."""
        sock = self.ftp_connect()

        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, "MKD subdir")
            response = self.ftp_recv(sock)
            self.assertEqual(self.ftp_get_code(response), 550)

        finally:
            sock.close()


class TestFtpdSecurity(FtpdTestCase):
    """Test path traversal security."""

    def test_path_traversal_cwd(self):
        """Test CWD blocks path traversal."""
        sock = self.ftp_connect()

        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, "CWD ..")
            response = self.ftp_recv(sock)
            # Should either fail or stay in root
            if self.ftp_get_code(response) == 250:
                # If it "succeeded", verify we're still in root
                self.ftp_send(sock, "PWD")
                pwd_response = self.ftp_recv(sock)
                self.assertIn("/", pwd_response)
                # Should not contain parent path
                self.assertNotIn(str(Path(self.test_root).parent), pwd_response)
            else:
                self.assertEqual(self.ftp_get_code(response), 550)

        finally:
            sock.close()

    def test_path_traversal_retr(self):
        """Test RETR blocks path traversal."""
        sock = self.ftp_connect()

        try:
            self.ftp_recv(sock)
            self.ftp_send(sock, "USER testuser")
            self.ftp_recv(sock)

            self.ftp_send(sock, "PORT 127,0,0,1,39,16")
            self.ftp_recv(sock)

            self.ftp_send(sock, "RETR ../../../etc/passwd")
            response = self.ftp_recv(sock)
            # Should fail - either 550 (not found) or 553 (invalid)
            code = self.ftp_get_code(response)
            self.assertIn(code, [550, 553])

        finally:
            sock.close()


class TestFtpdMultipleClients(FtpdTestCase):
    """Test multiple simultaneous clients."""

    def test_multiple_clients(self):
        """Test server handles multiple clients."""
        sock1 = self.ftp_connect()
        sock2 = self.ftp_connect()

        try:
            # Both should get welcome
            response1 = self.ftp_recv(sock1)
            response2 = self.ftp_recv(sock2)
            self.assertEqual(self.ftp_get_code(response1), 220)
            self.assertEqual(self.ftp_get_code(response2), 220)

            # Both can log in
            self.ftp_send(sock1, "USER user1")
            self.ftp_send(sock2, "USER user2")
            response1 = self.ftp_recv(sock1)
            response2 = self.ftp_recv(sock2)
            self.assertEqual(self.ftp_get_code(response1), 230)
            self.assertEqual(self.ftp_get_code(response2), 230)

        finally:
            sock1.close()
            sock2.close()


if __name__ == "__main__":
    unittest.main()
