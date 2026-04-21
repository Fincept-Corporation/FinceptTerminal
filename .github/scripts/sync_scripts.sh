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
#   .venv, venv, env,
#   *.deleted.*, *.bak, *.orig,
#   *.db, *.sqlite, *.sqlite3,
#   .DS_Store, Thumbs.db,
#   .git, .firecrawl
#
# Implementation note: we use `tar --exclude ... | tar -x` instead of rsync
# because rsync is NOT pre-installed on GitHub's windows-2022 runner
# (git-bash omits it, MSYS2 is not on PATH). tar is available on all three
# runner OSes (Windows git-bash, Ubuntu, macOS) out of the box.

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

# Stream tar from SRC → DEST with exclusions applied. The trailing `.` in
# `tar -C "$SRC" -c .` packs SRC's contents (not SRC itself) so files land
# directly under DEST, matching rsync's `SRC/ DEST/` semantics.
tar \
    --exclude='__pycache__' \
    --exclude='*.pyc' \
    --exclude='.pytest_cache' \
    --exclude='.benchmarks' \
    --exclude='.venv' \
    --exclude='venv' \
    --exclude='env' \
    --exclude='*.deleted.*' \
    --exclude='*.deleted' \
    --exclude='*.bak' \
    --exclude='*.orig' \
    --exclude='*.db' \
    --exclude='*.sqlite' \
    --exclude='*.sqlite3' \
    --exclude='.DS_Store' \
    --exclude='Thumbs.db' \
    --exclude='.git' \
    --exclude='.firecrawl' \
    -C "$SRC" -cf - . | tar -C "$DEST" -xf -
