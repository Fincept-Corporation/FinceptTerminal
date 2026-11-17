#pragma once
// ReportLibrary — the report library on the existing `reports` / `report_templates`
// tables (ReportRepository; v001), so library reports sync through ReportCloudAdapter
// when Settings > Cloud Sync > Reports is on.
//
// Every database call runs on ONE private worker thread (serialised: a write is never
// overtaken by a later read or write of the same row); results come back on the GUI
// thread and are dropped when `ctx` was destroyed. Nothing here blocks the GUI thread.
//
// Delete is deferred: remove_later() hides the row at once and deletes it after a
// grace period unless undo_remove() is called (pending deletes are committed on quit).
// No schema change: library extras (unseen blocks, page setup, theme, the revision)
// live inside the report JSON itself.
//
// Revisions: every write stamps the report JSON with {"revision": {n, by}}. The open
// report saves with save_checked(expected): when the row's revision is not the one the
// open copy was based on (another computer saved it and cloud sync brought it here), the
// write is refused and the caller gets THEIR version to offer "keep mine / take theirs /
// keep both" — last-save-wins no longer overwrites silently. Rows written by builds that
// knew no revisions have none; that is a valid base too.
// Pictures are embedded into the stored row on the worker (ReportAssets.h) so a synced
// report carries them; loading writes them back into the assets folder.

#include "core/report/ReportDocument.h"
#include "services/report_builder/ReportLibraryLogic.h" // LibraryEntry, TemplateEntry, library_logic

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <functional>
#include <memory>

class QThreadPool;
class QTimer;

namespace fincept::services {

/// Result of a revision-checked write.
struct LibrarySaveOutcome {
    bool ok = false;
    qint64 id = 0;
    report::ReportRevision revision; ///< what the row carries now (ok)
    bool conflict = false;           ///< refused: the row changed since `expected`
    report::ReportDocument theirs;   ///< conflict: the stored version
    QString theirs_title;
    QString theirs_updated;          ///< SQLite UTC text of the stored version
    QString error;
};

class ReportLibrary : public QObject {
    Q_OBJECT
  public:
    static ReportLibrary& instance();
    ~ReportLibrary() override;

    /// Short stable id of this installation (stamped into revisions as "by").
    static QString installation_id();

    using ListDone = std::function<void(bool ok, const QVector<LibraryEntry>& entries, const QString& error)>;
    using LoadDone = std::function<void(bool ok, const report::ReportDocument& doc, const QString& title,
                                        const QString& error)>;
    using SaveDone = std::function<void(bool ok, qint64 id, const QString& error)>;
    using Done = std::function<void(bool ok, const QString& error)>;
    using AppendDone = std::function<void(bool ok, const QVector<int>& ids, const QString& title, const QString& error)>;
    using TemplatesDone = std::function<void(bool ok, const QVector<TemplateEntry>& entries, const QString& error)>;

    /// Reports matching `query` (title or content; empty = all), newest first.
    void list(const QString& query, int limit, QObject* ctx, ListDone done);
    void load(qint64 id, QObject* ctx, LoadDone done);
    /// Write `json` to row `id` (0 = create a row). `done` gets the row id.
    void save(qint64 id, const QString& title, const QString& json, QObject* ctx, SaveDone done);
    /// Synchronous save for shutdown (main thread only). Returns the row id or 0.
    qint64 save_blocking(qint64 id, const QString& title, const QString& json);

    using CheckedSaveDone = std::function<void(const LibrarySaveOutcome&)>;
    /// Write `json` to row `id` only when the row's revision still equals `expected`
    /// (or the row is gone / id is 0: a new row). `force` writes regardless (the user
    /// chose "keep mine"). The JSON is stamped with the next revision and its pictures are
    /// embedded on the worker.
    void save_checked(qint64 id, const QString& title, const QString& json, const report::ReportRevision& expected,
                      bool force, QObject* ctx, CheckedSaveDone done);
    /// Synchronous variant for shutdown (main thread, main DB connection).
    LibrarySaveOutcome save_checked_blocking(qint64 id, const QString& title, const QString& json,
                                             const report::ReportRevision& expected, bool force);

    /// Run `work` on the library's database worker (one thread, its own SQLite connection)
    /// and hand its result to `done` on the GUI thread (dropped when `ctx` died). For small
    /// reads other report code needs (watchlists for a data block).
    void run_on_db_worker(std::function<QVariant()> work, QObject* ctx, std::function<void(const QVariant&)> done);
    /// Shutdown only: wait (at most `ms`) for queued library work to finish.
    void wait_for_idle(int ms);
    /// Title in the row AND inside the report JSON.
    void rename(qint64 id, const QString& title, QObject* ctx, Done done);
    void duplicate(qint64 id, QObject* ctx, SaveDone done);
    /// Append blocks to a stored report (ids assigned from its next_id), mark them unseen.
    void append_to(qint64 id, const QVector<report::ReportComponent>& blocks, const QString& source_label,
                   QObject* ctx, AppendDone done);

    /// Deferred delete (see the file comment). `grace_ms` before the row goes.
    void remove_later(qint64 id, int grace_ms = 8000);
    /// Cancel a pending delete; false when it already happened.
    bool undo_remove(qint64 id);
    bool is_pending_removal(qint64 id) const { return pending_removals_.contains(id); }

    // ── Templates ────────────────────────────────────────────────────────────
    /// Built-in templates (static catalogue, previews included). GUI thread, cheap.
    static QVector<TemplateEntry> builtin_templates();
    /// The user's own templates (report_templates rows).
    void list_user_templates(QObject* ctx, TemplatesDone done);
    void save_user_template(const QString& name, const QString& description, const QString& json, QObject* ctx,
                            Done done);
    void rename_user_template(const QString& id, const QString& name, QObject* ctx, Done done);
    void remove_user_template(const QString& id, QObject* ctx, Done done);
    /// A user template's document ("user:<id>").
    void load_user_template(const QString& id, QObject* ctx, LoadDone done);

    // ── Recently opened (newest first; QSettings) ────────────────────────────
    QVector<qint64> recent_ids() const;
    void note_opened(qint64 id);

    /// Summary of a document (also used for the open report). Pure, any thread.
    static LibraryEntry summarise(qint64 id, const QString& title, const report::ReportDocument& doc);

  signals:
    /// A row was written / removed here, or the cloud pull changed `reports`.
    void library_changed();
    /// A deferred delete was committed (the row is gone).
    void report_removed(qint64 id);

  private:
    ReportLibrary();
    Q_DISABLE_COPY(ReportLibrary)

    template <typename Fn>
    void on_worker(Fn&& fn);
    void commit_removal(qint64 id);
    void ensure_cloud_hook();

    QThreadPool* pool_ = nullptr;
    bool cloud_hooked_ = false;
    QHash<qint64, QTimer*> pending_removals_;
};

} // namespace fincept::services
