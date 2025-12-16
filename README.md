# jbox

![jbox](City_Box.png)

A custom UNIX shell implementation with CLI tools designed for AI agent integration. jbox provides a complete shell environment with built-in commands, external applications, a package manager, FTP server/client, and AI assistance features.

## Features

### Shell (jshell)

- **BNFC-generated parser** - Robust grammar-based command parsing
- **Pipeline support** - Chain commands with `|`
- **I/O redirection** - Input (`<`) and output (`>`) redirection
- **Background jobs** - Run commands in background with `&`
- **Variable expansion** - `$VAR`, `${VAR}`, command substitution `$(cmd)`
- **Glob expansion** - Wildcards `*`, `?`, `[a-z]`
- **Job control** - `jobs`, `kill`, `wait` commands
- **Command history** - Persistent command history
- **AI integration** - `@query` for chat, `@!query` for command generation

### Built-in Commands

| Command | Description |
|---------|-------------|
| `cd` | Change directory |
| `pwd` | Print working directory |
| `env` | List environment variables |
| `export` | Set environment variable |
| `unset` | Unset environment variable |
| `jobs` | List background jobs |
| `ps` | List processes |
| `kill` | Send signal to process |
| `wait` | Wait for job completion |
| `type` | Show command type |
| `help` | Display help |
| `history` | Show command history |
| `http-get` | HTTP GET request |
| `http-post` | HTTP POST request |
| `edit-replace-line` | Replace a line in a file |
| `edit-insert-line` | Insert a line in a file |
| `edit-delete-line` | Delete a line from a file |
| `edit-replace` | Find/replace with regex |

### External Applications

All external apps support `-h`/`--help` and `--json` output for agent integration.

| Command | Description |
|---------|-------------|
| `ls` | List directory contents |
| `cat` | Print file contents |
| `head` | View start of file |
| `tail` | View end of file |
| `stat` | File metadata |
| `cp` | Copy files/directories |
| `mv` | Move/rename files |
| `rm` | Remove files/directories |
| `mkdir` | Create directories |
| `rmdir` | Remove empty directories |
| `touch` | Create/update file timestamps |
| `rg` | Regex search (ripgrep-like) |
| `echo` | Print text |
| `sleep` | Delay execution |
| `date` | Show system time |
| `less` | Pager for viewing text |
| `vi` | VI-like text editor |
| `ftp` | FTP client |
| `pkg` | Package manager |

### Package Manager (pkg)

- `pkg list` - List installed packages
- `pkg info <name>` - Show package details
- `pkg search <name>` - Search package registry
- `pkg install <tarball>` - Install a package
- `pkg remove <name>` - Remove a package
- `pkg build [dir]` - Build package tarball
- `pkg check-update` - Check for updates
- `pkg upgrade` - Upgrade packages
- `pkg compile [name]` - Compile installed packages

### FTP Server (ftpd)

A standalone FTP server daemon supporting:
- USER, QUIT, PWD, CWD, LIST, RETR, STOR, MKD, TYPE, SYST, NOOP commands
- PORT mode data transfers
- Multi-threaded client handling
- Path security (chroot-like containment)

### AI Integration

Set `ANTHROPIC_API_KEY` or `GEMINI_API_KEY` in `~/.jshell/env`:

```bash
# Chat with AI
@what is the capital of France?

# Generate and execute commands
@!list all files modified today
```

## Building

### Prerequisites

- GCC with C23 support
- CMake (for libcurl)
- BNFC (BNF Converter) - for grammar regeneration
- Python 3 - for tests
- OpenSSL, libidn2 - for HTTP support
- Node.js/npm - for package server (optional)

### Build Commands

```bash
# Build everything (shell, apps, packages, FTP server)
make all

# Build only the shell
make jbox

# Build standalone app binaries
make apps

# Build FTP server
make ftpd

# Build package tarballs for all apps
make packages

# Regenerate parser from grammar (only if Grammar.cf changed)
make bnfc

# Build libcurl (if needed)
make curl-build

# Clean all build artifacts
make clean
```

> **Note:** Add `DEBUG=1` to any make command (e.g., `make jbox DEBUG=1`) to enable debug mode, which prints additional diagnostic information in the shell.

### Build Targets

| Target | Description |
|--------|-------------|
| `make all` | Build shell, apps, packages, and ftpd |
| `make jbox` | Build the shell binary |
| `make apps` | Build all standalone app binaries |
| `make packages` | Build package tarballs for each app |
| `make ftpd` | Build FTP server daemon |
| `make bnfc` | Regenerate parser from grammar |
| `make clean` | Remove all build artifacts |
| `make clean-apps` | Clean app build artifacts |
| `make clean-packages` | Clean package build artifacts |
| `make clean-pkg-repository` | Clean package repository |
| `make clean-ftpd` | Clean FTP server binary |

## Running

### Interactive Shell

```bash
# Run the shell
./bin/jbox

# Or use the symlink
./bin/jshell
```

### Execute Commands

```bash
# Execute a single command
./bin/jshell -c "ls -la"

# Execute multiple commands
./bin/jshell -c "echo hello; pwd; ls"
```

### Standalone Apps

Apps are built to `bin/standalone-apps/`:

```bash
./bin/standalone-apps/ls -la
./bin/standalone-apps/cat README.md
./bin/standalone-apps/rg --json "pattern" file.txt
```

## Testing

### Run All Tests

```bash
# Run all tests
make test

# Or from the tests directory
make -C tests all
```

### Specific Test Targets

```bash
# App tests
make -C tests apps          # All app tests
make -C tests ls            # ls tests
make -C tests cat           # cat tests
make -C tests rg            # rg tests
make -C tests vi            # vi tests
make -C tests less          # less tests

# Builtin command tests
make -C tests builtins      # All builtin tests
make -C tests edit-replace  # edit-replace tests

# Shell system tests
make -C tests jshell        # All shell tests
make -C tests jshell-path   # Path resolution tests
make -C tests jshell-pipes  # Pipe tests
make -C tests jshell-signals # Signal handling tests

# Signal handling tests
make -C tests signals       # All signal tests
make -C tests app-signals   # App signal tests

# Package manager tests
make -C tests pkg           # All pkg tests
make -C tests pkg-lifecycle # Lifecycle tests

# FTP server tests
make -C tests ftpd          # FTP server tests

# Grammar tests
make test-grammar
```

### Run Individual Test Files

```bash
# Run a specific test file
python -m unittest tests.apps.ls.test_ls -v

# Run a specific test case
python -m unittest tests.apps.ls.test_ls.TestLs.test_basic -v
```

## Package Server

The package server provides a registry for distributing packages.

### Start Server

```bash
# Start the package registry server (runs on http://localhost:3000)
./scripts/start-pkg-server.sh
```

### Stop Server

```bash
./scripts/shutdown-pkg-server.sh
```

### Build and Publish Packages

```bash
# Build all packages
./scripts/build-packages.sh

# Or via make
make packages

# Generate manifest
./scripts/generate-pkg-manifest.sh
```

Package tarballs are created in `srv/pkg_repository/downloads/`.

## FTP Server

### Start FTP Server

```bash
# Start with defaults (port 21021, root srv/ftp)
./bin/ftpd

# Custom port and root
./bin/ftpd -p 2121 -r /path/to/ftp/root
```

### FTP Server Options

```
-h, --help          Show help
-p, --port PORT     Listen port (default: 21021)
-r, --root DIR      Root directory (default: srv/ftp)
```

### FTP Client

```bash
# Connect to FTP server
./bin/standalone-apps/ftp -H localhost -p 21021

# With JSON output
./bin/standalone-apps/ftp --json -H localhost -p 21021
```

Interactive FTP commands:
- `ls [path]` - List directory
- `cd <path>` - Change directory
- `pwd` - Print working directory
- `get <remote> [local]` - Download file
- `put <local> [remote]` - Upload file
- `mkdir <dir>` - Create directory
- `help` - Show commands
- `quit` / `exit` - Disconnect

## Project Structure

```
jbox/
├── bin/                    # Compiled binaries
│   ├── jbox               # Main shell binary
│   ├── jshell             # Symlink to jbox
│   ├── ftpd               # FTP server daemon
│   └── standalone-apps/   # Individual command binaries
├── src/
│   ├── jbox.c             # Main entry point
│   ├── jshell/            # Shell implementation
│   │   ├── jshell.c       # Shell main loop
│   │   ├── jshell_cmd_registry.c  # Command registry
│   │   ├── jshell_job_control.c   # Job management
│   │   ├── jshell_history.c       # Command history
│   │   ├── jshell_path.c          # PATH handling
│   │   ├── jshell_signals.c       # Signal handling
│   │   ├── jshell_ai.c            # AI integration
│   │   └── builtins/      # Built-in commands
│   ├── apps/              # External applications
│   │   ├── ls/            # ls command
│   │   ├── cat/           # cat command
│   │   ├── pkg/           # Package manager
│   │   ├── ftp/           # FTP client
│   │   └── ...            # Other apps
│   ├── ftpd/              # FTP server daemon
│   ├── ast/               # AST interpreter
│   ├── shell-grammar/     # BNFC grammar definition
│   └── utils/             # Utility functions
├── gen/
│   └── bnfc/              # Generated parser code
├── extern/
│   ├── argtable3/         # Argument parsing library
│   └── curl/              # HTTP library
├── tests/                 # Python unittest suite
│   ├── apps/              # App tests
│   ├── jshell/            # Shell tests
│   ├── pkg/               # Package manager tests
│   └── ftpd/              # FTP server tests
├── scripts/               # Build and utility scripts
├── srv/
│   ├── pkg_repository/    # Package repository storage
│   └── ftp/               # FTP server root directory
├── ai/                    # AI planning documentation
│   └── plans/             # Implementation plans
└── Makefile               # Main build file
```

## Configuration

### Shell Environment

Create `~/.jshell/env` for environment variables loaded at startup:

```bash
ANTHROPIC_API_KEY=sk-ant-...
GEMINI_API_KEY=...
PATH=$HOME/.jshell/bin:$PATH
```

### Package Installation Directory

- Packages install to: `~/.jshell/pkgs/<name>-<version>/`
- Executables symlinked to: `~/.jshell/bin/`
- Package database: `~/.jshell/pkgs/pkgdb.json`

## Code Conventions

- **Standard**: GNU C23
- **Indentation**: 2 spaces
- **Line limit**: 80 characters
- **Naming**:
  - Functions/variables: `snake_case`
  - Structs/types: `PascalCase`
  - Constants: `ALL_CAPS`
- **CLI parsing**: argtable3
- **Agent commands**: Support `--json` output

## Dependencies

| Library | Purpose |
|---------|---------|
| argtable3 | CLI argument parsing |
| libcurl | HTTP requests |
| OpenSSL | TLS for HTTP |
| libidn2 | Internationalized domain names |
| pthreads | Threading |
| BNFC | Parser generation (build-time) |

## Implementation Status

The following major components have been implemented:

### Phase 1: Core Infrastructure
- Command registry system
- Builtin and external command registration
- AST interpreter and execution helpers

### Phase 2: Filesystem Tools
- ls, stat, cat, head, tail, cp, mv, rm, mkdir, rmdir, touch

### Phase 3: Search and Text Tools
- rg (regex search with POSIX regex)

### Phase 4: Structured Editing Commands
- edit-replace-line, edit-insert-line, edit-delete-line, edit-replace

### Phase 5: Process and Job Control
- jobs, ps, kill, wait

### Phase 6: Shell and Environment
- cd, pwd, env, export, unset, type, help, history

### Phase 7: Human-Facing Tools
- echo, sleep, date, less, vi

### Phase 8: Networking
- http-get, http-post (using libcurl)

### Phase 9: Package Manager
- Full pkg command with JSON database, dynamic command registration

### Shell Execution Engine
- Command path resolution (`~/.jshell/bin` priority)
- Threaded builtin execution
- Socketpair pipes for builtins
- Complete AST execution with pipes, redirection, background jobs
- Signal handling (SIGINT, SIGTERM, SIGPIPE, etc.)

### AI Integration
- Anthropic/Gemini API integration
- `@query` chat mode
- `@!query` command generation mode

### FTP Server/Client
- Complete FTP server daemon (ftpd)
- Interactive FTP client

## Documentation

Additional documentation is available in the `ai/` directory:

- `ai/CLItools.md` - CLI tools specification
- `ai/APPANATOMY.md` - Command anatomy and structure
- `ai/CONVENTIONS.md` - Code style conventions
- `ai/plans/` - Implementation plans and task tracking

## License

This project is for educational purposes.
