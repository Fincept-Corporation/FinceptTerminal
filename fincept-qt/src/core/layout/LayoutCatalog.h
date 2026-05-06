#pragma once
#include "core/identity/Uuid.h"
#include "core/layout/LayoutTypes.h"
#include "core/result/Result.h"

#include <QList>
#include <QObject>
#include <QString>

namespace fincept {

/// Phase 6: per-profile saved-layouts catalogue.
///
/// Directory layout (under ProfilePaths::layouts_dir()):
///   <uuid>.json      — one Workspace per file (decisions 4.6/4.8)
///   _index.db        — SQLite index for fast list/search/recent
///
/// The index db has one table:
///   layouts(
///     uuid TEXT PRIMARY KEY,
///     name TEXT NOT NULL,
///     kind TEXT NOT NULL,            -- 'auto' | 'user' | 'builtin' | 'crash_recovery'
///     created_at_unix INTEGER NOT NULL,
///     updated_at_unix INTEGER NOT NULL,
///     description TEXT,
///     thumbnail_path TEXT
///   )
///
/// Insert/update on save_workspace; read on list_layouts. Full Workspace
/// JSON is stored in the per-uuid file, NOT the index, so the index
/// stays small (one row per saved layout).
///
/// Threading: UI thread only. SQLite + filesystem I/O runs synchronously
/// from save/list/load — workspace-level operations are user-driven and
/// rare (a few per session) so no QtConcurrent path needed.
class LayoutCatalog : public QObject {
    Q_OBJECT
  public:
    static LayoutCatalog& instance();

    /// Open the catalogue for the active profile (called from
    /// TerminalShell::initialise after ProfilePaths::ensure_all). Idempotent.
    Result<void> open();

    /// Persist a Workspace to disk (file + index row). Replaces any
    /// existing entry with the same UUID. Returns the saved layout's id
    /// for caller chaining.
    Result<LayoutId> save_workspace(const layout::Workspace& w);

    /// Load a Workspace by UUID. err if file missing / corrupt.
    Result<layout::Workspace> load_workspace(const LayoutId& id);

    /// Remove a layout (file + index row). No-op if not present.
    Result<void> remove_layout(const LayoutId& id);

    /// List all known layouts. Cheap (just the index).
    struct Entry {
        LayoutId id;
        QString name;
        QString kind;          ///< 'auto' | 'user' | 'builtin' | 'crash_recovery'
        qint64 created_at_unix = 0;
        qint64 updated_at_unix = 0;
        QString description;
        QString thumbnail_path;
    };
    Result<QList<Entry>> list_layouts();

    /// Most-recent N layouts (by updated_at), excluding 'auto' kind unless
    /// `include_auto=true`. Drives the Launchpad recent list.
    Result<QList<Entry>> recent_layouts(int limit, bool include_auto = false);

    /// Convenience: layout id of any layout matching `name` (case-insensitive),
    /// or null id if none. Used by the `layout.switch` action.
    LayoutId find_by_name(const QString& name);

    /// Export a layout to an arbitrary file path. Same JSON format as the
    /// internal layouts/<uuid>.json file. Path must include the .json
    /// extension by convention; we don't enforce it.
    Result<void> export_to(const LayoutId& id, const QString& path);

    /// Import a JSON file as a new layout. Returns the new layout's UUID
    /// (mints a fresh one if the file doesn't carry a valid id, so two
    /// users importing the same shared file don't collide).
    Result<LayoutId> import_from(const QString& path);

    /// Persisted "last applied layout" pointer. Stored in the index DB's
    /// `_meta` table under key `last_loaded_uuid`. Set by
    /// `WorkspaceShell::apply()` on every successful apply of a non-auto
    /// workspace. Read by `WorkspaceShell::load_last_or_default()` on
    /// startup. Returns null id if unset.
    Result<LayoutId> last_loaded_id() const;
    Result<void> set_last_loaded_id(const LayoutId& id);

    /// Read/write any key in the index DB's `_meta` table. Empty string
    /// returned on miss (no error). Used by both the last_loaded_id pair
    /// above and Phase 7's `WorkspaceFwspImporter` (key=`fwsp_import_done`).
    Result<QString> meta(const QString& key) const;
    Result<void> set_meta(const QString& key, const QString& value);

  signals:
    void layout_saved(const LayoutId& id);
    void layout_removed(const LayoutId& id);

  private:
    LayoutCatalog() = default;

    Result<void> ensure_index_schema();
    Result<void> upsert_index_row(const layout::Workspace& w);

    QString file_path_for_(const LayoutId& id) const;

    bool opened_ = false;
};

} // namespace fincept
