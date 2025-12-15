#!/bin/bash
# Generate pkg_manifest.json from app pkg.json files

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
APPS_DIR="$PROJECT_ROOT/src/apps"
OUTPUT_FILE="$PROJECT_ROOT/srv/pkg_repository/pkg_manifest.json"
BASE_URL="${PKG_BASE_URL:-http://localhost:3000}"

# Ensure output directory exists
mkdir -p "$(dirname "$OUTPUT_FILE")"

# Collect all package entries
entries=()
for pkg_json in "$APPS_DIR"/*/pkg.json; do
    if [ -f "$pkg_json" ]; then
        name=$(grep -o '"name"[[:space:]]*:[[:space:]]*"[^"]*"' "$pkg_json" | sed 's/.*"\([^"]*\)"$/\1/')
        version=$(grep -o '"version"[[:space:]]*:[[:space:]]*"[^"]*"' "$pkg_json" | sed 's/.*"\([^"]*\)"$/\1/')
        description=$(grep -o '"description"[[:space:]]*:[[:space:]]*"[^"]*"' "$pkg_json" | sed 's/.*"\([^"]*\)"$/\1/')

        if [ -n "$name" ] && [ -n "$version" ]; then
            entries+=("$name|$version|$description")
        fi
    fi
done

# Write JSON file
{
    echo "["
    for i in "${!entries[@]}"; do
        IFS='|' read -r name version description <<< "${entries[$i]}"
        echo "  {"
        echo "    \"name\": \"$name\","
        echo "    \"latestVersion\": \"$version\","
        echo "    \"description\": \"$description\","
        echo "    \"downloadUrl\": \"$BASE_URL/downloads/$name-$version.tar.gz\""
        if [ $i -lt $((${#entries[@]} - 1)) ]; then
            echo "  },"
        else
            echo "  }"
        fi
    done
    echo "]"
} > "$OUTPUT_FILE"

echo "Generated $OUTPUT_FILE"
