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

# Extract version from pkg.json if not provided
PKG_VERSION ?= $(shell grep -o '"version"[[:space:]]*:[[:space:]]*"[^"]*"' \
                 $(PKG_JSON) 2>/dev/null | \
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
