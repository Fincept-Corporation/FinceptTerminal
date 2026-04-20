#pragma once
#include <QString>

namespace fincept {

/// Central registry for all application file-system paths.
///
/// All paths live under a single root directory:
///   Windows : %LOCALAPPDATA%/com.fincept.terminal/
///   macOS   : ~/Library/Application Support/com.fincept.terminal/
///   Linux   : ~/.local/share/com.fincept.terminal/
///
/// Sub-directories:
///   data/    — SQLite databases (fincept.db, cache.db)
///   logs/    — Application log files
///   files/   — User-managed file attachments
///   cache/   — Tile caches and other transient network data
///   models/  — ML model files
///   runtime/ — Python interpreter, UV, virtual environments
///
/// Call AppPaths::ensure_all() once at startup to create every sub-directory.
class AppPaths {
  public:
    /// Root: %LOCALAPPDATA%\com.fincept.terminal  (Windows)
    static QString root();

    /// root/data  — persistent databases
    static QString data();

    /// root/logs  — log files
    static QString logs();

    /// root/files — user-managed file attachments
    static QString files();

    /// root/cache — tile caches and transient network data
    static QString cache();

    /// root/models — ML model files
    static QString models();

    /// root/runtime — Python interpreter, UV, virtual environments
    static QString runtime();

    /// root/workspaces — saved workspace files (.fwsp)
    static QString workspaces();

    /// Create all sub-directories if they don't exist.
    /// Call once before any path is used.
    static void ensure_all();
};

} // namespace fincept
