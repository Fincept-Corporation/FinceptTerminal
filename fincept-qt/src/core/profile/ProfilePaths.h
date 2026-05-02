#pragma once
#include "core/identity/Uuid.h"

#include <QString>

namespace fincept {

/// Resolves the paths the multi-window refactor introduces, scoped to the
/// currently-active profile.
///
/// `AppPaths` (in core/config/) already covers the legacy layout —
/// data/, logs/, cache/, files/, workspaces/, etc. Those keep working
/// unchanged. `ProfilePaths` adds the *new* directories the refactor needs
/// without disturbing the legacy tree:
///
///   <profile_root>/
///     workspace.db        (Phase 2 — live state, WAL SQLite)
///     layouts/            (Phase 6 — saved layouts, JSON-per-file)
///     cache.db            (Phase 4 — panel state, distinct from legacy /data/cache.db)
///     crashes/            (Phase 10 — minidumps; legacy /crashdumps/ remains for old code)
///
/// All accessors are pure path computation — they do not touch the filesystem.
/// Use ensure_all() once at shell startup (after ProfileManager::set_active())
/// to create the directory tree.
///
/// Why a separate class: the legacy AppPaths is wired into 100+ call sites
/// for fincept.db/cache.db/runtime/. Touching it risks breaking unrelated
/// subsystems. ProfilePaths is the new, isolated surface for the new code.
/// Phase 6 will gradually migrate consumers off AppPaths::workspaces() onto
/// ProfilePaths::layouts_dir(); until then both coexist.
class ProfilePaths {
  public:
    /// Active profile's filesystem root. Equivalent to
    /// ProfileManager::instance().profile_root() — exposed here so callers
    /// can stay in the new namespace.
    static QString profile_root();

    /// New SQLite live-workspace database. Phase 2 schema.
    /// Distinct from AppPaths::data()/fincept.db (the application DB).
    static QString workspace_db();

    /// Directory holding one JSON file per saved layout, plus a small
    /// SQLite index. Replaces AppPaths::workspaces() (.fwsp files) in Phase 6.
    static QString layouts_dir();

    /// Index db inside layouts_dir(). Tracks (uuid, name, type, created,
    /// updated, thumbnail_path) for fast list/search.
    static QString layouts_index_db();

    /// Per-panel persisted state (filters, scroll, sub-panel state). Keyed
    /// by PanelInstanceId in Phase 4. Distinct from the legacy
    /// AppPaths::data()/cache.db which holds market-data caches and
    /// screen-state for the pre-UUID system; the two coexist during the
    /// migration window.
    static QString panel_cache_db();

    /// Minidumps written by Phase 10's CrashReporter. Legacy
    /// AppPaths::crashdumps() stays for the old crash handler until it's
    /// retired.
    static QString crashes_dir();

    /// Create the new directory tree under the current profile root if it
    /// doesn't exist. Safe to call repeatedly. No-op if profile_root() is
    /// empty (which would mean ProfileManager::set_active() was never called
    /// — diagnosed as an error).
    static void ensure_all();

    /// Stable UUID of the active profile. May be null if ProfileManager
    /// hasn't loaded the manifest yet — callers should treat null as
    /// "shell init not complete." Convenience passthrough to
    /// ProfileManager::active_profile_id().
    static ProfileId active_profile_id();
};

} // namespace fincept
