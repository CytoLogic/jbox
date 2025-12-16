# FTP Server and Client Implementation Plan

## Overview

Implement a basic FTP server (`ftpd`) and client (`ftp`) for the jbox project. The server runs as a standalone daemon, and the client is an external app following the standard command anatomy.

### Key Specifications

- **Server Port:** 21021
- **Server Root:** `$PROJECT_ROOT/srv/ftp`
- **Protocol:** FTP-like with NVT responses
- **Data Connections:** Separate port per transfer (PORT mode)
- **Threading:** pthreads for connection handling
- **Sockets:** Unix TCP sockets

---

## Phase 1: Project Setup and Infrastructure

### 1.1 Create Directory Structure
- [ ] Create `src/ftpd/` for server implementation
- [ ] Create `src/apps/ftp/` for client implementation
- [ ] Create `tests/ftpd/` for server tests
- [ ] Create `tests/apps/ftp/` for client tests
- [ ] Create `srv/ftp/` directory with test files

### 1.2 Populate Test FTP Root
- [ ] Create `srv/ftp/README.txt` - sample text file
- [ ] Create `srv/ftp/public/` - test subdirectory
- [ ] Create `srv/ftp/public/sample.txt` - nested file
- [ ] Create `srv/ftp/uploads/` - writable directory for STOR tests

---

## Phase 2: FTP Server Implementation (`src/ftpd/`)

### 2.1 Core Server Files

#### `src/ftpd/ftpd.h` - Main Header
```c
#ifndef FTPD_H
#define FTPD_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#define FTPD_DEFAULT_PORT 21021
#define FTPD_MAX_CLIENTS 64
#define FTPD_BUFFER_SIZE 4096
#define FTPD_CMD_MAX 512

typedef struct ftpd_config {
  uint16_t port;
  const char *root_dir;
  int max_clients;
} ftpd_config_t;

typedef struct ftpd_client {
  int ctrl_fd;
  int data_fd;
  uint16_t data_port;
  char username[64];
  char cwd[PATH_MAX];
  bool authenticated;
  bool passive_mode;
  pthread_t thread;
  struct ftpd_client *next;
} ftpd_client_t;

typedef struct ftpd_server {
  int listen_fd;
  ftpd_config_t config;
  ftpd_client_t *clients;
  pthread_mutex_t clients_lock;
  bool running;
} ftpd_server_t;

int ftpd_init(ftpd_server_t *server, const ftpd_config_t *config);
int ftpd_start(ftpd_server_t *server);
void ftpd_stop(ftpd_server_t *server);
void ftpd_cleanup(ftpd_server_t *server);

#endif
```

#### `src/ftpd/ftpd.c` - Server Core
- [ ] Implement `ftpd_init()` - Initialize server structure
- [ ] Implement `ftpd_start()` - Create listen socket, accept loop
- [ ] Implement `ftpd_stop()` - Signal shutdown
- [ ] Implement `ftpd_cleanup()` - Free resources
- [ ] Implement `accept_client()` - Accept new connection, spawn thread

#### `src/ftpd/ftpd_client.h` - Client Handler Header
- [ ] Declare client session management functions
- [ ] Declare command handler prototypes

#### `src/ftpd/ftpd_client.c` - Client Handler
- [ ] Implement `ftpd_client_handler()` - Main thread entry point
- [ ] Implement `ftpd_read_command()` - Read line from control connection
- [ ] Implement `ftpd_send_response()` - Send NVT response
- [ ] Implement `ftpd_dispatch_command()` - Route to handlers

### 2.2 FTP Command Implementations

#### `src/ftpd/ftpd_commands.h`
```c
#ifndef FTPD_COMMANDS_H
#define FTPD_COMMANDS_H

#include "ftpd.h"

// Command handlers - return 0 on success, -1 to disconnect
int ftpd_cmd_user(ftpd_client_t *client, const char *arg);
int ftpd_cmd_quit(ftpd_client_t *client, const char *arg);
int ftpd_cmd_port(ftpd_client_t *client, const char *arg);
int ftpd_cmd_stor(ftpd_client_t *client, const char *arg);
int ftpd_cmd_retr(ftpd_client_t *client, const char *arg);
int ftpd_cmd_list(ftpd_client_t *client, const char *arg);
int ftpd_cmd_mkd(ftpd_client_t *client, const char *arg);

#endif
```

#### `src/ftpd/ftpd_commands.c`
- [ ] Implement `ftpd_cmd_user()`:
  - Accept any username (no password required)
  - Set `client->username`
  - Set `client->cwd` to server root + "/" + username
  - Create user directory if needed
  - Response: "230 User logged in.\r\n"

- [ ] Implement `ftpd_cmd_quit()`:
  - Response: "221 Goodbye.\r\n"
  - Return -1 to signal disconnect

- [ ] Implement `ftpd_cmd_port()`:
  - Parse "a1,a2,a3,a4,p1,p2" format
  - Extract IP (ignored, always use 127.0.0.1)
  - Calculate port: p1*256 + p2
  - Store in `client->data_port`
  - Response: "200 PORT command successful.\r\n"

- [ ] Implement `ftpd_cmd_stor()`:
  - Validate PORT was set
  - Connect to client's data port
  - Open file for writing (in client's cwd)
  - Read binary data from data connection
  - Close data connection and file
  - Response: "150 Opening data connection.\r\n" then
             "226 Transfer complete.\r\n"

- [ ] Implement `ftpd_cmd_retr()`:
  - Validate PORT was set
  - Connect to client's data port
  - Open file for reading (in client's cwd)
  - Send binary data over data connection
  - Close data connection
  - Response: "150 Opening data connection.\r\n" then
             "226 Transfer complete.\r\n"

- [ ] Implement `ftpd_cmd_list()`:
  - Validate PORT was set
  - Connect to client's data port
  - Execute `ls -l` equivalent for client's cwd
  - Send directory listing over data connection
  - Response: "150 Opening data connection.\r\n" then
             "226 Transfer complete.\r\n"

- [ ] Implement `ftpd_cmd_mkd()`:
  - Create directory in client's cwd
  - Response: "257 Directory created.\r\n"

### 2.3 Data Connection Helper

#### `src/ftpd/ftpd_data.h`
```c
#ifndef FTPD_DATA_H
#define FTPD_DATA_H

#include "ftpd.h"

int ftpd_data_connect(ftpd_client_t *client);
int ftpd_data_send(ftpd_client_t *client, const void *data, size_t len);
int ftpd_data_recv(ftpd_client_t *client, void *buf, size_t len);
void ftpd_data_close(ftpd_client_t *client);

#endif
```

#### `src/ftpd/ftpd_data.c`
- [ ] Implement `ftpd_data_connect()`:
  - Create socket, connect to 127.0.0.1:client->data_port
  - Store in client->data_fd

- [ ] Implement `ftpd_data_send()`:
  - Write data to data_fd

- [ ] Implement `ftpd_data_recv()`:
  - Read data from data_fd

- [ ] Implement `ftpd_data_close()`:
  - Close data_fd, set to -1

### 2.4 Path Security Helper

#### `src/ftpd/ftpd_path.h`
```c
#ifndef FTPD_PATH_H
#define FTPD_PATH_H

#include "ftpd.h"

// Resolve path relative to client's cwd, ensure within server root
// Returns NULL if path escapes root (path traversal attempt)
char *ftpd_resolve_path(ftpd_client_t *client, const char *path,
                        const char *server_root);

#endif
```

#### `src/ftpd/ftpd_path.c`
- [ ] Implement `ftpd_resolve_path()`:
  - Handle absolute vs relative paths
  - Resolve ".." components
  - Verify final path is within server root
  - Return allocated string or NULL

### 2.5 Server Main Entry Point

#### `src/ftpd/ftpd_main.c`
```c
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "ftpd.h"

static ftpd_server_t server;

static void handle_sigint(int sig) {
  (void)sig;
  ftpd_stop(&server);
}

int main(int argc, char **argv) {
  // Parse args (--port, --root, --help)
  // Set up signal handlers
  // Initialize and start server
  // Run until stopped
  // Cleanup
}
```

- [ ] Implement argument parsing with argtable3:
  - `-h, --help` - Show usage
  - `-p, --port PORT` - Listen port (default 21021)
  - `-r, --root DIR` - Root directory (default srv/ftp)
  - `-d, --daemon` - Daemonize (optional)

### 2.6 Build System Updates

#### Update `Makefile` (root)
- [ ] Add `FTPD_DIR := $(SRC_DIR)/ftpd`
- [ ] Add `FTPD_SRCS` variable listing source files
- [ ] Add `ftpd` target to build `bin/ftpd`
- [ ] Add `ftpd` to `all` target dependencies

```makefile
FTPD_DIR := $(SRC_DIR)/ftpd
FTPD_SRCS := $(FTPD_DIR)/ftpd.c \
             $(FTPD_DIR)/ftpd_main.c \
             $(FTPD_DIR)/ftpd_client.c \
             $(FTPD_DIR)/ftpd_commands.c \
             $(FTPD_DIR)/ftpd_data.c \
             $(FTPD_DIR)/ftpd_path.c

ftpd: $(ARGTABLE3_OBJ)
	mkdir -p bin/
	$(COMPILE) $(FTPD_SRCS) $(ARGTABLE3_OBJ) -lpthread -o $(BIN_DIR)/ftpd

all: jbox apps packages ftpd
```

---

## Phase 3: FTP Client Implementation (`src/apps/ftp/`)

### 3.1 Client Module Structure

#### `src/apps/ftp/cmd_ftp.h`
```c
#ifndef CMD_FTP_H
#define CMD_FTP_H

#include "jshell/jshell_cmd_registry.h"

extern const jshell_cmd_spec_t cmd_ftp_spec;

void jshell_register_ftp_command(void);

#endif
```

#### `src/apps/ftp/cmd_ftp.c`
- [ ] Implement argument structure:
  ```c
  typedef struct {
    struct arg_lit *help;
    struct arg_str *host;
    struct arg_int *port;
    struct arg_lit *json;
    struct arg_end *end;
    void *argtable[5];
  } ftp_args_t;
  ```

- [ ] Implement `build_ftp_argtable()`:
  - `-h, --help` - Show usage
  - `-H, --host HOST` - Server hostname (default localhost)
  - `-p, --port PORT` - Server port (default 21021)
  - `--json` - JSON output for operations

- [ ] Implement `ftp_print_usage()`

- [ ] Implement `ftp_run()`:
  - Parse arguments
  - Connect to server
  - Enter interactive mode or execute single command

- [ ] Define `cmd_ftp_spec`:
  ```c
  const jshell_cmd_spec_t cmd_ftp_spec = {
    .name = "ftp",
    .summary = "FTP client",
    .long_help = "Connect to FTP server for file transfer.",
    .type = CMD_EXTERNAL,
    .run = ftp_run,
    .print_usage = ftp_print_usage,
  };
  ```

### 3.2 FTP Client Core

#### `src/apps/ftp/ftp_client.h`
```c
#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  int ctrl_fd;
  int data_listen_fd;
  uint16_t data_port;
  char last_response[512];
  int last_code;
} ftp_session_t;

int ftp_connect(ftp_session_t *session, const char *host, uint16_t port);
int ftp_login(ftp_session_t *session, const char *username);
int ftp_quit(ftp_session_t *session);
int ftp_list(ftp_session_t *session, char **output);
int ftp_get(ftp_session_t *session, const char *remote, const char *local);
int ftp_put(ftp_session_t *session, const char *local, const char *remote);
int ftp_mkdir(ftp_session_t *session, const char *dirname);
void ftp_close(ftp_session_t *session);

#endif
```

#### `src/apps/ftp/ftp_client.c`
- [ ] Implement `ftp_connect()`:
  - Create socket
  - Connect to server
  - Read welcome message (220)

- [ ] Implement `ftp_send_command()`:
  - Send command string
  - Append \r\n

- [ ] Implement `ftp_read_response()`:
  - Read response line
  - Parse response code

- [ ] Implement `ftp_setup_data_port()`:
  - Bind to ephemeral port
  - Listen for connection
  - Send PORT command to server
  - Store listen fd in session

- [ ] Implement `ftp_accept_data()`:
  - Accept connection on data_listen_fd
  - Return data socket

- [ ] Implement `ftp_login()`:
  - Send USER command
  - Check for 230 response

- [ ] Implement `ftp_quit()`:
  - Send QUIT command
  - Check for 221 response

- [ ] Implement `ftp_list()`:
  - Setup data port
  - Send LIST command
  - Accept data connection
  - Read directory listing
  - Return allocated string

- [ ] Implement `ftp_get()`:
  - Setup data port
  - Send RETR command
  - Accept data connection
  - Write to local file

- [ ] Implement `ftp_put()`:
  - Setup data port
  - Send STOR command
  - Accept data connection
  - Send local file contents

- [ ] Implement `ftp_mkdir()`:
  - Send MKD command
  - Check for 257 response

### 3.3 Interactive Mode

#### `src/apps/ftp/ftp_interactive.h`
```c
#ifndef FTP_INTERACTIVE_H
#define FTP_INTERACTIVE_H

#include "ftp_client.h"

int ftp_interactive(ftp_session_t *session, bool json_output);

#endif
```

#### `src/apps/ftp/ftp_interactive.c`
- [ ] Implement `ftp_interactive()`:
  - Read line from stdin
  - Parse command (ls, get, put, mkdir, quit, help)
  - Execute and display results
  - Loop until quit

- [ ] Implement command handlers:
  - `ls` - Call ftp_list, display output
  - `get <remote> [local]` - Download file
  - `put <local> [remote]` - Upload file
  - `mkdir <dir>` - Create directory
  - `help` - Show commands
  - `quit` - Exit

### 3.4 Client Main and Build

#### `src/apps/ftp/ftp_main.c`
```c
#include "cmd_ftp.h"

int main(int argc, char **argv) {
  return cmd_ftp_spec.run(argc, argv);
}
```

#### `src/apps/ftp/Makefile`
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=gnu23

ifneq ($(wildcard deps/),)
  BUILD_MODE = installed
  ARGTABLE_DIR = ./deps
  SRC_DIR = ./deps
  BIN_DIR = ./bin
  CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
  ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.c
  REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c
else
  BUILD_MODE = source
  ARGTABLE_DIR = ../../../extern/argtable3/dist
  SRC_DIR = ../../../src
  BIN_DIR = ../../../bin/standalone-apps
  CFLAGS += -fsanitize=address,undefined
  CFLAGS += -I$(ARGTABLE_DIR) -I$(SRC_DIR)
  ARGTABLE_SRC = $(ARGTABLE_DIR)/argtable3.o
  REGISTRY_SRC = $(SRC_DIR)/jshell/jshell_cmd_registry.c
endif

SRCS = cmd_ftp.c ftp_client.c ftp_interactive.c
OBJS = $(SRCS:.c=.o)
LIB = libftp.a
BIN = $(BIN_DIR)/ftp
PKG_BIN = $(BIN)

all: $(BIN) $(LIB)
	@echo "Build mode: $(BUILD_MODE)"

$(LIB): $(OBJS)
	ar rcs $(LIB) $(OBJS)

$(BIN): ftp_main.o $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) ftp_main.o $(OBJS) $(REGISTRY_SRC) $(ARGTABLE_SRC) -lm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f *.o $(BIN) $(LIB)

ifeq ($(BUILD_MODE),source)
include pkg.mk
endif
```

#### `src/apps/ftp/pkg.json`
```json
{
  "name": "ftp",
  "version": "0.0.1",
  "description": "FTP client for file transfer",
  "files": ["bin/ftp"],
  "docs": ["README.md"]
}
```

### 3.5 Register in Build System

- [ ] Update `Makefile` (root):
  - Add `ftp` to `APP_DIRS`

- [ ] Update `src/jshell/jshell_register_externals.c`:
  - Include `apps/ftp/cmd_ftp.h`
  - Add `jshell_register_ftp_command()` call

- [ ] Update `src/jshell/jshell_register_externals.h`:
  - Declare `void jshell_register_ftp_command(void);`

---

## Phase 4: FTP Protocol Response Codes

### Standard NVT Response Codes to Implement

```
110 - Restart marker reply
120 - Service ready in nnn minutes
125 - Data connection already open
150 - File status okay; about to open data connection
200 - Command okay
220 - Service ready for new user
221 - Service closing control connection (Goodbye)
226 - Closing data connection; transfer complete
230 - User logged in, proceed
250 - Requested file action okay, completed
257 - "PATHNAME" created
331 - User name okay, need password (not implemented - no auth)
425 - Can't open data connection
426 - Connection closed; transfer aborted
450 - Requested file action not taken
500 - Syntax error, command unrecognized
501 - Syntax error in parameters
502 - Command not implemented
530 - Not logged in
550 - Requested action not taken (file unavailable)
553 - Requested action not taken (file name not allowed)
```

---

## Phase 5: Testing

### 5.1 Server Tests (`tests/ftpd/test_ftpd.py`)

```python
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
    """Base class with server management utilities."""

    FTPD_BIN = Path(__file__).parent.parent.parent / "bin" / "ftpd"
    SERVER_PORT = 21521  # Use different port for tests
    server_proc = None

    @classmethod
    def setUpClass(cls):
        """Start the FTP server."""
        if not cls.FTPD_BIN.exists():
            raise unittest.SkipTest(f"ftpd not found at {cls.FTPD_BIN}")

        cls.test_root = tempfile.mkdtemp(prefix="ftpd_test_")
        cls.server_proc = subprocess.Popen(
            [str(cls.FTPD_BIN), "-p", str(cls.SERVER_PORT),
             "-r", cls.test_root],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        time.sleep(0.5)  # Wait for server to start

    @classmethod
    def tearDownClass(cls):
        """Stop the FTP server."""
        if cls.server_proc:
            cls.server_proc.terminate()
            cls.server_proc.wait()

    def ftp_connect(self):
        """Create a control connection."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", self.SERVER_PORT))
        sock.settimeout(5)
        return sock

    def ftp_send(self, sock, command):
        """Send FTP command."""
        sock.send(f"{command}\r\n".encode())

    def ftp_recv(self, sock):
        """Receive FTP response."""
        data = b""
        while True:
            chunk = sock.recv(1024)
            data += chunk
            if b"\r\n" in data:
                break
        return data.decode().strip()
```

#### Test Cases to Implement

- [ ] `test_connect_welcome()` - Server sends 220 on connect
- [ ] `test_user_command()` - USER returns 230
- [ ] `test_quit_command()` - QUIT returns 221, closes connection
- [ ] `test_port_command()` - PORT returns 200
- [ ] `test_list_command()` - LIST returns directory listing
- [ ] `test_stor_command()` - STOR uploads file correctly
- [ ] `test_retr_command()` - RETR downloads file correctly
- [ ] `test_mkd_command()` - MKD creates directory
- [ ] `test_invalid_command()` - Unknown command returns 500
- [ ] `test_multiple_clients()` - Multiple simultaneous connections
- [ ] `test_path_traversal_blocked()` - "../" doesn't escape root
- [ ] `test_binary_transfer()` - Binary files transfer correctly

### 5.2 Client Tests (`tests/apps/ftp/test_ftp.py`)

```python
#!/usr/bin/env python3
"""Unit tests for the FTP client."""

import json
import os
import socket
import subprocess
import tempfile
import threading
import time
import unittest
from pathlib import Path


class MockFtpServer:
    """Simple mock FTP server for client testing."""

    def __init__(self, port):
        self.port = port
        self.sock = None
        self.running = False
        self.commands_received = []

    def start(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("127.0.0.1", self.port))
        self.sock.listen(1)
        self.running = True

    def stop(self):
        self.running = False
        if self.sock:
            self.sock.close()


class TestFtpClient(unittest.TestCase):
    """Test cases for the ftp client command."""

    FTP_BIN = Path(__file__).parent.parent.parent.parent \
              / "bin" / "standalone-apps" / "ftp"

    @classmethod
    def setUpClass(cls):
        if not cls.FTP_BIN.exists():
            raise unittest.SkipTest(f"ftp not found at {cls.FTP_BIN}")

    def run_ftp(self, *args, input_data=None):
        """Run the ftp client with given arguments."""
        cmd = [str(self.FTP_BIN)] + list(args)
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            input=input_data,
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}
        )
        return result

    def test_help_short(self):
        """Test -h flag shows help."""
        result = self.run_ftp("-h")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: ftp", result.stdout)

    def test_help_long(self):
        """Test --help flag shows help."""
        result = self.run_ftp("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage: ftp", result.stdout)
```

#### Test Cases to Implement

- [ ] `test_help_short()` - -h shows help
- [ ] `test_help_long()` - --help shows help
- [ ] `test_connect_success()` - Connects to server
- [ ] `test_connect_failure()` - Handles connection failure gracefully
- [ ] `test_login()` - USER command works
- [ ] `test_list_command()` - ls command works
- [ ] `test_get_command()` - get downloads file
- [ ] `test_put_command()` - put uploads file
- [ ] `test_mkdir_command()` - mkdir creates directory
- [ ] `test_quit_command()` - quit exits cleanly
- [ ] `test_json_output()` - --json produces valid JSON

### 5.3 Update Tests Makefile

- [ ] Add to `tests/Makefile`:
  ```makefile
  ftpd:
  	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.ftpd.test_ftpd -v

  ftp:
  	cd $(PROJECT_ROOT) && $(PYTHON) -m unittest tests.apps.ftp.test_ftp -v
  ```

- [ ] Add `__init__.py` files:
  - `tests/ftpd/__init__.py`
  - `tests/apps/ftp/__init__.py`

---

## Phase 6: Documentation

### 6.1 Doxygen Style Docstrings

All functions should have docstrings in this format:
```c
/**
 * @brief Brief description.
 *
 * Detailed description if needed.
 *
 * @param param1 Description of param1.
 * @param param2 Description of param2.
 * @return Description of return value.
 */
```

### 6.2 Files to Document

#### Server Files
- [ ] `src/ftpd/ftpd.h` - Public API
- [ ] `src/ftpd/ftpd.c` - Server implementation
- [ ] `src/ftpd/ftpd_client.h` - Client handler API
- [ ] `src/ftpd/ftpd_client.c` - Client handler implementation
- [ ] `src/ftpd/ftpd_commands.h` - Command handlers API
- [ ] `src/ftpd/ftpd_commands.c` - Command implementations
- [ ] `src/ftpd/ftpd_data.h` - Data connection API
- [ ] `src/ftpd/ftpd_data.c` - Data connection implementation
- [ ] `src/ftpd/ftpd_path.h` - Path utilities API
- [ ] `src/ftpd/ftpd_path.c` - Path utilities implementation

#### Client Files
- [ ] `src/apps/ftp/cmd_ftp.h` - Command spec
- [ ] `src/apps/ftp/cmd_ftp.c` - Command implementation
- [ ] `src/apps/ftp/ftp_client.h` - Client session API
- [ ] `src/apps/ftp/ftp_client.c` - Client session implementation
- [ ] `src/apps/ftp/ftp_interactive.h` - Interactive mode API
- [ ] `src/apps/ftp/ftp_interactive.c` - Interactive mode implementation

---

## Implementation Order

1. **Phase 1: Setup** - Create directories, test files
2. **Phase 2.1-2.3**: Server core and commands (USER, QUIT, PORT, LIST)
3. **Phase 5.1 (partial)**: Basic server tests (connect, USER, QUIT, LIST)
4. **Phase 2.3 (continued)**: STOR, RETR, MKD commands
5. **Phase 5.1 (complete)**: Full server tests
6. **Phase 3.1-3.4**: Client implementation
7. **Phase 5.2**: Client tests
8. **Phase 2.6**: Build system integration
9. **Phase 6**: Documentation pass

---

## Notes

### Thread Safety
- Use mutex for client list modifications
- Each client has its own thread with isolated state
- Server shutdown signals all client threads

### Path Security
- All paths resolved relative to server root
- Reject paths containing ".." that escape root
- Use realpath() to canonicalize

### Error Handling
- All socket operations check for errors
- Clean up resources on failure
- Send appropriate FTP error codes

### Binary Mode
- All file transfers are binary (TYPE I implied)
- No ASCII mode conversion needed
