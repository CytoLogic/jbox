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

## Phase 1: Project Setup and Infrastructure ✅ COMPLETED

### 1.1 Create Directory Structure
- [x] Create `src/ftpd/` for server implementation
- [x] Create `src/apps/ftp/` for client implementation
- [x] Create `tests/ftpd/` for server tests
- [x] Create `tests/apps/ftp/` for client tests
- [x] Create `srv/ftp/` directory with test files

### 1.2 Populate Test FTP Root
- [x] Create `srv/ftp/README.txt` - sample text file
- [x] Create `srv/ftp/public/` - test subdirectory
- [x] Create `srv/ftp/public/sample.txt` - nested file
- [x] Create `srv/ftp/uploads/` - writable directory for STOR tests

---

## Phase 2: FTP Server Implementation (`src/ftpd/`) ✅ COMPLETED

### 2.1 Core Server Files ✅ COMPLETED

#### `src/ftpd/ftpd.h` - Main Header ✅ COMPLETED
- [x] Define `FTPD_DEFAULT_PORT`, `FTPD_MAX_CLIENTS`, `FTPD_BUFFER_SIZE`, `FTPD_CMD_MAX`
- [x] Define `ftpd_config_t` structure
- [x] Define `ftpd_client_t` structure
- [x] Define `ftpd_server_t` structure
- [x] Declare public API functions

#### `src/ftpd/ftpd.c` - Server Core ✅ COMPLETED
- [x] Implement `ftpd_init()` - Initialize server structure
- [x] Implement `ftpd_start()` - Create listen socket, accept loop
- [x] Implement `ftpd_stop()` - Signal shutdown
- [x] Implement `ftpd_cleanup()` - Free resources
- [x] Implement `accept_client()` - Accept new connection, spawn thread

#### `src/ftpd/ftpd_client.h` - Client Handler Header ✅ COMPLETED
- [x] Declare client session management functions
- [x] Declare command handler prototypes

#### `src/ftpd/ftpd_client.c` - Client Handler ✅ COMPLETED
- [x] Implement `ftpd_client_handler()` - Main thread entry point
- [x] Implement `ftpd_read_command()` - Read line from control connection
- [x] Implement `ftpd_send_response()` - Send NVT response
- [x] Implement `ftpd_dispatch_command()` - Route to handlers

### 2.2 FTP Command Implementations ✅ COMPLETED

#### `src/ftpd/ftpd_commands.h` ✅ COMPLETED
- [x] Declare all command handler prototypes

#### `src/ftpd/ftpd_commands.c` ✅ COMPLETED
- [x] Implement `ftpd_cmd_user()`:
  - Accept any username (no password required)
  - Set `client->username`
  - Set `client->cwd` to server root
  - Response: "230 User logged in.\r\n"

- [x] Implement `ftpd_cmd_quit()`:
  - Response: "221 Goodbye.\r\n"
  - Return -1 to signal disconnect

- [x] Implement `ftpd_cmd_port()`:
  - Parse "a1,a2,a3,a4,p1,p2" format
  - Extract IP (ignored, always use 127.0.0.1)
  - Calculate port: p1*256 + p2
  - Store in `client->data_port`
  - Response: "200 PORT command successful.\r\n"

- [x] Implement `ftpd_cmd_stor()`:
  - Validate PORT was set
  - Connect to client's data port
  - Open file for writing (in client's cwd)
  - Read binary data from data connection
  - Close data connection and file
  - Response: "150 Opening data connection.\r\n" then
             "226 Transfer complete.\r\n"

- [x] Implement `ftpd_cmd_retr()`:
  - Validate PORT was set
  - Connect to client's data port
  - Open file for reading (in client's cwd)
  - Send binary data over data connection
  - Close data connection
  - Response: "150 Opening data connection.\r\n" then
             "226 Transfer complete.\r\n"

- [x] Implement `ftpd_cmd_list()`:
  - Validate PORT was set
  - Connect to client's data port
  - Execute `ls -l` equivalent for client's cwd
  - Send directory listing over data connection
  - Response: "150 Opening data connection.\r\n" then
             "226 Transfer complete.\r\n"

- [x] Implement `ftpd_cmd_mkd()`:
  - Create directory in client's cwd
  - Response: "257 Directory created.\r\n"

- [x] Implement `ftpd_cmd_pwd()`:
  - Return current working directory
  - Response: "257 \"/path\" is current directory.\r\n"

- [x] Implement `ftpd_cmd_cwd()`:
  - Change working directory
  - Response: "250 Directory changed.\r\n"

- [x] Implement `ftpd_cmd_type()`:
  - Accept but ignore type setting (always binary)
  - Response: "200 Type set to I (binary).\r\n"

- [x] Implement `ftpd_cmd_syst()`:
  - Return system type
  - Response: "215 UNIX Type: L8\r\n"

- [x] Implement `ftpd_cmd_noop()`:
  - No operation
  - Response: "200 NOOP ok.\r\n"

### 2.3 Data Connection Helper ✅ COMPLETED

#### `src/ftpd/ftpd_data.h` ✅ COMPLETED
- [x] Declare data connection functions

#### `src/ftpd/ftpd_data.c` ✅ COMPLETED
- [x] Implement `ftpd_data_connect()`:
  - Create socket, connect to 127.0.0.1:client->data_port
  - Store in client->data_fd

- [x] Implement `ftpd_data_send()`:
  - Write data to data_fd

- [x] Implement `ftpd_data_recv()`:
  - Read data from data_fd

- [x] Implement `ftpd_data_send_file()`:
  - Send file contents over data connection

- [x] Implement `ftpd_data_recv_file()`:
  - Receive data and write to file

- [x] Implement `ftpd_data_close()`:
  - Close data_fd, set to -1

### 2.4 Path Security Helper ✅ COMPLETED

#### `src/ftpd/ftpd_path.h` ✅ COMPLETED
- [x] Declare path resolution functions

#### `src/ftpd/ftpd_path.c` ✅ COMPLETED
- [x] Implement `ftpd_resolve_path()`:
  - Handle absolute vs relative paths
  - Resolve ".." components
  - Verify final path is within server root
  - Return allocated string or NULL

- [x] Implement `ftpd_path_is_safe()`:
  - Check if path is within server root

- [x] Implement `ftpd_path_to_display()`:
  - Convert absolute path to display path

- [x] Implement `ftpd_path_join()`:
  - Join two path components

### 2.5 Server Main Entry Point ✅ COMPLETED

#### `src/ftpd/ftpd_main.c` ✅ COMPLETED
- [x] Implement argument parsing with argtable3:
  - `-h, --help` - Show usage
  - `-p, --port PORT` - Listen port (default 21021)
  - `-r, --root DIR` - Root directory (default srv/ftp)

- [x] Set up signal handlers (SIGINT, SIGTERM)
- [x] Ignore SIGPIPE
- [x] Initialize and start server
- [x] Clean up on shutdown

### 2.6 Build System Updates ✅ COMPLETED

#### Update `Makefile` (root) ✅ COMPLETED
- [x] Add `FTPD_DIR := $(SRC_DIR)/ftpd`
- [x] Add `FTPD_SRCS` variable listing source files
- [x] Add `ftpd` target to build `bin/ftpd`
- [x] Add `ftpd` to `all` target dependencies
- [x] Add `clean-ftpd` target

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
- [ ] Create Makefile following standard app template

#### `src/apps/ftp/pkg.json`
- [ ] Create package metadata file

### 3.5 Register in Build System

- [ ] Update `Makefile` (root):
  - Add `ftp` to `APP_DIRS`

- [ ] Update `src/jshell/jshell_register_externals.c`:
  - Include `apps/ftp/cmd_ftp.h`
  - Add `jshell_register_ftp_command()` call

- [ ] Update `src/jshell/jshell_register_externals.h`:
  - Declare `void jshell_register_ftp_command(void);`

---

## Phase 4: FTP Protocol Response Codes ✅ IMPLEMENTED

### Standard NVT Response Codes Implemented

```
150 - File status okay; about to open data connection
200 - Command okay
215 - System type (SYST response)
220 - Service ready for new user
221 - Service closing control connection (Goodbye)
226 - Closing data connection; transfer complete
230 - User logged in, proceed
250 - Requested file action okay, completed (CWD)
257 - "PATHNAME" created (MKD, PWD)
425 - Can't open data connection
426 - Connection closed; transfer aborted
500 - Syntax error, command unrecognized
501 - Syntax error in parameters
530 - Not logged in
550 - Requested action not taken (file unavailable)
553 - Requested action not taken (file name not allowed)
```

---

## Phase 5: Testing

### 5.1 Server Tests (`tests/ftpd/test_ftpd.py`) ✅ COMPLETED

#### Test Cases Implemented (22 tests, all passing)
- [x] `test_connect_welcome()` - Server sends 220 on connect
- [x] `test_user_command()` - USER returns 230
- [x] `test_quit_command()` - QUIT returns 221, closes connection
- [x] `test_syst_command()` - SYST returns 215
- [x] `test_noop_command()` - NOOP returns 200
- [x] `test_unknown_command()` - Unknown command returns 500
- [x] `test_auth_required()` - Commands require authentication
- [x] `test_pwd_command()` - PWD returns current directory
- [x] `test_cwd_command()` - CWD changes directory
- [x] `test_cwd_invalid_dir()` - CWD to non-existent returns 550
- [x] `test_port_command()` - PORT returns 200
- [x] `test_port_invalid_format()` - Invalid PORT returns 501
- [x] `test_list_requires_port()` - LIST requires PORT first
- [x] `test_list_command()` - LIST returns directory listing
- [x] `test_retr_command()` - RETR downloads file correctly
- [x] `test_retr_nonexistent()` - RETR non-existent returns 550
- [x] `test_stor_command()` - STOR uploads file correctly
- [x] `test_mkd_command()` - MKD creates directory
- [x] `test_mkd_existing()` - MKD existing returns 550
- [x] `test_multiple_clients()` - Multiple simultaneous connections
- [x] `test_path_traversal_cwd()` - CWD blocks path traversal
- [x] `test_path_traversal_retr()` - RETR blocks path traversal

### 5.2 Client Tests (`tests/apps/ftp/test_ftp.py`)

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

### 5.3 Update Tests Makefile ✅ COMPLETED

- [x] Add `ftpd` target to `tests/Makefile`
- [x] Add `tests/ftpd/__init__.py`
- [x] Add `tests/apps/ftp/__init__.py`
- [ ] Add `ftp` target for client tests (when client is implemented)

---

## Phase 6: Documentation ✅ COMPLETED (Server)

### 6.1 Doxygen Style Docstrings ✅ COMPLETED

All server functions have docstrings in Doxygen format.

### 6.2 Files Documented

#### Server Files ✅ COMPLETED
- [x] `src/ftpd/ftpd.h` - Public API
- [x] `src/ftpd/ftpd.c` - Server implementation
- [x] `src/ftpd/ftpd_client.h` - Client handler API
- [x] `src/ftpd/ftpd_client.c` - Client handler implementation
- [x] `src/ftpd/ftpd_commands.h` - Command handlers API
- [x] `src/ftpd/ftpd_commands.c` - Command implementations
- [x] `src/ftpd/ftpd_data.h` - Data connection API
- [x] `src/ftpd/ftpd_data.c` - Data connection implementation
- [x] `src/ftpd/ftpd_path.h` - Path utilities API
- [x] `src/ftpd/ftpd_path.c` - Path utilities implementation

#### Client Files (pending)
- [ ] `src/apps/ftp/cmd_ftp.h` - Command spec
- [ ] `src/apps/ftp/cmd_ftp.c` - Command implementation
- [ ] `src/apps/ftp/ftp_client.h` - Client session API
- [ ] `src/apps/ftp/ftp_client.c` - Client session implementation
- [ ] `src/apps/ftp/ftp_interactive.h` - Interactive mode API
- [ ] `src/apps/ftp/ftp_interactive.c` - Interactive mode implementation

---

## Progress Summary

### Completed
- ✅ Phase 1: Project Setup and Infrastructure
- ✅ Phase 2: FTP Server Implementation
- ✅ Phase 4: FTP Protocol Response Codes (server-side)
- ✅ Phase 5.1: Server Tests (22 tests passing)
- ✅ Phase 5.3: Tests Makefile (server)
- ✅ Phase 6: Documentation (server)

### Pending
- ⏳ Phase 3: FTP Client Implementation
- ⏳ Phase 5.2: Client Tests
- ⏳ Phase 6: Documentation (client)

### Git Commits
1. `efd8bf4` - feat(ftpd): implement FTP server daemon
2. `bbce2a4` - test(ftpd): add comprehensive FTP server tests

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
