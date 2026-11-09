#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# stage_linux_package.sh — build the install tree shared by the .deb and .rpm.
#
#   stage_linux_package.sh <appdir> <yt-dlp> <src-dir> <out-root> <depends-out>
#
#   <appdir>       linuxdeploy AppDir — the AppImage payload, with Qt bundled
#   <yt-dlp>       the pinned, hash-verified yt-dlp the build put beside the binary
#   <src-dir>      fincept-qt/ (desktop entry, AppStream file, icon, qt.conf)
#   <out-root>     staging root; recreated from scratch
#   <depends-out>  receives the computed Debian "Depends:" value
#
# WHY (#415)
#   The .deb/.rpm used to be `cmake --install --prefix /usr`: a bare
#   /usr/bin/FinceptTerminal linked against the CI runner's Qt 6.8.3, with no Qt
#   in the package and none in Depends. No distro ships Qt 6.8 (Ubuntu 22.04 has
#   6.2, 24.04 has 6.4, 26.04 has 6.10) and the binary uses Qt private headers,
#   which carry no ABI promise across minor versions — so the package could not
#   start anywhere:
#     error while loading shared libraries: libQt6Charts.so.6: cannot open ...
#   It also put yt-dlp in /usr/bin, colliding with the distro's yt-dlp package.
#
# LAYOUT
#   /opt/fincept-terminal/            the AppDir's usr/ tree, unchanged — the same
#                                     bytes the smoke-tested AppImage runs
#   /opt/fincept-terminal/bin/yt-dlp  private copy, never on $PATH
#   /usr/bin/FinceptTerminal          symlink to /opt/fincept-terminal/bin/...
#   /usr/share/{applications,metainfo,icons}  desktop integration
#   Libraries resolve through the binary's RUNPATH ($ORIGIN/../lib) and plugins
#   through bin/qt.conf. Both the loader and applicationDirPath() resolve the
#   /usr/bin symlink via /proc/self/exe, so $ORIGIN is the /opt directory.
#
# CHECKS — fail the release rather than ship a package that cannot start:
#   - every ELF resolves all of its libraries with LD_LIBRARY_PATH cleared
#     (install-qt-action puts the runner's Qt on it, which would mask a gap)
#   - no Qt library resolves from outside the package
#   - every library taken from the host belongs to a distro package; those
#     packages become Depends, with libc6 floored at the newest GLIBC symbol
#     version any shipped binary needs
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

die() { echo "::error::$*" >&2; exit 1; }

[ "$#" -eq 5 ] || die "usage: $0 <appdir> <yt-dlp> <src-dir> <out-root> <depends-out>"
APPDIR="$1"; YTDLP="$2"; SRC="$3"; DEPENDS_OUT="$5"

PREFIX="/opt/fincept-terminal"
ICON="${SRC}/resources/fincept.png"

[ -x "${APPDIR}/usr/bin/FinceptTerminal" ] \
    || die "${APPDIR}/usr/bin/FinceptTerminal missing — the AppImage step must run first"
[ -f "${YTDLP}" ] || die "bundled yt-dlp not found at ${YTDLP}"
[ -f "${ICON}" ] || die "${ICON} missing — the 'Generate Linux icon PNG' step must run first"

# Absolute paths throughout, so ldd reports canonical locations we can compare.
rm -rf "$4"
mkdir -p "$4"
ROOT="$(cd "$4" && pwd -P)"
DEST="${ROOT}${PREFIX}"
mkdir -p "${DEST}" "${ROOT}/usr/bin"

# ── 1. Payload ───────────────────────────────────────────────────────────────
# cp -a keeps linuxdeploy's relative symlinks and file modes.
cp -a "${APPDIR}/usr/." "${DEST}/"

# linuxdeploy-plugin-qt writes bin/qt.conf; keep it (the AppImage runs with it).
# Fall back to the IFW installer's copy only if it is missing.
if [ ! -f "${DEST}/bin/qt.conf" ]; then
    install -m 644 "${SRC}/packaging/linux/qt.conf" "${DEST}/bin/qt.conf"
fi

# VideoPlayerWidget looks for yt-dlp beside the binary first.
install -m 755 "${YTDLP}" "${DEST}/bin/yt-dlp"

# Complete uninstaller: removes the package and every user's-home leftover the
# package manager never touches (data, settings, caches). Ships as
# /opt/fincept-terminal/uninstall.sh; see packaging/uninstall/README.md.
install -m 755 "${SRC}/packaging/uninstall/uninstall.sh" "${DEST}/uninstall.sh"

# Absolute target: the link crosses top-level directories (Debian policy 10.5).
ln -s "${PREFIX}/bin/FinceptTerminal" "${ROOT}/usr/bin/FinceptTerminal"

install -D -m 644 "${SRC}/packaging/linux/fincept-terminal.desktop" \
    "${ROOT}/usr/share/applications/fincept-terminal.desktop"
install -D -m 644 "${SRC}/packaging/linux/fincept-terminal.appdata.xml" \
    "${ROOT}/usr/share/metainfo/fincept-terminal.appdata.xml"
install -D -m 644 "${ICON}" \
    "${ROOT}/usr/share/icons/hicolor/256x256/apps/fincept-terminal.png"

# ── 2. Runtime pieces that are loaded by path, not by DT_NEEDED ──────────────
# ldd cannot see these, so name them.
REQUIRED="bin/qt.conf bin/yt-dlp bin/resources bin/scripts
          plugins/platforms/libqxcb.so plugins/sqldrivers/libqsqlite.so plugins/tls/libqopensslbackend.so"
if [ -e "${DEST}/lib/libQt6WebEngineCore.so.6" ]; then
    REQUIRED="${REQUIRED} libexec/QtWebEngineProcess resources/icudtl.dat resources/qtwebengine_resources.pak"
fi
for req in ${REQUIRED}; do
    [ -e "${DEST}/${req}" ] || die "package tree is missing ${PREFIX}/${req}"
done

# ── 3. Library closure and Depends ───────────────────────────────────────────
WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT
: >"${WORK}/external"; : >"${WORK}/missing"; : >"${WORK}/qt_leaks"; : >"${WORK}/glibc"

elf_count=0
# scripts/ and resources/ hold Python and data files, not shipped native code.
while IFS= read -r -d '' f; do
    [ "$(head -c 4 "${f}" | od -An -tx1 | tr -d ' \n')" = "7f454c46" ] || continue
    elf_count=$((elf_count + 1))
    rel="${f#"${ROOT}"}"

    { readelf -V --wide "${f}" 2>/dev/null || true; } \
        | { grep -oE 'Name: GLIBC_[0-9]+(\.[0-9]+)+' || true; } \
        | sed 's/^Name: GLIBC_//' >>"${WORK}/glibc"

    while read -r soname arrow path _; do
        [ "${arrow}" = "=>" ] || continue
        if [ "${path}" = "not" ]; then
            echo "${soname} (needed by ${rel})" >>"${WORK}/missing"
            continue
        fi
        real="$(readlink -f "${path}")"
        case "${real}" in
            "${DEST}"/*) ;;
            *)
                case "${soname}" in
                    libQt6*) echo "${soname} -> ${real} (needed by ${rel})" >>"${WORK}/qt_leaks" ;;
                esac
                echo "${path}" >>"${WORK}/external"
                ;;
        esac
    done < <(env -u LD_LIBRARY_PATH ldd "${f}" 2>/dev/null || true)
done < <(find "${DEST}" \( -path "${DEST}/bin/scripts" -o -path "${DEST}/bin/resources" \) -prune \
              -o -type f -print0)

[ "${elf_count}" -gt 0 ] || die "no ELF files found under ${DEST}"
if [ -s "${WORK}/missing" ]; then
    sort -u "${WORK}/missing" >&2
    die "unresolved libraries in the package tree (listed above) — it would not start on a clean machine"
fi
if [ -s "${WORK}/qt_leaks" ]; then
    sort -u "${WORK}/qt_leaks" >&2
    die "Qt libraries resolve from outside the package (listed above) — the bundle is incomplete (#415)"
fi

# Owning package of a host library. dpkg records some files under /lib and some
# under /usr/lib (usrmerge), so try the path as given, canonical, and both forms.
owner_pkg() {
    local p c pkg
    for p in "$1" "$(readlink -f "$1")"; do
        for c in "${p}" "/usr${p}" "${p#/usr}"; do
            pkg="$(dpkg-query -S "${c}" 2>/dev/null \
                   | awk -F': ' '!/^diversion/ { split($1, a, ":"); print a[1]; exit }')" || true
            if [ -n "${pkg}" ]; then
                echo "${pkg}"
                return 0
            fi
        done
    done
    return 1
}

: >"${WORK}/pkgs"; : >"${WORK}/unowned"
while read -r lib; do
    [ -n "${lib}" ] || continue
    if pkg="$(owner_pkg "${lib}")"; then
        echo "${pkg}" >>"${WORK}/pkgs"
        echo "  host: ${lib} (${pkg})"
    else
        echo "${lib}" >>"${WORK}/unowned"
    fi
done < <(sort -u "${WORK}/external")

if [ -s "${WORK}/unowned" ]; then
    cat "${WORK}/unowned" >&2
    die "host libraries above belong to no distro package — users would not have them; bundle them instead"
fi

# libc6's floor comes from the symbol versions actually needed. Other packages
# stay unversioned; anything with glibc >= 2.35 (Ubuntu 22.04+, Debian 12+) also
# ships the GCC 12 libstdc++ the binary needs.
GLIBC_MIN="$(sort -V -u "${WORK}/glibc" | tail -n 1)"
[ -n "${GLIBC_MIN}" ] || die "could not determine the minimum glibc version"
{
    echo "libc6 (>= ${GLIBC_MIN})"
    sort -u "${WORK}/pkgs" | { grep -vx 'libc6' || true; }
} | paste -sd ',' - | sed 's/,/, /g' >"${DEPENDS_OUT}"

echo "Staged ${PREFIX}: ${elf_count} ELF files, all resolved (Qt from the package only)"
echo "Depends: $(cat "${DEPENDS_OUT}")"
