#!/usr/bin/env bash

set -euo pipefail

DIRECTORIES=("core" "net" "server")

# Qt and OpenGL headers prohibited in these modules
PATTERN='^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]((Qt[^/"]*/|Q[A-Z][A-Za-z0-9_]*([./"]|$)|GL/|OpenGL/))'

FAILED=0

for DIR in "${DIRECTORIES[@]}"; do
    if [[ ! -d "$DIR" ]]; then
        continue
    fi

    echo "Checking $DIR..."

    if grep -rnE "$PATTERN" "$DIR" \
        --include='*.h' \
        --include='*.hpp' \
        --include='*.cpp' \
        --include='*.cc' \
        --include='*.cxx'; then

        echo "ERROR: Prohibited Qt/OpenGL headers found in $DIR"
        FAILED=1
    fi
done

if [[ "$FAILED" -eq 1 ]]; then
    echo "Dependency boundary check FAILED."
    exit 1
fi

echo "Dependency boundary check PASSED."