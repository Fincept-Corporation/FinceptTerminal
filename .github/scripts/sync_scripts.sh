#!/usr/bin/env bash
# Copy the repo's fincept-qt/scripts/ tree to a target directory, excluding
# dev-only junk that must never ship (it bloats installers and can cause
# macOS codesign to reject bundles with "unrecognized" members).
#
# Usage: sync_scripts.sh <source-dir> <dest-dir>
#
# Exclusions (kept in sync with fincept-qt/cmake/prune_scripts_junk.cmake
# and the install(DIRECTORY ... REGEX EXCLUDE) rule in CMakeLists.txt):
#   __pycache__, *.pyc, .pytest_cache, .benchmarks,
#   *.deleted.*, *.bak, *.orig,
#   *.db, *.sqlite, *.sqlite3,
#   .DS_Store, Thumbs.db

set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <source-dir> <dest-dir>" >&2
    exit 2
fi

SRC="$1"
DEST="$2"

if [ ! -d "$SRC" ]; then
    echo "::warning::sync_scripts: source directory not found: $SRC" >&2
    exit 0
fi

mkdir -p "$DEST"

# rsync is present on ubuntu-latest and macos-latest by default. On Windows
# runners it's in git-bash (which is what `shell: bash` uses).
rsync -a \
    --exclude='__pycache__' \
    --exclude='*.pyc' \
    --exclude='.pytest_cache' \
    --exclude='.benchmarks' \
    --exclude='*.deleted.*' \
    --exclude='*.deleted' \
    --exclude='*.bak' \
    --exclude='*.orig' \
    --exclude='*.db' \
    --exclude='*.sqlite' \
    --exclude='*.sqlite3' \
    --exclude='.DS_Store' \
    --exclude='Thumbs.db' \
    "${SRC%/}/" "${DEST%/}/"
