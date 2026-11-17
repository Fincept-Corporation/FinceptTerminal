#pragma once
#include <QString>

namespace fincept {

/// Central registry for all application file-system paths.
///
/// All paths live under a single root directory, named by the edition
/// (core/config/EditionIdentity.h — never the private build's folder):
///   Windows : %LOCALAPPDATA%/com.fincept.terminal.community/
///   macOS   : ~/Library/Application Support/com.fincept.terminal.community/
///   Linux   : ~/.local/share/com.fincept.terminal.community/
/// The com.fincept.terminal folder that released versions shared with the
/// private build is only read, once, by LegacyRootAdoption.
///
/// Sub-directories:
///   data/    — SQLite databases (fincept.db, cache.db)
///   logs/    — Application log files
///   files/   — User-managed file attachments
///   cache/   — Tile caches and other transient network data
///
/// The Python runtime lives directly under root() (uv/, python/, uv-cache/,
/// venv-numpy1/, venv-numpy2/) and is owned by PythonSetupManager.
///
/// Call AppPaths::ensure_all() once at startup to create every sub-directory.
class AppPaths {
  public:
    /// Root: %LOCALAPPDATA%\com.fincept.terminal.community  (Windows)
    /// Overridable via FINCEPT_DATA_DIR environment variable.
    static QString root();

    /// The trimmed FINCEPT_DATA_DIR value, or empty when root() is the default.
    /// A value that names the com.fincept.terminal folder (legacy_shared_root(),
    /// the private build's data root) or any folder holding a file only the
    /// private build writes is refused — empty, with one line on stderr — so a
    /// global FINCEPT_DATA_DIR can never point this edition at the other one's
    /// databases.
    static QString root_override();

    /// True when FINCEPT_DATA_DIR is set but was refused (see root_override()).
    static bool root_override_refused();

    /// Parent of the default root: %LOCALAPPDATA%, ~/Library/Application Support,
    /// ~/.local/share. FINCEPT_DATA_DIR does not change it.
    static QString platform_data_base();

    /// The data root released versions used — com.fincept.terminal, shared with
    /// the private build. For LegacyRootAdoption only; never written.
    static QString legacy_shared_root();

    /// root/data  — persistent databases
    static QString data();

    /// root/logs  — log files
    static QString logs();

    /// root/files — user-managed file attachments
    static QString files();

    /// root/cache — tile caches and transient network data
    static QString cache();

    /// root/workspaces — saved workspace files (.fwsp)
    static QString workspaces();

    /// root/crashdumps — minidumps written by the unhandled-exception filter
    static QString crashdumps();

    /// cache/tmp — short-lived files handed to child processes (script cells,
    /// spilled arguments, cancel flags). Created on demand; empty string when it
    /// cannot be created. Not the OS temp folder: names there are shared with
    /// every other program (the private build's uninstaller sweeps
    /// %TEMP%\fincept_* and /tmp/fincept_*), and /tmp is shared by every account.
    static QString temp();

    /// temp()/<name>. When temp() cannot be created: the OS temp folder, with the
    /// edition's prefix ("fincept-community-<name>") so no other program's name
    /// pattern matches it. `name` should already be unique (a UUID).
    static QString temp_file(const QString& name);

    /// Create all sub-directories if they don't exist.
    /// Call once before any path is used.
    static void ensure_all();

    /// Locate a file shipped in the app's read-only resources/ tree, e.g.
    /// bundled_resource("charts/klinechart.html"). Searches every layout we ship:
    ///   <exe>/resources/                         Windows, AppImage, dev builds
    ///   <exe>/../Resources/                      macOS .app (MacOS/resources/* is flattened here)
    ///   <exe>/../Resources/resources/            macOS .app (copy of the source tree)
    ///   <exe>/../share/fincept-terminal/resources/  Linux .deb/.rpm (FHS)
    /// Returns the first existing path. When none exists, returns the
    /// <exe>/resources/ candidate so a caller's "not found" log names the
    /// conventional location.
    static QString bundled_resource(const QString& relative_path);
};

} // namespace fincept
