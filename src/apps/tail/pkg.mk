# Shared package building rules for jshell apps
# Include this in each app's Makefile after defining:
#   PKG_BIN     - path to built binary (required)
#
# Optional overrides:
#   PKG_NAME    - package name (defaults to current directory name)
#   PKG_VERSION - version string (read from pkg.json if not set)

PROJECT_ROOT ?= ../../..
PKG_REPO_DIR := $(PROJECT_ROOT)/srv/pkg_repository/downloads
PKG_NAME ?= $(notdir $(CURDIR))
PKG_JSON := pkg.json
PKG_STAGING := .pkg-staging

# Dependencies paths (for bundling into package)
PKG_ARGTABLE_DIR := $(PROJECT_ROOT)/extern/argtable3/dist
PKG_JSHELL_DIR := $(PROJECT_ROOT)/src/jshell
PKG_UTILS_DIR := $(PROJECT_ROOT)/src/utils

# Extract version from pkg.json if not provided
PKG_VERSION ?= $(shell grep -o '"version"[[:space:]]*:[[:space:]]*"[^"]*"' \
                 $(PKG_JSON) 2>/dev/null | \
                 sed 's/.*"\([^"]*\)"$$/\1/' || echo "0.0.0")

PKG_TARBALL := $(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_DEST := $(PKG_REPO_DIR)/$(PKG_TARBALL)

.PHONY: pkg pkg-clean pkg-info

pkg: $(PKG_BIN) $(PKG_JSON)
	@echo "Building package $(PKG_NAME)-$(PKG_VERSION)..."
	@rm -rf $(PKG_STAGING)
	@mkdir -p $(PKG_STAGING)/bin
	@mkdir -p $(PKG_STAGING)/deps/jshell
	@mkdir -p $(PKG_STAGING)/deps/utils
	@cp $(PKG_BIN) $(PKG_STAGING)/bin/$(PKG_NAME)
	@cp $(PKG_JSON) $(PKG_STAGING)/
	@cp Makefile $(PKG_STAGING)/ 2>/dev/null || true
	@cp pkg.mk $(PKG_STAGING)/ 2>/dev/null || true
	@cp *.c $(PKG_STAGING)/ 2>/dev/null || true
	@cp *.h $(PKG_STAGING)/ 2>/dev/null || true
	@cp *.a $(PKG_STAGING)/ 2>/dev/null || true
	@# Bundle argtable3 dependencies
	@cp $(PKG_ARGTABLE_DIR)/argtable3.h $(PKG_STAGING)/deps/
	@cp $(PKG_ARGTABLE_DIR)/argtable3.c $(PKG_STAGING)/deps/
	@cp $(PKG_ARGTABLE_DIR)/argtable3.o $(PKG_STAGING)/deps/ 2>/dev/null || true
	@# Bundle jshell dependencies
	@cp $(PKG_JSHELL_DIR)/jshell_cmd_registry.h $(PKG_STAGING)/deps/jshell/
	@cp $(PKG_JSHELL_DIR)/jshell_cmd_registry.c $(PKG_STAGING)/deps/jshell/
	@# Bundle utils dependencies
	@cp $(PKG_UTILS_DIR)/jbox_signals.h $(PKG_STAGING)/deps/utils/
	@cp $(PKG_UTILS_DIR)/jbox_signals.c $(PKG_STAGING)/deps/utils/
	@mkdir -p $(PKG_REPO_DIR)
	@tar -czf $(PKG_DEST) -C $(PKG_STAGING) .
	@rm -rf $(PKG_STAGING)
	@echo "  Created: $(PKG_DEST)"
	@echo "  Bundled deps: argtable3, jshell_cmd_registry, jbox_signals"

pkg-clean:
	rm -rf $(PKG_STAGING)
	rm -f $(PKG_DEST)

pkg-info:
	@echo "Package: $(PKG_NAME)"
	@echo "Version: $(PKG_VERSION)"
	@echo "Binary:  $(PKG_BIN)"
	@echo "Output:  $(PKG_DEST)"
