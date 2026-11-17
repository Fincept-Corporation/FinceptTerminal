#pragma once
// EditionIdentity.h — the one place that names this build of Fincept Terminal to
// the operating system.
//
// Two builds of the same product exist: this open-source "community" terminal and
// a separate private build. Released versions of both used the SAME per-user
// names (data root com.fincept.terminal, QSettings Fincept/FinceptTerminal, the
// single-instance key FinceptTerminal-<profile>, ...), so each opened, migrated,
// overwrote and even deleted the other's data. Every machine- or user-global name
// this build uses is derived from the constants below, and none of them can equal
// a name the private build uses: they carry the edition id, not a version.
//
// The values come from CMake cache variables (FINCEPT_EDITION_*, see
// CMakeLists.txt "Edition identity"), which also name the installed desktop entry,
// AppStream id, macOS bundle id and installer folders, so code and packaging
// cannot drift. The fallbacks here are the same defaults, for translation units
// compiled without the target's definitions (unit tests, clang-tidy).
//
// kLegacy* are the names the two builds shared. This build never writes them
// again; LegacyRootAdoption reads them (read-only) once, to carry an existing
// community install over.

#ifndef FINCEPT_EDITION_ID
#    define FINCEPT_EDITION_ID "community"
#endif
#ifndef FINCEPT_EDITION_DATA_DIR_NAME
#    define FINCEPT_EDITION_DATA_DIR_NAME "com.fincept.terminal.community"
#endif
#ifndef FINCEPT_EDITION_SETTINGS_ORG
#    define FINCEPT_EDITION_SETTINGS_ORG "FinceptCommunity"
#endif
#ifndef FINCEPT_EDITION_SETTINGS_APP
#    define FINCEPT_EDITION_SETTINGS_APP "FinceptTerminalCommunity"
#endif
#ifndef FINCEPT_EDITION_DESKTOP_ID
#    define FINCEPT_EDITION_DESKTOP_ID "fincept-terminal-community"
#endif
#ifndef FINCEPT_EDITION_FLATPAK_ID
#    define FINCEPT_EDITION_FLATPAK_ID "in.fincept.FinceptTerminal"
#endif
#ifndef FINCEPT_EDITION_BUNDLE_ID
#    define FINCEPT_EDITION_BUNDLE_ID "com.fincept.terminal.community"
#endif
#ifndef FINCEPT_EDITION_LINUX_PACKAGE
#    define FINCEPT_EDITION_LINUX_PACKAGE "fincept-terminal-community"
#endif
#ifndef FINCEPT_EDITION_LINUX_PREFIX
#    define FINCEPT_EDITION_LINUX_PREFIX "/opt/fincept-terminal-community"
#endif
#ifndef FINCEPT_EDITION_TEMP_PREFIX
#    define FINCEPT_EDITION_TEMP_PREFIX "fincept-community"
#endif
#ifndef FINCEPT_EDITION_MCP_SERVER_NAME
#    define FINCEPT_EDITION_MCP_SERVER_NAME "fincept_community"
#endif

namespace fincept::edition {

/// Short edition id ("community"). Also names the edition's own database lineage.
inline constexpr char kId[] = FINCEPT_EDITION_ID;

/// Folder name of the per-user data root under the platform data location
/// (%LOCALAPPDATA%, ~/Library/Application Support, ~/.local/share).
inline constexpr char kDataDirName[] = FINCEPT_EDITION_DATA_DIR_NAME;

/// QCoreApplication organization / application name. They key every QSettings
/// store (HKCU\Software\<org>\<app>, ~/.config/<org>/<app>.conf,
/// ~/Library/Preferences/com.<org>.<app>.plist), the QStandardPaths App* / Cache
/// locations, and the X11 WM_CLASS class. The organization is not "Fincept": the
/// private build's uninstaller deletes HKCU\Software\Fincept and ~/.cache/Fincept
/// recursively.
inline constexpr char kSettingsOrganization[] = FINCEPT_EDITION_SETTINGS_ORG;
inline constexpr char kSettingsApplication[] = FINCEPT_EDITION_SETTINGS_APP;

/// Prefix of the single-instance key ("<prefix>-<profile>"; InstanceLock hashes it
/// into the local-socket / named-pipe name).
inline constexpr char kInstanceKeyPrefix[] = FINCEPT_EDITION_SETTINGS_APP;

/// Desktop-entry id of the AppImage / .deb / .rpm / IFW installs (Wayland app_id,
/// icon name, <id>.desktop). The Flatpak keeps its published app id.
inline constexpr char kDesktopFileName[] = FINCEPT_EDITION_DESKTOP_ID;
inline constexpr char kFlatpakAppId[] = FINCEPT_EDITION_FLATPAK_ID;

/// macOS CFBundleIdentifier, AppStream id and installer id.
inline constexpr char kBundleId[] = FINCEPT_EDITION_BUNDLE_ID;

/// .deb / .rpm package name, its install prefix (<prefix>/bin/FinceptTerminal)
/// and the launcher it puts on PATH (/usr/bin/<package name>). The private build's
/// packages are "fincept-terminal" in /opt/fincept-terminal: with one shared
/// package name dpkg / rpm treated installing either edition as an upgrade or
/// downgrade of the other, and removed one to install the other.
inline constexpr char kLinuxPackageName[] = FINCEPT_EDITION_LINUX_PACKAGE;
inline constexpr char kLinuxInstallPrefix[] = FINCEPT_EDITION_LINUX_PREFIX;

/// Prefix of every name this edition puts in a folder or namespace shared with
/// other programs: the single-instance socket / named pipe, the boot log in the
/// temp folder (EditionNames.h).
inline constexpr char kTempPrefix[] = FINCEPT_EDITION_TEMP_PREFIX;

/// Name of this edition's MCP server entry in the MCP client configs it writes
/// (Claude, Codex, Gemini, ...). The two editions' entries must never share a
/// key: writing one would replace (and removing one would delete) the other's.
inline constexpr char kMcpServerName[] = FINCEPT_EDITION_MCP_SERVER_NAME;

// ── Names shared with the private build (read-only, for adoption) ────────────
inline constexpr char kLegacyDataDirName[] = "com.fincept.terminal";
inline constexpr char kLegacySettingsOrganization[] = "Fincept";
inline constexpr char kLegacySettingsApplication[] = "FinceptTerminal";
inline constexpr char kLegacyInstanceKeyPrefix[] = "FinceptTerminal";
inline constexpr char kLegacyInstanceSocketPrefix[] = "fincept-";
inline constexpr char kLegacyLinuxPackageName[] = "fincept-terminal";
inline constexpr char kLegacyLinuxInstallPrefix[] = "/opt/fincept-terminal";

/// Paths, relative to a data root, that only the private build writes (checked
/// against its source; none occurs in this tree). A folder holding any of them
/// is, or was, the private build's: this edition never adopts it
/// (app/EditionRootDecision.h) and never accepts it as FINCEPT_DATA_DIR
/// (AppPaths::root_override()). graphics.json is written at the root on every
/// start of the private build, before its QApplication.
inline constexpr const char* kPrivateBuildFingerprints[] = {
    "graphics.json",
    "test-settings",
    "data/.fincept_notebooks_seeded_v2",
    "data/algo.db",
    "data/fincept-mcp",
    "data/harness",
    "data/settings_cache.json",
    "data/settings_sync_meta.json",
    "data/chat_archive",
};

} // namespace fincept::edition
