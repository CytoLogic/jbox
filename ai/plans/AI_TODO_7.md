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

## Phase 3: FTP Client Implementation (`src/apps/ftp/`) ✅ COMPLETED

### 3.1 Client Module Structure ✅ COMPLETED

#### `src/apps/ftp/cmd_ftp.h` ✅ COMPLETED
- [x] Define command spec declaration
- [x] Declare registration function

#### `src/apps/ftp/cmd_ftp.c` ✅ COMPLETED
- [x] Implement `ftp_args_t` argument structure with:
  - `-h, --help` - Show usage
  - `-H, --host HOST` - Server hostname (default localhost)
  - `-p, --port PORT` - Server port (default 21021)
  - `-u, --user USER` - Username (default anonymous)
  - `--json` - JSON output for operations

- [x] Implement `build_ftp_argtable()` - Build argument definitions
- [x] Implement `ftp_print_usage()` - Print usage with argtable3
- [x] Implement `ftp_run()` - Parse arguments, connect, and enter interactive mode
- [x] Define `cmd_ftp_spec` with `.type = CMD_EXTERNAL`

### 3.2 FTP Client Core ✅ COMPLETED

#### `src/apps/ftp/ftp_client.h` ✅ COMPLETED
- [x] Define `ftp_session_t` structure with:
  - Control connection socket
  - Data port listening socket
  - Response buffer and code
  - Connection state flags

#### `src/apps/ftp/ftp_client.c` ✅ COMPLETED
- [x] Implement `ftp_session_init()` - Initialize session structure
- [x] Implement `ftp_connect()` - Connect to server, read welcome
- [x] Implement `ftp_login()` - Send USER command
- [x] Implement `ftp_quit()` - Send QUIT and close
- [x] Implement `ftp_close()` - Close without QUIT
- [x] Implement `ftp_command()` - Send raw command
- [x] Implement `ftp_list()` - Get directory listing via PORT/LIST
- [x] Implement `ftp_get()` - Download file via PORT/RETR
- [x] Implement `ftp_put()` - Upload file via PORT/STOR
- [x] Implement `ftp_mkdir()` - Create directory via MKD
- [x] Implement `ftp_pwd()` - Get current directory via PWD
- [x] Implement `ftp_cd()` - Change directory via CWD
- [x] Implement `ftp_last_response()` - Get last response string
- [x] Implement `ftp_last_code()` - Get last response code
- [x] Implement helper functions for PORT mode data transfers

### 3.3 Interactive Mode ✅ COMPLETED

#### `src/apps/ftp/ftp_interactive.h` ✅ COMPLETED
- [x] Declare `ftp_interactive()` function

#### `src/apps/ftp/ftp_interactive.c` ✅ COMPLETED
- [x] Implement `ftp_interactive()` - Main command loop
- [x] Implement command handlers:
  - `ls [path]` - List directory contents
  - `cd <path>` - Change directory
  - `pwd` - Print working directory
  - `get <remote> [local]` - Download file
  - `put <local> [remote]` - Upload file
  - `mkdir <dir>` - Create directory
  - `help` - Show available commands
  - `quit/exit` - Disconnect and exit
- [x] Implement JSON output for all commands
- [x] Implement human-readable output format

### 3.4 Client Main and Build ✅ COMPLETED

#### `src/apps/ftp/ftp_main.c` ✅ COMPLETED
- [x] Implement main() wrapper that calls cmd_ftp_spec.run()

#### `src/apps/ftp/Makefile` ✅ COMPLETED
- [x] Create Makefile following standard app template
- [x] Support both source and installed build modes
- [x] Include pkg.mk for package building

#### `src/apps/ftp/pkg.json` ✅ COMPLETED
- [x] Create package metadata file with commands definition

#### `src/apps/ftp/pkg.mk` ✅ COMPLETED
- [x] Create package building rules

### 3.5 Register in Build System ✅ COMPLETED

- [x] Update `Makefile` (root): Add `ftp` to `APP_DIRS`
- [x] FTP client registered as package (installed via pkg_registry dynamically)

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

### 5.2 Client Tests (`tests/apps/ftp/test_ftp.py`) ✅ COMPLETED

#### Test Cases Implemented (30 tests, all passing)
- [x] `test_help_short()` - -h shows help
- [x] `test_help_long()` - --help shows help
- [x] `test_connect_default_host()` - Connects to localhost (default)
- [x] `test_connect_explicit_host()` - Connects with explicit host argument
- [x] `test_connect_json_output()` - JSON output on connection
- [x] `test_connect_bad_port()` - Handles invalid port gracefully
- [x] `test_pwd_command()` - pwd shows working directory
- [x] `test_pwd_json()` - pwd with JSON output
- [x] `test_ls_command()` - ls lists directory contents
- [x] `test_ls_json()` - ls with JSON output
- [x] `test_ls_subdir()` - ls with subdirectory path
- [x] `test_cd_command()` - cd changes directory
- [x] `test_cd_json()` - cd with JSON output
- [x] `test_cd_missing_arg()` - cd without argument shows error
- [x] `test_help_interactive()` - help shows commands
- [x] `test_help_interactive_json()` - help with JSON output
- [x] `test_get_command()` - get downloads file
- [x] `test_get_json()` - get with JSON output
- [x] `test_get_missing_arg()` - get without argument shows error
- [x] `test_put_command()` - put uploads file
- [x] `test_put_json()` - put with JSON output
- [x] `test_put_missing_arg()` - put without argument shows error
- [x] `test_mkdir_command()` - mkdir creates directory
- [x] `test_mkdir_json()` - mkdir with JSON output
- [x] `test_mkdir_missing_arg()` - mkdir without argument shows error
- [x] `test_unknown_command()` - unknown command shows error
- [x] `test_unknown_command_json()` - unknown command with JSON
- [x] `test_quit_command()` - quit exits cleanly
- [x] `test_exit_command()` - exit also exits cleanly
- [x] `test_quit_json()` - quit with JSON output

### 5.3 Update Tests Makefile ✅ COMPLETED

- [x] Add `ftpd` target to `tests/Makefile`
- [x] Add `tests/ftpd/__init__.py`
- [x] Add `tests/apps/ftp/__init__.py`
- [x] Add `tests/apps/ftp/test_ftp.py` with 30 tests

---

## Phase 6: Documentation ✅ COMPLETED

### 6.1 Doxygen Style Docstrings ✅ COMPLETED

All functions have docstrings in Doxygen format.

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

#### Client Files ✅ COMPLETED
- [x] `src/apps/ftp/cmd_ftp.h` - Command spec
- [x] `src/apps/ftp/cmd_ftp.c` - Command implementation
- [x] `src/apps/ftp/ftp_client.h` - Client session API
- [x] `src/apps/ftp/ftp_client.c` - Client session implementation
- [x] `src/apps/ftp/ftp_interactive.h` - Interactive mode API
- [x] `src/apps/ftp/ftp_interactive.c` - Interactive mode implementation

---

## Progress Summary

### Completed
- ✅ Phase 1: Project Setup and Infrastructure
- ✅ Phase 2: FTP Server Implementation
- ✅ Phase 3: FTP Client Implementation
- ✅ Phase 4: FTP Protocol Response Codes
- ✅ Phase 5.1: Server Tests (22 tests passing)
- ✅ Phase 5.2: Client Tests (30 tests passing)
- ✅ Phase 5.3: Tests Makefile
- ✅ Phase 6: Documentation (server and client)

### All Phases Complete

### Git Commits
1. `efd8bf4` - feat(ftpd): implement FTP server daemon
2. `bbce2a4` - test(ftpd): add comprehensive FTP server tests
3. `a94d7b2` - docs: update AI_TODO_7.md with FTP server progress
4. `382bf64` - feat(ftp): implement FTP client

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
