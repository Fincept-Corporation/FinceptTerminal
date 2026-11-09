#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# uninstall.sh — remove Fincept Terminal and everything it created
#                (macOS and Linux; Windows uses uninstall.ps1 / uninstall.cmd)
#
#   bash uninstall.sh [options]
#
# Removes, for the current user:
#   - the application, however it was installed
#       macOS : FinceptTerminal.app (DMG) or the IFW wizard install
#       Linux : .deb / .rpm package, Flatpak, AppImage, IFW .run install,
#               or a `cmake --install` prefix (/usr/local, ~/.local)
#   - all data: every profile, databases (incl. saved API keys), logs,
#     layouts, caches, crash dumps, the Python runtime, venvs and uv cache
#   - settings (plist / .conf), legacy Keychain / libsecret items from
#     versions <= 4.0.2, temp files, the single-instance socket, and data the
#     bundled Python scripts keep in your home folder (~/.fincept, ...)
#
# Options
#   -y, --yes            do not ask for confirmation
#   -n, --dry-run        only list what would be removed; change nothing
#       --keep-data      remove the application only; keep data and settings
#       --data-only      remove data and settings only; keep the application
#       --shared-caches  also remove caches other Python software shares
#                        (py-yfinance, matplotlib, yt-dlp, nltk_data, ~/.qlib,
#                        ~/.edgar) — off by default
#       --data-dir DIR   also remove a data folder moved with FINCEPT_DATA_DIR
#                        (repeatable; $FINCEPT_DATA_DIR itself is picked up)
#   -h, --help           show this help
#
# Run it as your normal user — it calls sudo only for system-wide pieces
# (/Applications, /opt, packages). Started with sudo, it cleans $SUDO_USER.
# It never edits MCP client configs (Claude Desktop, Codex, Cursor, ...); it
# lists the ones that still mention Fincept so you can remove those entries.
#
# Exit status: 0 done (or nothing to do), 1 something could not be removed,
#              2 usage error or cancelled.
#
# Keep in step with: packaging/uninstall/uninstall.ps1 (Windows) and
# packaging/installer/packages/com.fincept.terminal.core/meta/installscript.qs.
# Written for bash 3.2 (the macOS system bash): no associative arrays, no
# mapfile, and empty arrays are never expanded under `set -u`.
# ─────────────────────────────────────────────────────────────────────────────
set -u

APP_ID="com.fincept.terminal"          # data folder name, macOS bundle id, legacy secret service
PKG_NAME="fincept-terminal"            # .deb / .rpm package, desktop file id, icon name
FLATPAK_ID="in.fincept.FinceptTerminal"
QT_ORG="Fincept"                       # QCoreApplication organization / application name
QT_APP="FinceptTerminal"

YES=0
DRY_RUN=0
KEEP_DATA=0
DATA_ONLY=0
SHARED=0
EXTRA_DATA_DIRS=""                     # newline-separated (bash 3.2 + set -u friendly)

usage() {
    cat <<'EOF'
Fincept Terminal uninstaller (macOS / Linux)

Usage: bash uninstall.sh [options]

  -y, --yes            do not ask for confirmation
  -n, --dry-run        only list what would be removed; change nothing
      --keep-data      remove the application only; keep data and settings
      --data-only      remove data and settings only; keep the application
      --shared-caches  also remove caches shared with other Python software
                       (py-yfinance, matplotlib, yt-dlp, nltk_data, ~/.qlib, ~/.edgar)
      --data-dir DIR   also remove a data folder moved with FINCEPT_DATA_DIR
  -h, --help           show this help

Without --yes it first lists everything it found and asks before removing.
EOF
}

die_usage() {
    printf 'uninstall.sh: %s\n\n' "$1" >&2
    usage >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
        -y | --yes) YES=1 ;;
        -n | --dry-run) DRY_RUN=1 ;;
        --keep-data) KEEP_DATA=1 ;;
        --data-only) DATA_ONLY=1 ;;
        --shared-caches) SHARED=1 ;;
        --data-dir)
            shift
            [ $# -gt 0 ] || die_usage "--data-dir needs a folder"
            EXTRA_DATA_DIRS="${EXTRA_DATA_DIRS}${1}
"
            ;;
        --data-dir=*)
            EXTRA_DATA_DIRS="${EXTRA_DATA_DIRS}${1#*=}
"
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *) die_usage "unknown option: $1" ;;
    esac
    shift
done
if [ "$KEEP_DATA" -eq 1 ] && [ "$DATA_ONLY" -eq 1 ]; then
    die_usage "--keep-data and --data-only cannot be combined"
fi

case "$(uname -s)" in
    Darwin) OS="mac" ;;
    Linux) OS="linux" ;;
    *)
        echo "uninstall.sh supports macOS and Linux. On Windows run uninstall.cmd." >&2
        exit 2
        ;;
esac

# ── Output ───────────────────────────────────────────────────────────────────
if [ -t 1 ]; then
    C_B=$'\033[1m'; C_G=$'\033[32m'; C_Y=$'\033[33m'; C_R=$'\033[31m'; C_D=$'\033[2m'; C_N=$'\033[0m'
else
    C_B=""; C_G=""; C_Y=""; C_R=""; C_D=""; C_N=""
fi
section() { printf '\n%s%s%s\n' "$C_B" "$*" "$C_N"; }
info() { printf '  %s\n' "$*"; }
warn() { printf '  %s! %s%s\n' "$C_Y" "$*" "$C_N"; }

ACTIONS=0
FAILED=0
FAILED_ITEMS=""
fail() {
    FAILED=$((FAILED + 1))
    FAILED_ITEMS="${FAILED_ITEMS}    $*
"
    printf '  %sx %s%s\n' "$C_R" "$*" "$C_N"
}

# ── Whose data ───────────────────────────────────────────────────────────────
CUR_UID="$(id -u)"
SUDO_TARGET=0
if [ "$CUR_UID" -eq 0 ] && [ -n "${SUDO_USER:-}" ] && [ "${SUDO_USER}" != "root" ]; then
    TARGET_USER="$SUDO_USER"
    SUDO_TARGET=1
else
    TARGET_USER="$(id -un)"
fi

home_of() {
    local h=""
    if [ "$SUDO_TARGET" -eq 0 ] && [ -n "${HOME:-}" ]; then
        printf '%s\n' "$HOME"
        return
    fi
    if command -v getent >/dev/null 2>&1; then
        h="$(getent passwd "$1" 2>/dev/null | cut -d: -f6)"
    fi
    if [ -z "$h" ] && command -v dscl >/dev/null 2>&1; then
        h="$(dscl . -read "/Users/$1" NFSHomeDirectory 2>/dev/null | sed 's/^NFSHomeDirectory: //')"
    fi
    printf '%s\n' "$h"
}
TARGET_HOME="$(home_of "$TARGET_USER")"
TARGET_HOME="${TARGET_HOME%/}"
if [ -z "$TARGET_HOME" ] || [ "$TARGET_HOME" = "/" ] || [ ! -d "$TARGET_HOME" ]; then
    echo "Cannot determine the home folder of ${TARGET_USER}; nothing was changed." >&2
    exit 2
fi

# Root's XDG_* would be wrong for $SUDO_USER, so only trust them when not sudo'd.
if [ "$SUDO_TARGET" -eq 1 ]; then
    XDG_DATA_T="$TARGET_HOME/.local/share"
    XDG_CONFIG_T="$TARGET_HOME/.config"
    XDG_CACHE_T="$TARGET_HOME/.cache"
else
    XDG_DATA_T="${XDG_DATA_HOME:-$TARGET_HOME/.local/share}"
    XDG_CONFIG_T="${XDG_CONFIG_HOME:-$TARGET_HOME/.config}"
    XDG_CACHE_T="${XDG_CACHE_HOME:-$TARGET_HOME/.cache}"
fi

# Run a command as the target user (matters only under sudo).
as_user() {
    if [ "$SUDO_TARGET" -eq 1 ]; then
        sudo -u "$TARGET_USER" -H "$@"
    else
        "$@"
    fi
}

# Run a command as root (sudo prompts for a password when needed).
as_root() {
    if [ "$CUR_UID" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        echo "sudo is not available — cannot run: $*" >&2
        return 127
    fi
}

# ── Removal primitives ───────────────────────────────────────────────────────

# Never delete a home, system or shared parent folder, whatever a caller passes.
is_protected() {
    local p="${1%/}"
    case "$p" in
        /*) ;;
        *) return 0 ;; # relative or empty
    esac
    case "$p" in
        "" | / | /bin | /sbin | /etc | /usr | /usr/bin | /usr/sbin | /usr/local | /usr/local/bin | \
            /usr/share | /usr/share/applications | /opt | /var | /tmp | /var/tmp | /private | /private/tmp | \
            /Applications | /Library | /System | /Users | /home | \
            "$TARGET_HOME" | "$TARGET_HOME/Library" | "$TARGET_HOME/Library/Application Support" | \
            "$TARGET_HOME/Library/Caches" | "$TARGET_HOME/Library/Preferences" | "$TARGET_HOME/Applications" | \
            "$TARGET_HOME/.local" | "$TARGET_HOME/.local/share" | "$TARGET_HOME/.local/bin" | \
            "$TARGET_HOME/.config" | "$TARGET_HOME/.cache" | "$TARGET_HOME/Downloads" | "$TARGET_HOME/Desktop" | \
            "$XDG_DATA_T" | "$XDG_CONFIG_T" | "$XDG_CACHE_T")
            return 0
            ;;
    esac
    return 1
}

exists() { [ -e "$1" ] || [ -L "$1" ]; }

# remove <path> [root] — delete a file, symlink or folder tree. "root" allows
# a sudo retry for system locations the user cannot write.
remove() {
    local p="$1" mode="${2:-user}"
    exists "$p" || return 0
    if is_protected "$p"; then
        warn "refusing to remove protected path: $p"
        return 0
    fi
    ACTIONS=$((ACTIONS + 1))
    if [ "$DRY_RUN" -eq 1 ]; then
        local note=""
        if [ "$mode" = "root" ] && [ "$CUR_UID" -ne 0 ] && [ ! -w "$(dirname "$p")" ]; then
            note=" (needs sudo)"
        fi
        info "${C_D}would remove${C_N} $p$note"
        return 0
    fi
    rm -rf -- "$p" 2>/dev/null
    if exists "$p" && [ "$mode" = "root" ] && [ "$CUR_UID" -ne 0 ]; then
        as_root rm -rf -- "$p"
    fi
    if exists "$p"; then
        fail "could not remove $p"
    else
        info "${C_G}removed${C_N} $p"
    fi
}

rmdir_if_empty() {
    [ "$DRY_RUN" -eq 1 ] && return 0
    [ -d "$1" ] || return 0
    rmdir "$1" 2>/dev/null && info "${C_G}removed${C_N} $1 (empty)"
    return 0
}

# run <description> <command...> — a non-file action that honours --dry-run.
run() {
    local desc="$1"
    shift
    ACTIONS=$((ACTIONS + 1))
    local shown="$*"
    shown="${shown/#as_root /sudo }"
    shown="${shown/#as_user /}"
    if [ "$DRY_RUN" -eq 1 ]; then
        info "${C_D}would run${C_N} $shown"
        return 0
    fi
    info "$desc"
    if "$@"; then
        return 0
    fi
    fail "$desc failed ($shown)"
    return 1
}

owner_of() {
    if [ "$OS" = "mac" ]; then
        stat -f %Su "$1" 2>/dev/null
    else
        stat -c %U "$1" 2>/dev/null
    fi
}

# ── Running processes ────────────────────────────────────────────────────────

# The app, its maintenance tool and helpers (Python workers, uv, yt-dlp,
# QtWebEngineProcess, the Flatpak sandbox) started from the install or data
# folders — never this script or its sudo parent.
find_pids() {
    local pat="FinceptTerminal|FinceptMaintenanceTool|com\\.fincept\\.terminal/|/opt/fincept-terminal/|in\\.fincept\\.FinceptTerminal"
    local pid cmd
    for pid in $(pgrep -u "$TARGET_USER" -f "$pat" 2>/dev/null); do
        [ "$pid" = "$$" ] && continue
        [ "$pid" = "${PPID:-}" ] && continue
        cmd="$(ps -o command= -p "$pid" 2>/dev/null)"
        case "$cmd" in
            *uninstall.sh* | *uninstall-fincept* | *"Uninstall Fincept"*) continue ;;
        esac
        printf '%s ' "$pid"
    done
}

stop_processes() {
    local pids alive pid
    pids="$(find_pids)"
    [ -n "${pids// /}" ] || return 0
    ACTIONS=$((ACTIONS + 1))
    if [ "$DRY_RUN" -eq 1 ]; then
        info "${C_D}would stop${C_N} running Fincept Terminal processes (PIDs: ${pids% })"
        return 0
    fi
    info "stopping Fincept Terminal processes (PIDs: ${pids% })"
    # shellcheck disable=SC2086 # word-splitting the PID list is intended
    kill -TERM $pids 2>/dev/null
    for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        alive=""
        for pid in $pids; do
            kill -0 "$pid" 2>/dev/null && alive="$alive $pid"
        done
        [ -z "$alive" ] && return 0
        sleep 0.5
    done
    # shellcheck disable=SC2086
    kill -KILL $alive 2>/dev/null
    sleep 1
    for pid in $alive; do
        kill -0 "$pid" 2>/dev/null && fail "process $pid is still running"
    done
    return 0
}

# ── Application ──────────────────────────────────────────────────────────────

# A Qt Installer Framework install folder (wizard installer on macOS / Linux).
remove_ifw_dir() {
    local d="$1"
    [ -d "$d" ] || return 0
    if exists "$d/FinceptMaintenanceTool" || exists "$d/FinceptMaintenanceTool.app" ||
        [ -d "$d/FinceptTerminal.app" ] || [ -x "$d/bin/FinceptTerminal" ]; then
        remove "$d" root
    else
        warn "skipping $d: it does not look like a Fincept Terminal install"
    fi
}

bundle_id_of() {
    /usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$1/Contents/Info.plist" 2>/dev/null
}

remove_app_mac() {
    local app lsreg
    lsreg="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
    for app in "/Applications/FinceptTerminal.app" "/Applications/Fincept Terminal.app" \
        "$TARGET_HOME/Applications/FinceptTerminal.app" "$TARGET_HOME/Applications/Fincept Terminal.app"; do
        [ -d "$app" ] || continue
        if [ "$(bundle_id_of "$app")" != "$APP_ID" ]; then
            warn "skipping $app: its bundle id is not $APP_ID"
            continue
        fi
        # Forget it in Launch Services so Spotlight / Open With drop it now.
        if [ "$DRY_RUN" -eq 0 ] && [ -x "$lsreg" ]; then
            as_user "$lsreg" -u "$app" >/dev/null 2>&1
        fi
        remove "$app" root
    done
    remove_ifw_dir "/Applications/FinceptTerminal"
    remove_ifw_dir "$TARGET_HOME/Applications/FinceptTerminal"
}

deb_installed() {
    command -v dpkg-query >/dev/null 2>&1 &&
        dpkg-query -W -f='${Status}' "$PKG_NAME" 2>/dev/null | grep -q 'ok installed'
}

rpm_installed() {
    command -v rpm >/dev/null 2>&1 && rpm -q "$PKG_NAME" >/dev/null 2>&1
}

owned_by_package() {
    if command -v dpkg >/dev/null 2>&1 && dpkg -S "$1" >/dev/null 2>&1; then
        return 0
    fi
    if command -v rpm >/dev/null 2>&1 && rpm -qf "$1" >/dev/null 2>&1; then
        return 0
    fi
    return 1
}

remove_packages_linux() {
    if deb_installed; then
        if command -v apt-get >/dev/null 2>&1; then
            run "removing the $PKG_NAME .deb package" as_root apt-get purge -y "$PKG_NAME"
        else
            run "removing the $PKG_NAME .deb package" as_root dpkg --purge "$PKG_NAME"
        fi
    fi
    if rpm_installed; then
        if command -v dnf >/dev/null 2>&1; then
            run "removing the $PKG_NAME .rpm package" as_root dnf remove -y "$PKG_NAME"
        elif command -v zypper >/dev/null 2>&1; then
            run "removing the $PKG_NAME .rpm package" as_root zypper --non-interactive remove "$PKG_NAME"
        elif command -v yum >/dev/null 2>&1; then
            run "removing the $PKG_NAME .rpm package" as_root yum remove -y "$PKG_NAME"
        else
            run "removing the $PKG_NAME .rpm package" as_root rpm -e "$PKG_NAME"
        fi
    fi

    # Files the package manager leaves behind (anything created at run time),
    # and `cmake --install` prefixes. While a package still owns a path (dry
    # run), the package removal above already covers it.
    local p prefix
    if [ -d /opt/fincept-terminal ] && ! owned_by_package /opt/fincept-terminal; then
        remove /opt/fincept-terminal root
    fi
    for prefix in /usr/local /usr "$TARGET_HOME/.local"; do
        exists "$prefix/bin/FinceptTerminal" || [ -d "$prefix/share/fincept-terminal" ] || continue
        for p in "$prefix/bin/FinceptTerminal" "$prefix/share/fincept-terminal" "$prefix/libexec/fincept-terminal" \
            "$prefix/share/applications/$PKG_NAME.desktop" "$prefix/share/metainfo/$PKG_NAME.appdata.xml" \
            "$prefix/share/icons/hicolor/256x256/apps/$PKG_NAME.png"; do
            exists "$p" || continue
            owned_by_package "$p" && continue
            remove "$p" root
        done
    done
}

remove_flatpak() {
    command -v flatpak >/dev/null 2>&1 || return 0
    if as_user flatpak info --user "$FLATPAK_ID" >/dev/null 2>&1; then
        run "removing the Flatpak $FLATPAK_ID (user)" as_user flatpak uninstall --user -y --delete-data "$FLATPAK_ID"
    fi
    if flatpak info --system "$FLATPAK_ID" >/dev/null 2>&1; then
        run "removing the Flatpak $FLATPAK_ID (system)" flatpak uninstall --system -y --delete-data "$FLATPAK_ID"
    fi
}

# Downloaded AppImages in the usual places (the release asset is named
# FinceptTerminal-<version>-linux-x64-setup.run).
remove_appimages() {
    local dir f name
    shopt -s nocasematch
    for dir in "$TARGET_HOME/Applications" "$TARGET_HOME/AppImages" "$TARGET_HOME/.local/bin" "$TARGET_HOME/bin" \
        "$TARGET_HOME/Downloads" "$TARGET_HOME/Desktop" "$TARGET_HOME/.local/share/AppImage" /opt /opt/appimages; do
        [ -d "$dir" ] || continue
        for f in "$dir"/*; do
            [ -f "$f" ] || continue
            name="$(basename "$f")"
            case "$name" in
                FinceptTerminal*.AppImage | Fincept_Terminal*.AppImage | fincept-terminal*.AppImage | \
                    FinceptTerminal-*-linux-*.run)
                    remove "$f" root
                    ;;
            esac
        done
    done
    shopt -u nocasematch
}

# Desktop entries and icons from the IFW installer and AppImage integrators
# (AppImageLauncher, appimaged, Gear Lever), plus any autostart entry.
remove_desktop_integration() {
    local dir f
    remove "$XDG_DATA_T/applications/$PKG_NAME.desktop"
    for dir in "$XDG_DATA_T/applications" "$XDG_CONFIG_T/autostart"; do
        [ -d "$dir" ] || continue
        for f in "$dir"/*.desktop; do
            [ -f "$f" ] || continue
            [ "$f" = "$XDG_DATA_T/applications/$PKG_NAME.desktop" ] && continue
            if grep -qiE '^(Exec|TryExec)=.*(FinceptTerminal|fincept-terminal|in\.fincept\.FinceptTerminal)' "$f" 2>/dev/null; then
                remove "$f"
            fi
        done
    done
    if [ -d "$XDG_DATA_T/icons" ]; then
        while IFS= read -r f; do
            remove "$f"
        done < <(find "$XDG_DATA_T/icons" -type f \( -iname "*$PKG_NAME*" -o -iname '*finceptterminal*' \) 2>/dev/null)
    fi
    if [ "$DRY_RUN" -eq 0 ]; then
        command -v update-desktop-database >/dev/null 2>&1 &&
            as_user update-desktop-database "$XDG_DATA_T/applications" >/dev/null 2>&1
        command -v gtk-update-icon-cache >/dev/null 2>&1 && [ -d "$XDG_DATA_T/icons/hicolor" ] &&
            as_user gtk-update-icon-cache -q -t "$XDG_DATA_T/icons/hicolor" >/dev/null 2>&1
    fi
    return 0
}

remove_app_linux() {
    remove_packages_linux
    remove_flatpak
    remove_appimages
    remove_ifw_dir "$TARGET_HOME/FinceptTerminal"
    remove_ifw_dir /opt/FinceptTerminal
    remove_desktop_integration
}

# ── Data and settings ────────────────────────────────────────────────────────

looks_like_data_root() {
    local d="$1"
    [ -d "$d" ] || return 1
    [ -f "$d/profiles.json" ] || [ -f "$d/data/fincept.db" ] || [ -f "$d/workspace.db" ] ||
        [ -d "$d/venv-numpy2" ] || [ -d "$d/venv-numpy1" ] || [ -f "$d/.setup_complete" ] ||
        [ -f "$d/logs/fincept.log" ]
}

# The data root (all profiles: <root>/profiles/<name>), Python runtime, venvs,
# uv cache, databases with the encrypted API keys, logs, layouts, caches.
remove_data_root() {
    local d="${1%/}" size=""
    [ -d "$d" ] || return 0
    if [ "$(basename "$d")" != "$APP_ID" ] && ! looks_like_data_root "$d"; then
        warn "skipping $d: it does not look like a Fincept Terminal data folder"
        return 0
    fi
    if [ "$DRY_RUN" -eq 1 ]; then
        size="$(du -sh "$d" 2>/dev/null | cut -f1)"
        [ -n "$size" ] && info "${C_D}data folder size: $size${C_N}"
    fi
    remove "$d"
}

data_roots() {
    if [ "$OS" = "mac" ]; then
        printf '%s\n' "$TARGET_HOME/Library/Application Support/$APP_ID"
    else
        printf '%s\n' "$TARGET_HOME/.local/share/$APP_ID"
        [ "$XDG_DATA_T" != "$TARGET_HOME/.local/share" ] && printf '%s\n' "$XDG_DATA_T/$APP_ID"
    fi
    [ "$SUDO_TARGET" -eq 0 ] && [ -n "${FINCEPT_DATA_DIR:-}" ] && printf '%s\n' "$FINCEPT_DATA_DIR"
    [ -n "$EXTRA_DATA_DIRS" ] && printf '%s' "$EXTRA_DATA_DIRS"
}

remove_data_mac() {
    local L="$TARGET_HOME/Library" dom f n
    # Legacy data folders (renamed/migrated by newer versions).
    remove "$L/Application Support/$QT_ORG/$QT_APP"
    rmdir_if_empty "$L/Application Support/$QT_ORG"
    remove "$L/Application Support/$QT_APP"
    remove "$L/Application Support/fincept-dev"
    # Caches, saved window state, URL-session storage.
    remove "$L/Caches/$APP_ID"
    remove "$L/Caches/$QT_ORG/$QT_APP"
    rmdir_if_empty "$L/Caches/$QT_ORG"
    remove "$L/Saved Application State/$APP_ID.savedState"
    remove "$L/HTTPStorages/$APP_ID"
    remove "$L/HTTPStorages/$APP_ID.binarycookies"
    remove "$L/WebKit/$APP_ID"
    # QSettings (com.fincept.FinceptTerminal) and the bundle-id domain. cfprefsd
    # caches preferences, so drop the domain before the file.
    for dom in "com.fincept.$QT_APP" "$APP_ID" "$QT_ORG.$QT_APP"; do
        if as_user defaults read "$dom" >/dev/null 2>&1; then
            run "deleting preferences $dom" as_user defaults delete "$dom"
        fi
    done
    for f in "com.fincept.$QT_APP.plist" "$APP_ID.plist" "$QT_ORG.$QT_APP.plist" "$QT_ORG.$QT_APP-Secure.plist"; do
        remove "$L/Preferences/$f"
    done
    # Keychain items written by versions <= 4.0.2 (service com.fincept.terminal).
    if as_user security find-generic-password -s "$APP_ID" >/dev/null 2>&1; then
        ACTIONS=$((ACTIONS + 1))
        if [ "$DRY_RUN" -eq 1 ]; then
            info "${C_D}would delete${C_N} legacy Keychain items (service $APP_ID)"
        else
            n=0
            while [ "$n" -lt 1000 ] && as_user security delete-generic-password -s "$APP_ID" >/dev/null 2>&1; do
                n=$((n + 1))
            done
            info "${C_G}deleted${C_N} $n legacy Keychain item(s) (service $APP_ID)"
        fi
    fi
}

remove_data_linux() {
    # Legacy data folders (renamed/migrated by newer versions).
    remove "$XDG_DATA_T/$QT_ORG/$QT_APP"
    rmdir_if_empty "$XDG_DATA_T/$QT_ORG"
    remove "$XDG_DATA_T/$QT_APP"
    remove "$XDG_DATA_T/fincept-dev"
    if [ "$XDG_DATA_T" != "$TARGET_HOME/.local/share" ]; then
        remove "$TARGET_HOME/.local/share/$QT_ORG/$QT_APP"
        rmdir_if_empty "$TARGET_HOME/.local/share/$QT_ORG"
        remove "$TARGET_HOME/.local/share/$QT_APP"
        remove "$TARGET_HOME/.local/share/fincept-dev"
    fi
    # QSettings, including the file fallback of the <= 4.0.2 secure storage.
    remove "$XDG_CONFIG_T/$QT_ORG/$QT_APP.conf"
    remove "$XDG_CONFIG_T/$QT_ORG/$QT_APP-Secure.conf"
    rmdir_if_empty "$XDG_CONFIG_T/$QT_ORG"
    # Qt / app caches.
    remove "$XDG_CACHE_T/$QT_ORG/$QT_APP"
    rmdir_if_empty "$XDG_CACHE_T/$QT_ORG"
    remove "$XDG_CACHE_T/$APP_ID"
    # Flatpak sandbox data, if `flatpak uninstall --delete-data` did not run.
    remove "$TARGET_HOME/.var/app/$FLATPAK_ID"
    # libsecret items written by versions <= 4.0.2 (schema com.fincept.terminal).
    if command -v secret-tool >/dev/null 2>&1; then
        if as_user secret-tool search --all xdg:schema "$APP_ID" 2>&1 | grep -q '^\['; then
            run "deleting legacy libsecret items (schema $APP_ID)" as_user secret-tool clear xdg:schema "$APP_ID"
        elif [ "$SUDO_TARGET" -eq 1 ]; then
            warn "under sudo the keyring of $TARGET_USER is not reachable; run without sudo to clear legacy libsecret items"
        fi
    fi
}

# Folders the bundled Python scripts and external tools create in $HOME.
remove_home_extras() {
    local d
    remove "$TARGET_HOME/.fincept"                     # AI Quant Lab: mlruns, models, pipelines
    remove "$TARGET_HOME/.oecd_cache"                  # OECD connector cache
    remove "$XDG_CACHE_T/fincept-terminal"             # exchange market cache (outside the app)
    [ "$XDG_CACHE_T" != "$TARGET_HOME/.cache" ] && remove "$TARGET_HOME/.cache/fincept-terminal"
    remove "$TARGET_HOME/fincept-terminal/agents.db"   # agent repository fallback (no APPDATA on Unix)
    rmdir_if_empty "$TARGET_HOME/fincept-terminal"
    # Claude Code history of the coding-agent runs (cwd <root>/agent-workspace/...).
    for d in "$TARGET_HOME/.claude/projects/"*com-fincept-terminal-agent-workspace*; do
        [ -d "$d" ] && remove "$d"
    done
    return 0
}

# Temp files, the Python cancel/arg files, and the single-instance socket
# fincept-<16 hex> (left behind when the app crashes).
remove_temp() {
    local dirs t f name seen=":" udt re='^fincept-[0-9a-f]{16}$'
    dirs="/tmp /var/tmp"
    [ "$SUDO_TARGET" -eq 0 ] && [ -n "${TMPDIR:-}" ] && dirs="${TMPDIR%/} $dirs"
    if [ "$OS" = "mac" ]; then
        udt="$(as_user getconf DARWIN_USER_TEMP_DIR 2>/dev/null)"
        [ -n "$udt" ] && dirs="${udt%/} $dirs"
    fi
    for t in $dirs; do
        case "$seen" in *":$t:"*) continue ;; esac
        seen="$seen$t:"
        [ -d "$t" ] || continue
        for f in "$t"/fincept_* "$t"/fincept-*; do
            exists "$f" || continue
            name="$(basename "$f")"
            case "$name" in
                fincept_* | fincept-boot.log) ;;
                *) [[ "$name" =~ $re ]] || continue ;;
            esac
            [ "$(owner_of "$f")" = "$TARGET_USER" ] || continue
            remove "$f"
        done
    done
}

remove_shared_caches() {
    if [ "$OS" = "mac" ]; then
        remove "$TARGET_HOME/Library/Caches/py-yfinance"
        remove "$TARGET_HOME/.matplotlib"
        remove "$TARGET_HOME/Library/Caches/yt-dlp"
    else
        remove "$XDG_CACHE_T/py-yfinance"
        remove "$XDG_CONFIG_T/matplotlib"
        remove "$XDG_CACHE_T/matplotlib"
    fi
    remove "$XDG_CACHE_T/yt-dlp"
    remove "$TARGET_HOME/nltk_data"
    remove "$TARGET_HOME/.qlib"
    remove "$TARGET_HOME/.edgar"
    local t
    for t in "${TMPDIR:-/tmp}" /tmp; do
        [ -d "$t/data-gym-cache" ] && [ "$(owner_of "$t/data-gym-cache")" = "$TARGET_USER" ] &&
            remove "$t/data-gym-cache"
    done
    return 0
}

remove_data() {
    local d old_ifs seen="
"
    old_ifs="$IFS"
    IFS='
'
    for d in $(data_roots); do
        IFS="$old_ifs"
        d="${d%/}"
        case "$seen" in *"
$d
"*) continue ;; esac
        seen="$seen$d
"
        remove_data_root "$d"
    done
    IFS="$old_ifs"
    if [ "$OS" = "mac" ]; then
        remove_data_mac
    else
        remove_data_linux
    fi
    remove_home_extras
    remove_temp
}

# MCP client configs are the user's files: report, never edit.
report_mcp_configs() {
    local f found=""
    for f in "$TARGET_HOME/Library/Application Support/Claude/claude_desktop_config.json" \
        "$XDG_CONFIG_T/Claude/claude_desktop_config.json" "$TARGET_HOME/.claude.json" \
        "$TARGET_HOME/.codex/config.toml" "$TARGET_HOME/.gemini/settings.json" "$TARGET_HOME/.cursor/mcp.json" \
        "$TARGET_HOME/.codeium/windsurf/mcp_config.json"; do
        [ -f "$f" ] || continue
        if grep -qE 'fincept_terminal|fincept_mcp|com\.fincept\.terminal|"fincept"|mcp_servers\.fincept' "$f" 2>/dev/null; then
            found="${found}    $f
"
        fi
    done
    [ -n "$found" ] || return 0
    section "Left for you to edit"
    info "These files still mention Fincept Terminal (MCP server entries or agent history)."
    info "Remove the \"fincept\" / \"fincept_terminal\" entries yourself — this script never edits them:"
    printf '%s' "$found"
}

# ── Main ─────────────────────────────────────────────────────────────────────

do_all() {
    ACTIONS=0
    FAILED=0
    FAILED_ITEMS=""
    section "Running processes"
    stop_processes
    if [ "$DATA_ONLY" -eq 0 ]; then
        section "Application"
        if [ "$OS" = "mac" ]; then
            remove_app_mac
        else
            remove_app_linux
        fi
    fi
    if [ "$KEEP_DATA" -eq 0 ]; then
        section "Data, settings and saved credentials"
        remove_data
        if [ "$SHARED" -eq 1 ]; then
            section "Shared Python caches"
            remove_shared_caches
        fi
    fi
}

printf '%sFincept Terminal uninstaller%s — %s, user %s (%s)\n' "$C_B" "$C_N" \
    "$([ "$OS" = mac ] && echo macOS || echo Linux)" "$TARGET_USER" "$TARGET_HOME"
[ "$KEEP_DATA" -eq 1 ] && info "--keep-data: data and settings are kept"
[ "$DATA_ONLY" -eq 1 ] && info "--data-only: the application is kept"

if [ "$DRY_RUN" -eq 1 ]; then
    do_all
    section "Dry run"
    info "$ACTIONS item(s) would be removed. Nothing was changed."
    report_mcp_configs
    exit 0
fi

if [ "$YES" -eq 0 ]; then
    DRY_RUN=1
    do_all
    DRY_RUN=0
    if [ "$ACTIONS" -eq 0 ]; then
        section "Nothing to remove"
        info "No Fincept Terminal files were found."
        report_mcp_configs
        exit 0
    fi
    if [ ! -r /dev/tty ]; then
        echo
        echo "No terminal to ask for confirmation; re-run with --yes to remove the items above." >&2
        exit 2
    fi
    printf '\n%sRemove the %d item(s) listed above? This cannot be undone. [y/N] %s' "$C_B" "$ACTIONS" "$C_N"
    answer=""
    read -r answer </dev/tty || answer=""
    case "$answer" in
        y | Y | yes | YES | Yes) ;;
        *)
            echo "Cancelled; nothing was changed."
            exit 2
            ;;
    esac
fi

do_all

section "Summary"
if [ "$FAILED" -gt 0 ]; then
    printf '  %s%d item(s) could not be removed:%s\n%s' "$C_R" "$FAILED" "$C_N" "$FAILED_ITEMS"
    info "Close Fincept Terminal, then run this script again (with sudo for system folders)."
elif [ "$ACTIONS" -eq 0 ]; then
    info "No Fincept Terminal files were found."
else
    if [ "$KEEP_DATA" -eq 1 ]; then
        info "${C_G}Fincept Terminal was removed; your data and settings were kept.${C_N}"
    elif [ "$DATA_ONLY" -eq 1 ]; then
        info "${C_G}Fincept Terminal data and settings were removed; the application was kept.${C_N}"
    else
        info "${C_G}Fincept Terminal and all of its data were removed.${C_N}"
    fi
fi
report_mcp_configs
[ "$FAILED" -gt 0 ] && exit 1
exit 0
