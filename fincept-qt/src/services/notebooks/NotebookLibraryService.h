#pragma once
// NotebookLibraryService.h — Catalog + seeding for the prebuilt "Fincept
// Notebook" library shipped under resources/notebooks/.
//
// Responsibilities:
//   - Parse resources/notebooks/notebooks_manifest.json into a catalog.
//   - Resolve the bundled assets dir at runtime (dev + installed layouts).
//   - Seed copies into <profile>/files/ on first run (registered with the
//     FileManagerService so they appear in the File Manager).
//   - Provide a writable working copy for the Library "OPEN" action so the
//     bundled originals stay pristine and user edits persist + stay in sync.

#include <QObject>
#include <QString>
#include <QVector>

namespace fincept::services {

/// One prebuilt notebook described by the manifest.
struct NotebookCatalogEntry {
    QString id;
    QString title;
    QString category;     // Finance, Economics, Trading, Investing, Portfolio, Quant
    QString difficulty;   // "Beginner" | "Intermediate" | "Hard"
    int est_minutes = 0;
    QString requirements; // "stdlib" | "pandas + numpy"
    QString summary;
    QString file;         // bundled filename, e.g. finance_time_value_of_money.ipynb
};

class NotebookLibraryService : public QObject {
    Q_OBJECT

  public:
    static NotebookLibraryService& instance();

    /// Parsed catalog (loaded + cached on first call). Empty if assets missing.
    const QVector<NotebookCatalogEntry>& catalog();

    /// Absolute path to the bundled notebooks directory, or empty if not found.
    QString assets_dir();

    /// Absolute path to a bundled notebook .ipynb for an entry (read-only asset).
    QString asset_path(const NotebookCatalogEntry& entry);

    /// First-run seeding: copy every bundled notebook into <profile>/files/ and
    /// register each with FileManagerService (source "code_editor"). Idempotent —
    /// guarded by a marker file. Pass force=true to re-seed regardless.
    void seed_into_files(bool force = false);

    /// Ensure a writable working copy of `entry` exists in <profile>/files/ and
    /// return its absolute path (registering it with FileManagerService if new).
    /// Returns the bundled asset path as a last resort if storage is unavailable.
    QString working_copy_for(const NotebookCatalogEntry& entry);

  private:
    explicit NotebookLibraryService(QObject* parent = nullptr);
    void load_catalog();
    QString seed_marker_path() const;

    QVector<NotebookCatalogEntry> catalog_;
    bool loaded_ = false;
    QString assets_dir_cache_;
    bool assets_dir_resolved_ = false;
};

} // namespace fincept::services
