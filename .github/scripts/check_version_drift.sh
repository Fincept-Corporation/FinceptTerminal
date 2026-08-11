#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# check_version_drift.sh — assert the packaging metadata agrees with CMake.
#
# The single source of truth is fincept-qt/CMakeLists.txt:
#     project(FinceptTerminal VERSION X.Y.Z ...)
# The release workflows parse it correctly, but every file under
# fincept-qt/packaging/ carries an independent hand-written copy (flatpak
# manifest + metainfo, AppImageBuilder, appdata, installer licence). A stale
# copy ships a bundle whose advertised version disagrees with the binary, which
# breaks the auto-updater's version comparison.
#
# RULE: a packaging file that mentions any X.Y.Z version at all must mention
# the CURRENT one somewhere. Files legitimately list *historical* releases
# (<release version="4.1.0"> …), so "contains no other version" would be wrong;
# "contains the current version" catches the file-stuck-on-the-old-release case
# without false-positiving on changelog history.
#
# Usage: check_version_drift.sh [repo-root]   (default: cwd)
# ─────────────────────────────────────────────────────────────────────────────
set -uo pipefail

ROOT="${1:-.}"
CMAKELISTS="${ROOT}/fincept-qt/CMakeLists.txt"
PKG_DIR="${ROOT}/fincept-qt/packaging"

if [ ! -f "$CMAKELISTS" ]; then
    echo "::error::${CMAKELISTS} not found"
    exit 1
fi

VERSION=$(grep -Eo 'project\(FinceptTerminal VERSION [0-9]+\.[0-9]+\.[0-9]+' "$CMAKELISTS" | awk '{print $3}')
if [ -z "$VERSION" ]; then
    echo "::error::could not parse project(FinceptTerminal VERSION ...) from ${CMAKELISTS}"
    exit 1
fi
echo "CMake project version: ${VERSION}"

if [ ! -d "$PKG_DIR" ]; then
    echo "::warning::${PKG_DIR} not found — nothing to check"
    exit 0
fi

# Shipped metadata only. Prose docs (*.md, licence text) are excluded: they
# quote unrelated versions (e.g. the bundled Python 3.11.9) and would
# false-positive without adding any signal the metadata files don't already
# carry.
DRIFT=0
while IFS= read -r f; do
    grep -Iq . "$f" 2>/dev/null || continue                       # skip binaries
    versions=$(grep -Eo '[0-9]+\.[0-9]+\.[0-9]+' "$f" 2>/dev/null | sort -u)
    [ -n "$versions" ] || continue                                # no version at all
    if ! printf '%s\n' "$versions" | grep -qx "$VERSION"; then
        echo "::error::version drift in ${f#"$ROOT"/}: mentions $(printf '%s' "$versions" | tr '\n' ' ') but never ${VERSION}"
        DRIFT=1
    fi
done < <(find "$PKG_DIR" -type f \
            \( -name '*.xml' -o -name '*.yml' -o -name '*.yaml' -o -name '*.json' \
               -o -name '*.desktop' -o -name '*.spec' -o -name '*.plist' \))

if [ "$DRIFT" -ne 0 ]; then
    echo "::error::packaging metadata is out of sync with CMakeLists.txt (${VERSION}). Update the files listed above."
    exit 1
fi
echo "Packaging metadata is consistent with ${VERSION}."
