# Package Registry Server Implementation Plan

## Overview

Implement a minimal Node.js + Express server acting as a package registry for jshell. This server provides the backend for `pkg search`, `pkg check-update`, and `pkg upgrade` commands.

**Prerequisites:** The pkg command skeleton and local operations were implemented in `AI_TODO_1.md` Phase 9.1 and `AI_TODO_2.md` Phases 1-8.

### Directory Structure

```
jbox/
├── src/
│   ├── pkg_srv/
│   │   ├── server.js           # Express server implementation
│   │   ├── package.json        # Node.js package config
│   │   └── node_modules/       # Dependencies (git-ignored)
│   └── apps/
│       ├── pkg.mk              # Shared Makefile rules for packaging
│       ├── ls/
│       │   ├── Makefile        # Includes pkg.mk
│       │   ├── pkg.json        # Package manifest
│       │   ├── cmd_ls.c
│       │   └── ...
│       ├── cat/
│       │   ├── Makefile
│       │   ├── pkg.json
│       │   └── ...
│       └── ...
├── srv/
│   └── pkg_repository/
│       └── downloads/          # Package tarballs (built, git-ignored)
│           ├── ls-1.0.0.tar.gz
│           ├── cat-1.0.0.tar.gz
│           └── ...
└── scripts/
    ├── start-pkg-server.sh     # Start the registry server
    └── build-packages.sh       # Build all packages (optional helper)
```

- **Server source:** `src/pkg_srv/server.js`
- **Package downloads:** `srv/pkg_repository/downloads/` (served at `/downloads/`)
- **Package sources:** `src/apps/*/` (existing app source code)
- **Package manifests:** `src/apps/*/pkg.json`
- **Shared package rules:** `src/apps/pkg.mk`
- **Startup script:** `scripts/start-pkg-server.sh`
- **Port:** 3000 (default)

### Data Model

Packages are stored in memory as an array of objects:

```json
{
  "name": "ls",
  "latestVersion": "1.0.0",
  "description": "List directory contents",
  "downloadUrl": "http://localhost:3000/downloads/ls-1.0.0.tar.gz"
}
```

### API Endpoints

| Method | Endpoint                      | Description                              |
|--------|-------------------------------|------------------------------------------|
| GET    | /packages                     | List all packages                        |
| GET    | /packages/:name               | Get package info (or 404)                |
| GET    | /downloads/:filename          | Download package tarball (static file)   |

---

## Phase 1: Server Setup ✅ COMPLETED

### 1.1 Initialize Node.js Project ✅ COMPLETED
Create the package registry server directory and initialize npm.

- [x] Create `src/pkg_srv/` directory
- [x] Run `npm init -y` in `src/pkg_srv/`
- [x] Run `npm install express` in `src/pkg_srv/`
- [x] Add `src/pkg_srv/node_modules/` to `.gitignore`

### 1.2 Create package.json ✅ COMPLETED
Create or update the generated `package.json` with proper metadata.

Expected `src/pkg_srv/package.json`:
```json
{
  "name": "jshell-pkg-registry",
  "version": "1.0.0",
  "description": "Package registry server for jshell",
  "main": "server.js",
  "scripts": {
    "start": "node server.js",
    "dev": "node server.js"
  },
  "keywords": ["jshell", "package", "registry"],
  "author": "",
  "license": "ISC",
  "dependencies": {
    "express": "^4.18.0"
  }
}
```

---

## Phase 2: Server Implementation ✅ COMPLETED

### 2.1 Create server.js ✅ COMPLETED
Implement the Express server with in-memory package storage.

- [x] Create `src/pkg_srv/server.js`:
  - [x] Import express and path
  - [x] Create express app
  - [x] Initialize in-memory packages array with example data
  - [x] Implement `GET /packages` endpoint
  - [x] Implement `GET /packages/:name` endpoint
  - [x] Serve static files from `downloads/` directory at `/downloads/` (completed in Phase 3.7)
  - [x] Start server on port 3000
  - [x] Add basic logging

Example package entries to include:
```javascript
const packages = [
  {
    name: "ls",
    latestVersion: "1.0.0",
    description: "List directory contents",
    downloadUrl: "http://localhost:3000/downloads/ls-1.0.0.tar.gz"
  },
  {
    name: "cat",
    latestVersion: "1.0.0",
    description: "Concatenate and print files",
    downloadUrl: "http://localhost:3000/downloads/cat-1.0.0.tar.gz"
  },
  {
    name: "rg",
    latestVersion: "1.0.0",
    description: "Search text with regex",
    downloadUrl: "http://localhost:3000/downloads/rg-1.0.0.tar.gz"
  }
  // ... add more apps as needed
];
```

### 2.2 API Response Formats

**GET /packages response:**
```json
{
  "status": "ok",
  "packages": [
    {
      "name": "ls",
      "latestVersion": "1.0.0",
      "description": "List directory contents",
      "downloadUrl": "http://localhost:3000/downloads/ls-1.0.0.tar.gz"
    }
  ]
}
```

**GET /packages/:name response (found):**
```json
{
  "status": "ok",
  "package": {
    "name": "ls",
    "latestVersion": "1.0.0",
    "description": "List directory contents",
    "downloadUrl": "http://localhost:3000/downloads/ls-1.0.0.tar.gz"
  }
}
```

**GET /packages/:name response (not found):**
```json
{
  "status": "error",
  "message": "Package 'nonexistent' not found"
}
```

---

## Phase 3: Package Build System ✅ COMPLETED

### 3.1 Create Repository Directory Structure ✅ COMPLETED
Set up the directory structure for serving package tarballs.

- [x] Create `srv/pkg_repository/downloads/` directory
- [x] Add `srv/pkg_repository/downloads/` to `.gitignore`

### 3.2 Create Shared Makefile Rules (pkg.mk) ✅ COMPLETED
Create shared Makefile rules that each app can include for package building.

- [x] Create `src/apps/pkg.mk`:
  ```makefile
  # Shared package building rules for jshell apps
  # Include this in each app's Makefile after defining:
  #   PKG_NAME    - package name (defaults to current directory name)
  #   PKG_BIN     - path to built binary (required)
  #   PKG_VERSION - version string (read from pkg.json if not set)

  PROJECT_ROOT ?= ../../..
  PKG_REPO_DIR := $(PROJECT_ROOT)/srv/pkg_repository/downloads
  PKG_NAME ?= $(notdir $(CURDIR))
  PKG_JSON := pkg.json
  PKG_STAGING := .pkg-staging

  # Extract version from pkg.json if not provided
  PKG_VERSION ?= $(shell grep -o '"version"[[:space:]]*:[[:space:]]*"[^"]*"' $(PKG_JSON) 2>/dev/null | \
                   sed 's/.*"\([^"]*\)"$$/\1/' || echo "0.0.0")

  PKG_TARBALL := $(PKG_NAME)-$(PKG_VERSION).tar.gz
  PKG_DEST := $(PKG_REPO_DIR)/$(PKG_TARBALL)

  .PHONY: pkg pkg-clean pkg-info

  pkg: $(PKG_BIN) $(PKG_JSON)
  	@echo "Building package $(PKG_NAME)-$(PKG_VERSION)..."
  	@mkdir -p $(PKG_STAGING)/bin
  	@cp $(PKG_BIN) $(PKG_STAGING)/bin/$(PKG_NAME)
  	@cp $(PKG_JSON) $(PKG_STAGING)/
  	@mkdir -p $(PKG_REPO_DIR)
  	@tar -czf $(PKG_DEST) -C $(PKG_STAGING) .
  	@rm -rf $(PKG_STAGING)
  	@echo "  Created: $(PKG_DEST)"

  pkg-clean:
  	rm -rf $(PKG_STAGING)
  	rm -f $(PKG_DEST)

  pkg-info:
  	@echo "Package: $(PKG_NAME)"
  	@echo "Version: $(PKG_VERSION)"
  	@echo "Binary:  $(PKG_BIN)"
  	@echo "Output:  $(PKG_DEST)"
  ```

### 3.3 Create pkg.json for Each App ✅ COMPLETED
Add package manifests to existing apps.

- [x] Create `src/apps/ls/pkg.json`
- [x] Create `src/apps/cat/pkg.json`
- [x] Create pkg.json for remaining apps (stat, head, tail, cp, mv, rm, mkdir, rmdir, touch, rg, echo, sleep, date, less, vi, pkg)

### 3.4 Update App Makefiles to Include pkg.mk ✅ COMPLETED
Add package building support to each app's Makefile.

- [x] Update `src/apps/ls/Makefile`:
  ```makefile
  # Add at top after existing variables:
  PKG_BIN = $(BIN)

  # Add at bottom:
  include ../pkg.mk
  ```
- [x] Update remaining app Makefiles similarly

### 3.5 Add Package Targets to Root Makefile ✅ COMPLETED
Add targets to build packages from the root Makefile.

- [x] Update root `Makefile`:
  ```makefile
  # Add to .PHONY:
  .PHONY: packages clean-packages

  # Package building
  PKG_APPS := ls cat stat head tail cp mv rm mkdir rmdir touch rg echo sleep date less vi

  packages: apps
  	@for app in $(PKG_APPS); do \
  		echo "Packaging $$app..."; \
  		$(MAKE) -C $(SRC_DIR)/apps/$$app pkg; \
  	done
  	@echo "All packages built to srv/pkg_repository/downloads/"

  clean-packages:
  	@for app in $(PKG_APPS); do \
  		$(MAKE) -C $(SRC_DIR)/apps/$$app pkg-clean; \
  	done
  	rm -rf srv/pkg_repository/downloads/*
  ```

### 3.6 Create Optional Build Script ✅ COMPLETED
Create a convenience script for building packages outside of make.

- [x] Create `scripts/build-packages.sh`:
  ```bash
  #!/bin/bash
  # Build all jshell app packages

  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

  cd "$PROJECT_ROOT" || exit 1

  echo "Building packages..."
  make packages

  echo ""
  echo "Packages available in srv/pkg_repository/downloads/:"
  ls -la srv/pkg_repository/downloads/*.tar.gz 2>/dev/null || echo "  (none built yet)"
  ```
- [x] Make script executable

### 3.7 Update server.js for Static File Serving ✅ COMPLETED
Update Express server to serve downloads from the new location.

- [x] Update `src/pkg_srv/server.js`:
  ```javascript
  const path = require('path');

  // Project root is three levels up from src/pkg_srv/
  const PROJECT_ROOT = path.join(__dirname, '..', '..');
  const DOWNLOADS_DIR = path.join(PROJECT_ROOT, 'srv', 'pkg_repository', 'downloads');

  // Serve package downloads
  app.use('/downloads', express.static(DOWNLOADS_DIR));
  ```

### 3.8 Update Server Package Registry ✅ COMPLETED
Update the server to dynamically read package info or maintain a registry.

- [x] Option A: Static registry in server.js (simple, current approach)
- [ ] Option B: Read pkg.json files from src/apps/ at startup (future enhancement)
- [ ] Option C: Scan tarballs in downloads/ directory (future enhancement)

Using Option A - manually updated packages array in server.js with all 18 apps.

---

## Phase 4: Development Script ✅ COMPLETED

### 4.1 Create Startup Script ✅ COMPLETED
Create a convenience script to start the server during development.

- [x] Create `scripts/` directory if it doesn't exist
- [x] Create `scripts/start-pkg-server.sh`:
  - [x] Navigate to `src/pkg_srv/`
  - [x] Check if `node_modules/` exists, run `npm install` if not
  - [x] Start the server with `npm start`
  - [x] Make script executable

Expected `scripts/start-pkg-server.sh`:
```bash
#!/bin/bash
# Start the jshell package registry server

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SERVER_DIR="$PROJECT_ROOT/src/pkg_srv"

cd "$SERVER_DIR" || exit 1

# Install dependencies if needed
if [ ! -d "node_modules" ]; then
    echo "Installing dependencies..."
    npm install
fi

echo "Starting package registry server on http://localhost:3000"
npm start
```

---

## Phase 5: Tests

### 5.1 Create Test Suite
Create Python unittest tests for the server API.

- [x] Create `tests/pkg_srv/` directory
- [x] Create `tests/pkg_srv/__init__.py`
- [x] Create `tests/pkg_srv/test_pkg_srv.py`:
  - [x] Test GET /packages returns 200 OK
  - [x] Test GET /packages returns valid JSON with status: ok
  - [x] Test GET /packages contains packages array
  - [x] Test packages have required fields (name, latestVersion, downloadUrl)
  - [x] Test GET /packages/:name returns 200 for existing package
  - [x] Test GET /packages/:name returns package object with correct data
  - [x] Test GET /packages/:name returns 404 for non-existent package
  - [x] Test 404 response has status: error and message
  - [x] Test edge cases (case sensitivity, special chars)
- [x] Update `tests/Makefile` with pkg-srv target

### 5.2 Run Tests
```bash
# Start server in one terminal
./scripts/start-pkg-server.sh

# Run tests in another terminal
make -C tests pkg-srv

# Or directly
python -m unittest tests.pkg_srv.test_pkg_srv -v
```

---

## Phase 6: Update .gitignore ✅ COMPLETED

### 6.1 Add Node.js and Build Artifact Ignores ✅ COMPLETED
Add Node.js-specific and build artifact entries to `.gitignore`.

- [x] Add `src/pkg_srv/node_modules/` to `.gitignore`
- [x] Add `src/pkg_srv/package-lock.json` to `.gitignore`
- [x] Add `srv/pkg_repository/downloads/` to `.gitignore` (built packages, not committed)
- [x] Add `src/apps/*/.pkg-staging/` to `.gitignore` (temp staging dirs)

---

## Phase 7: Integration with pkg Command ✅ COMPLETED

These tasks connect the registry server to the pkg command.

### 7.1 pkg search Implementation ✅ COMPLETED
- [x] Create `src/apps/pkg/pkg_registry.h`:
  - [x] Declare `PkgRegistryList *pkg_registry_search(const char *query);`
  - [x] Declare `PkgRegistryEntry *pkg_registry_fetch_package(const char *name);`
  - [x] Declare `int pkg_registry_download(const char *url, const char *dest_path);`
  - [x] Declare `int pkg_version_compare(const char *v1, const char *v2);`
- [x] Create `src/apps/pkg/pkg_registry.c`:
  - [x] Implement HTTP GET using libcurl
  - [x] Parse JSON response
  - [x] Implement `pkg_registry_search()`
  - [x] Implement `pkg_registry_fetch_package()`
  - [x] Implement `pkg_registry_download()`
- [x] Update `cmd_pkg.c` to use registry functions
- [x] Update Makefile to link with libcurl

### 7.2 pkg check-update Implementation ✅ COMPLETED
- [x] Implement `pkg_check_update()`:
  - [x] Load installed packages from database
  - [x] Query registry for each package
  - [x] Compare versions using `pkg_version_compare()`
  - [x] Report available updates
  - [x] Support `--json` output

### 7.3 pkg upgrade Implementation ✅ COMPLETED
- [x] Implement `pkg_upgrade()`:
  - [x] Check for updates
  - [x] Download new versions from registry
  - [x] Remove old versions
  - [x] Install upgrades
  - [x] Support `--json` output

### 7.4 Tests ✅ COMPLETED
- [x] Update `tests/apps/pkg/test_pkg.py`:
  - [x] Add `TestPkgRegistry` class with server startup/shutdown
  - [x] Test search functionality (returns results, JSON, no matches)
  - [x] Test check-update with installed packages
  - [x] Test upgrade with installed packages

---

## Usage

### Starting the Server

```bash
# From project root
./scripts/start-pkg-server.sh

# Or manually
cd src/pkg_srv
npm install  # first time only
npm start
```

### Testing the API

```bash
# List all packages
curl http://localhost:3000/packages

# Get specific package
curl http://localhost:3000/packages/ls

# Get non-existent package (returns 404)
curl http://localhost:3000/packages/nonexistent

# Download a package tarball
curl -O http://localhost:3000/downloads/ls-1.0.0.tar.gz
```

### Building Packages

```bash
# Build all app packages (from project root)
make packages

# Or use the helper script
./scripts/build-packages.sh

# Build a single package
make -C src/apps/ls pkg

# Verify packages were created
ls -la srv/pkg_repository/downloads/

# Get package info for an app
make -C src/apps/ls pkg-info
```

---

## Workflow Summary

### Complete Build and Serve Workflow

```bash
# 1. Build all apps (compiles binaries)
make apps

# 2. Build all packages (creates tarballs from binaries)
make packages

# 3. Start the registry server
./scripts/start-pkg-server.sh

# 4. Test package download
curl http://localhost:3000/packages/ls
curl -O http://localhost:3000/downloads/ls-1.0.0.tar.gz
```

### Adding a New App to the Registry

1. Create the app in `src/apps/<name>/` with standard Makefile
2. Add `pkg.json` manifest to the app directory
3. Add `PKG_BIN = $(BIN)` and `include ../pkg.mk` to the Makefile
4. Add the app name to `PKG_APPS` in the root Makefile
5. Add the package entry to the `packages` array in `server.js`
6. Run `make packages` to build

### Updating Package Versions

1. Update `version` in `src/apps/<name>/pkg.json`
2. Update `latestVersion` in `src/pkg_srv/server.js`
3. Run `make -C src/apps/<name> pkg` to rebuild

---

## Notes

### Port Configuration
The server listens on port 3000 by default. To use a different port, set the `PORT` environment variable:
```bash
PORT=8080 npm start
```

### Future Enhancements
- Add POST endpoint for publishing packages
- Add authentication for write operations
- Persist data to a JSON file or SQLite database
- Add version history per package
- Add search filtering (by name, description)
- Add package dependency resolution
