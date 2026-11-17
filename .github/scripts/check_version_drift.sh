#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# check_version_drift.sh — assert the packaging metadata agrees with CMake.
#
# The single source of truth is fincept-qt/CMakeLists.txt:
#     project(FinceptTerminal VERSION X.Y.Z ...)
# The release workflows parse it correctly, but every file under
# fincept-qt/packaging/ carries an independent hand-written copy (flatpak
# manifest + metainfo, the .deb's appdata). A stale
# copy ships a bundle whose advertised version disagrees with the binary, which
# breaks the auto-updater's version comparison.
#
# RULE: a packaging file that mentions any X.Y.Z version at all must mention
# the CURRENT one somewhere. Files legitimately list *historical* releases
# (<release version="4.1.0"> …), so "contains no other version" would be wrong;
# "contains the current version" catches the file-stuck-on-the-old-release case
# without false-positiving on changelog history.
#
# RULE 2 (AppStream): the newest *uncommented* <release> of every
# *.metainfo.xml / *.appdata.xml must BE the current version. Rule 1 is
# satisfied by a comment or a commented-out draft; this one is not. It is the
# version Flatpak (`flatpak list`), software centres and the .deb's
# metadata report for a build of this tree.
#
# EXEMPT: the Flathub-form Flatpak manifest. Its main source pins the last
# release tag that builds in the sandbox, which lags this version by design (a
# commit cannot pin its own hash), so rule 1 would fail it on every version
# bump. The [manifest-pin] checks of release.yml's build-flatpak job ("Version
# + metadata consistency") govern it, and that job builds the checkout instead
# of the pin.
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
FLATHUB_MANIFEST="${PKG_DIR}/flatpak/in.fincept.FinceptTerminal.yml"   # see EXEMPT above
while IFS= read -r f; do
    [ "$f" = "$FLATHUB_MANIFEST" ] && continue
    grep -Iq . "$f" 2>/dev/null || continue                       # skip binaries
    versions=$(grep -Eo '[0-9]+\.[0-9]+\.[0-9]+' "$f" 2>/dev/null | sort -u)
    [ -n "$versions" ] || continue                                # no version at all
    if ! printf '%s\n' "$versions" | grep -qx "$VERSION"; then
        echo "::error::version drift in ${f#"$ROOT"/}: mentions $(printf '%s' "$versions" | tr '\n' ' ') but never ${VERSION}"
        DRIFT=1
    fi
done < <(find "$PKG_DIR" -type f \
            \( -name '*.xml' -o -name '*.yml' -o -name '*.yaml' -o -name '*.json' \
               -o -name '*.desktop' -o -name '*.plist' \))

# RULE 2 — parsed as XML, so commented-out <release> drafts do not count.
# Probe by running it: on Windows `python3` can be a Store alias that only
# prints an install hint.
PYTHON=""
for _py in python3 python; do
    if "$_py" -c 'import xml.dom.minidom' >/dev/null 2>&1; then PYTHON="$_py"; break; fi
done
while IFS= read -r f; do
    if [ -z "$PYTHON" ]; then
        echo "::error::python3 not found — cannot check the AppStream <release> list of ${f#"$ROOT"/}"
        DRIFT=1
        continue
    fi
    if ! newest=$("$PYTHON" - "$f" <<'PY'
import re, sys, xml.dom.minidom
rels = [r.getAttribute("version")
        for r in xml.dom.minidom.parse(sys.argv[1]).getElementsByTagName("release")]
key = lambda v: tuple(int(x) for x in re.findall(r"\d+", v))
print(max(rels, key=key) if rels else "")
PY
    ); then
        echo "::error::could not read the <release> list of ${f#"$ROOT"/} (Python error above)"
        DRIFT=1
        continue
    fi
    newest="${newest%$'\r'}"
    [ -n "$newest" ] || continue                                  # no <releases> at all
    if [ "$newest" != "$VERSION" ]; then
        echo "::error::version drift in ${f#"$ROOT"/}: newest <release> is ${newest}, CMake project version is ${VERSION} — add <release version=\"${VERSION}\" date=\"YYYY-MM-DD\"> at the top of <releases> (that entry is the version a build of this tree reports)"
        DRIFT=1
    else
        echo "AppStream ${f#"$ROOT"/}: newest <release> is ${newest}"
    fi
done < <(find "$PKG_DIR" -type f \( -name '*.metainfo.xml' -o -name '*.appdata.xml' \))

if [ "$DRIFT" -ne 0 ]; then
    echo "::error::packaging metadata is out of sync with CMakeLists.txt (${VERSION}). Update the files listed above."
    exit 1
fi
echo "Packaging metadata is consistent with ${VERSION}."
