#!/usr/bin/env python3
"""Base test class for package manager tests."""

import json
import os
import shutil
import subprocess
import tempfile
import time
import unittest
from pathlib import Path
from typing import Optional
from urllib.error import URLError
from urllib.request import urlopen

from tests.helpers import JShellRunner


class PkgTestBase(unittest.TestCase):
    """Base class for package manager tests.

    Provides common setup/teardown for .jshell directory management,
    helpers for creating test packages, and registry server control.
    """

    # Paths
    PROJECT_ROOT = Path(__file__).parent.parent.parent
    PKG_BIN = PROJECT_ROOT / "bin" / "standalone-apps" / "pkg"
    JSHELL_BIN = PROJECT_ROOT / "bin" / "jshell"
    JSHELL_HOME = Path.home() / ".jshell"

    # Registry server
    START_SCRIPT = PROJECT_ROOT / "scripts" / "start-pkg-server.sh"
    SHUTDOWN_SCRIPT = PROJECT_ROOT / "scripts" / "shutdown-pkg-server.sh"
    REGISTRY_URL = "http://localhost:3000"
    PKG_REPO_DIR = PROJECT_ROOT / "srv" / "pkg_repository"
    DOWNLOADS_DIR = PKG_REPO_DIR / "downloads"

    # Class-level state
    backup_dir: Optional[Path] = None
    server_process: Optional[subprocess.Popen] = None
    _server_started: bool = False

    @classmethod
    def setUpClass(cls):
        """Set up class-level resources.

        - Verify pkg binary exists
        - Backup existing .jshell directory
        - Start registry server
        """
        if not cls.PKG_BIN.exists():
            raise unittest.SkipTest(f"pkg binary not found at {cls.PKG_BIN}")

        # Backup existing .jshell if it exists
        cls.backup_dir = None
        if cls.JSHELL_HOME.exists():
            cls.backup_dir = cls.JSHELL_HOME.parent / ".jshell.backup"
            if cls.backup_dir.exists():
                shutil.rmtree(cls.backup_dir)
            shutil.move(cls.JSHELL_HOME, cls.backup_dir)

        # Start registry server for all tests
        try:
            cls.start_registry_server()
        except unittest.SkipTest:
            # Server not available, tests will run without it
            pass

    @classmethod
    def tearDownClass(cls):
        """Tear down class-level resources.

        - Stop registry server if started
        - Restore .jshell directory
        """
        cls.stop_registry_server()

        # Restore .jshell
        if cls.JSHELL_HOME.exists():
            shutil.rmtree(cls.JSHELL_HOME)
        if cls.backup_dir and cls.backup_dir.exists():
            shutil.move(cls.backup_dir, cls.JSHELL_HOME)

    def setUp(self):
        """Clean up .jshell before each test."""
        if self.JSHELL_HOME.exists():
            shutil.rmtree(self.JSHELL_HOME)

    # -------------------------------------------------------------------------
    # Command Runners
    # -------------------------------------------------------------------------

    def run_pkg(self, *args, cwd: Optional[str] = None,
                timeout: Optional[int] = None) -> subprocess.CompletedProcess:
        """Run the pkg command directly with given arguments.

        Args:
            *args: Arguments to pass to pkg
            cwd: Working directory
            timeout: Timeout in seconds

        Returns:
            CompletedProcess with stdout/stderr
        """
        cmd = [str(self.PKG_BIN)] + list(args)
        return subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            cwd=cwd,
            timeout=timeout,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )

    def run_pkg_json(self, *args, cwd: Optional[str] = None) -> dict:
        """Run pkg command and parse JSON output.

        Args:
            *args: Arguments to pass to pkg (should include --json)
            cwd: Working directory

        Returns:
            Parsed JSON output
        """
        result = self.run_pkg(*args, cwd=cwd)
        return json.loads(result.stdout)

    def run_shell(self, command: str, timeout: Optional[int] = None
                  ) -> subprocess.CompletedProcess:
        """Run command via jshell -c.

        Args:
            command: Shell command to execute
            timeout: Timeout in seconds

        Returns:
            CompletedProcess with stdout/stderr
        """
        return JShellRunner.run(command, timeout=timeout)

    def run_shell_json(self, command: str) -> dict:
        """Run shell command and parse JSON output.

        Args:
            command: Shell command to execute (should produce JSON)

        Returns:
            Parsed JSON output
        """
        return JShellRunner.run_json(command)

    # -------------------------------------------------------------------------
    # Test Package Creation
    # -------------------------------------------------------------------------

    def create_test_package(self, name: str = "test-pkg",
                            version: str = "1.0.0",
                            description: Optional[str] = None,
                            files: Optional[list] = None,
                            with_source: bool = False) -> Path:
        """Create a test package directory with pkg.json.

        Args:
            name: Package name
            version: Package version
            description: Package description (default: auto-generated)
            files: List of files to include (default: ["bin/<name>"])
            with_source: Include source files for compilation

        Returns:
            Path to the package directory (caller must clean up parent tmpdir)
        """
        tmpdir = tempfile.mkdtemp()
        pkg_dir = Path(tmpdir) / name
        pkg_dir.mkdir()
        bin_dir = pkg_dir / "bin"
        bin_dir.mkdir()

        # Create executable
        exe_path = bin_dir / name
        exe_path.write_text(f'#!/bin/sh\necho "Hello from {name}!"')
        exe_path.chmod(0o755)

        if files is None:
            files = [f"bin/{name}"]

        # Create pkg.json
        manifest = {
            "name": name,
            "version": version,
            "description": description or f"Test package {name}",
            "files": files
        }

        if with_source:
            # Create minimal source structure
            src_dir = pkg_dir / "src"
            src_dir.mkdir()
            (src_dir / f"{name}_main.c").write_text(
                f'#include <stdio.h>\n'
                f'int main(void) {{\n'
                f'    printf("Hello from {name}!\\n");\n'
                f'    return 0;\n'
                f'}}\n'
            )
            manifest["sources"] = [f"src/{name}_main.c"]
            manifest["build"] = "gcc -o bin/{name} src/{name}_main.c"

        (pkg_dir / "pkg.json").write_text(json.dumps(manifest, indent=2))

        return pkg_dir

    def build_test_tarball(self, name: str = "test-pkg",
                           version: str = "1.0.0",
                           **kwargs) -> Path:
        """Create a test package and build a tarball.

        Args:
            name: Package name
            version: Package version
            **kwargs: Additional arguments to create_test_package

        Returns:
            Path to the tarball (caller must clean up)
        """
        pkg_dir = self.create_test_package(name, version, **kwargs)
        try:
            tarball = tempfile.NamedTemporaryFile(
                suffix=".tar.gz", delete=False
            )
            tarball.close()

            result = self.run_pkg("build", str(pkg_dir), tarball.name)
            if result.returncode != 0:
                raise RuntimeError(
                    f"Failed to build tarball: {result.stderr}"
                )

            return Path(tarball.name)
        finally:
            shutil.rmtree(pkg_dir.parent)

    def install_test_package(self, name: str = "test-pkg",
                             version: str = "1.0.0",
                             **kwargs) -> None:
        """Create, build, and install a test package.

        Args:
            name: Package name
            version: Package version
            **kwargs: Additional arguments to create_test_package
        """
        tarball = self.build_test_tarball(name, version, **kwargs)
        try:
            result = self.run_pkg("install", str(tarball))
            if result.returncode != 0:
                raise RuntimeError(
                    f"Failed to install package: {result.stderr}"
                )
        finally:
            tarball.unlink()

    # -------------------------------------------------------------------------
    # Registry Server Control
    # -------------------------------------------------------------------------

    @classmethod
    def start_registry_server(cls, timeout: float = 10.0) -> None:
        """Start the package registry server.

        Args:
            timeout: Maximum time to wait for server to start

        Raises:
            unittest.SkipTest: If server cannot be started
        """
        if cls._server_started:
            return

        if not cls.START_SCRIPT.exists():
            raise unittest.SkipTest(
                f"Start script not found at {cls.START_SCRIPT}"
            )

        cls.server_process = subprocess.Popen(
            ["bash", str(cls.START_SCRIPT)],
            cwd=cls.PROJECT_ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env={**os.environ, "PORT": "3000"}
        )

        # Wait for server to be ready
        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                urlopen(f"{cls.REGISTRY_URL}/packages", timeout=1)
                cls._server_started = True
                return
            except (URLError, ConnectionRefusedError):
                time.sleep(0.1)

            # Check if process died
            if cls.server_process.poll() is not None:
                stdout, stderr = cls.server_process.communicate()
                raise unittest.SkipTest(
                    f"Server died. stdout: {stdout.decode()}, "
                    f"stderr: {stderr.decode()}"
                )

        raise unittest.SkipTest(
            f"Server did not start within {timeout} seconds"
        )

    @classmethod
    def stop_registry_server(cls) -> None:
        """Stop the package registry server if running."""
        if not cls._server_started:
            return

        if cls.SHUTDOWN_SCRIPT.exists():
            subprocess.run(
                ["bash", str(cls.SHUTDOWN_SCRIPT)],
                cwd=cls.PROJECT_ROOT,
                capture_output=True,
                env={**os.environ, "PORT": "3000"}
            )

        if cls.server_process:
            # Close stdout/stderr pipes to avoid ResourceWarnings
            if cls.server_process.stdout:
                cls.server_process.stdout.close()
            if cls.server_process.stderr:
                cls.server_process.stderr.close()
            try:
                cls.server_process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                cls.server_process.kill()
                cls.server_process.wait()

        cls._server_started = False
        cls.server_process = None

    @classmethod
    def require_registry(cls) -> None:
        """Ensure registry server is running, skip test if not available."""
        try:
            cls.start_registry_server()
        except unittest.SkipTest:
            raise
        except Exception as e:
            raise unittest.SkipTest(f"Registry server unavailable: {e}")

    # -------------------------------------------------------------------------
    # Package Database Helpers
    # -------------------------------------------------------------------------

    def get_pkgdb_path(self) -> Path:
        """Get path to package database file."""
        # Check for JSON first (new format), then txt (old format)
        json_path = self.JSHELL_HOME / "pkgs" / "pkgdb.json"
        if json_path.exists():
            return json_path
        return self.JSHELL_HOME / "pkgdb.txt"

    def read_pkgdb(self) -> dict:
        """Read package database and return as dict.

        Returns:
            For JSON: the parsed JSON
            For txt: {"packages": [{"name": ..., "version": ...}, ...]}
        """
        json_path = self.JSHELL_HOME / "pkgs" / "pkgdb.json"
        txt_path = self.JSHELL_HOME / "pkgdb.txt"

        if json_path.exists():
            return json.loads(json_path.read_text())

        if txt_path.exists():
            packages = []
            for line in txt_path.read_text().strip().split('\n'):
                if line.strip():
                    parts = line.split()
                    if len(parts) >= 2:
                        packages.append({
                            "name": parts[0],
                            "version": parts[1]
                        })
            return {"packages": packages}

        return {"packages": []}

    def write_pkgdb_json(self, data: dict) -> None:
        """Write package database in JSON format.

        Args:
            data: Database content to write
        """
        pkgs_dir = self.JSHELL_HOME / "pkgs"
        pkgs_dir.mkdir(parents=True, exist_ok=True)
        (pkgs_dir / "pkgdb.json").write_text(json.dumps(data, indent=2))

    # -------------------------------------------------------------------------
    # Assertion Helpers
    # -------------------------------------------------------------------------

    def assertPackageInstalled(self, name: str, version: Optional[str] = None):
        """Assert a package is installed.

        Args:
            name: Package name
            version: Expected version (optional)
        """
        result = self.run_pkg("list", "--json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)

        packages = {p["name"]: p["version"] for p in data.get("packages", [])}
        self.assertIn(name, packages, f"Package {name} not installed")

        if version:
            self.assertEqual(
                packages[name], version,
                f"Package {name} version mismatch: "
                f"expected {version}, got {packages[name]}"
            )

    def assertPackageNotInstalled(self, name: str):
        """Assert a package is not installed.

        Args:
            name: Package name
        """
        result = self.run_pkg("list", "--json")
        self.assertEqual(result.returncode, 0)
        data = json.loads(result.stdout)

        packages = [p["name"] for p in data.get("packages", [])]
        self.assertNotIn(name, packages, f"Package {name} should not be installed")

    def assertBinaryExists(self, name: str):
        """Assert a binary exists in ~/.jshell/bin/.

        Args:
            name: Binary name
        """
        bin_path = self.JSHELL_HOME / "bin" / name
        self.assertTrue(
            bin_path.exists() or bin_path.is_symlink(),
            f"Binary {name} not found in {self.JSHELL_HOME / 'bin'}"
        )

    def assertBinaryNotExists(self, name: str):
        """Assert a binary does not exist in ~/.jshell/bin/.

        Args:
            name: Binary name
        """
        bin_path = self.JSHELL_HOME / "bin" / name
        self.assertFalse(
            bin_path.exists() or bin_path.is_symlink(),
            f"Binary {name} should not exist in {self.JSHELL_HOME / 'bin'}"
        )


class PkgRegistryTestBase(PkgTestBase):
    """Base class for tests that require the registry server."""

    @classmethod
    def setUpClass(cls):
        """Set up class with registry server."""
        super().setUpClass()
        cls.require_registry()
