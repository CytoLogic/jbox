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
