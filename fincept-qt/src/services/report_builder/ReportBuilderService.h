#pragma once
// ReportBuilderService — Single source of truth for the active report document.
//
// Why a service: per CLAUDE.md §P6 (Service Layer Separation), screens render
// UI only. The report document state must live outside the screen so:
//   • LLM/MCP tools and other tabs (ReportInbox) can add to the document even when
//     the screen isn't open.
//   • State survives screen open/close (factory-registered screen recreates).
//   • Manual edits, data refreshes and inbound blocks go through the same undo stack.
//   • Autosave runs regardless of UI visibility — but only after an edit.
//
// Persistence: the open report is a row of the report library (`reports` table via
// ReportLibrary, synced to the cloud when Settings > Cloud Sync > Reports is on). Edits
// are written to that row a few seconds after they stop (and on quit); a report that
// was opened from / saved to a .fincept file is also written back to that file on Save.
// The recovery copy (report_autosave.fincept) is kept as before.
//
// Threading: the service object lives on the main thread. All public mutators
// must be invoked there. Workers post via QMetaObject::invokeMethod. Reads
// (components()/metadata()/theme()/snapshot()) are guarded by a mutex and are safe
// from any thread.

#include "core/report/ReportDocument.h"
#include "core/result/Result.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUndoStack>
#include <QVariantMap>
#include <QVector>

#include <cstddef>

namespace fincept::services {

namespace detail {
class ServiceFriend; // defined in ReportBuilderService.cpp
}

class ReportBuilderService : public QObject {
    Q_OBJECT
  public:
    static ReportBuilderService& instance();

    // ── Read API (thread-safe) ────────────────────────────────────────────────

    QVector<report::ReportComponent> components() const;
    report::ReportMetadata metadata() const;
    report::ReportTheme theme() const;
    int next_id() const;

    // Find by stable id; -1 if not found.
    int index_of(int id) const;
    /// Copy of one component by stable id; false when the id is unknown.
    bool component(int id, report::ReportComponent* out) const;

    QString current_file() const;
    QStringList recent_files() const;

    /// Library row of the open report (reports.id); 0 = not stored in the library yet.
    qint64 library_id() const;

    // Snapshot the entire doc. Useful for serialization, MCP get_state,
    // PDF export.
    report::ReportDocument snapshot() const;

    /// Blocks other tabs added that the user has not looked at yet (ids still present).
    QVector<int> unseen_ids() const;
    QStringList unseen_sources() const;

    /// The open report has edits that were not written to its .fincept file
    /// (the library copy is kept current automatically).
    bool file_dirty() const { return file_dirty_; }

    // ── Write API (main thread) ──────────────────────────────────────────────
    //
    // All mutators push onto undo_stack_ (so manual undo covers LLM edits).
    // They emit the appropriate fine-grained signal AND document_changed.

    /// Insert a component. insert_at: -1 = end, otherwise clamped to [0,size].
    /// Returns the assigned stable id.
    int add_component(const report::ReportComponent& comp, int insert_at = -1);

    /// Append several blocks as ONE undo step ("Add from <source_label>"); returns
    /// their ids in order. insert_at -1 = end. When `mark_unseen`, they are listed
    /// as new for the "N new blocks" notice.
    QVector<int> append_components(const QVector<report::ReportComponent>& blocks, const QString& source_label,
                                   int insert_at = -1, bool mark_unseen = false);

    /// Update content/config by stable id. config replaces (not merges).
    /// Returns true if the id was found.
    bool update_component(int id, const QString& content, const QMap<QString, QString>& config);

    /// Patch update — updates content if non-null, merges config (only the
    /// keys present in `partial_config` are touched; an EMPTY value removes the key
    /// when `empty_removes`). `undo_text` names the undo step (default "Edit
    /// component"); `mergeable` = false keeps it a step of its own (data refreshes),
    /// instead of folding it into the next edit of the same block.
    bool patch_component(int id, const QString* content_or_null, const QMap<QString, QString>& partial_config,
                         const QString& undo_text = {}, bool mergeable = true, bool empty_removes = false);

    /// Remove by stable id. Returns true if removed.
    bool remove_component(int id);

    /// Move by stable id to new index. new_index clamped to [0,size-1].
    bool move_component(int id, int new_index);

    void set_metadata(const report::ReportMetadata& meta);
    void set_theme(const report::ReportTheme& theme);

    /// Apply a built-in template by name (e.g. "Investment Memo"). Clears
    /// the doc, sets metadata, populates components. `params` feeds the
    /// self-filling templates ({"symbol": "AAPL"} / {"portfolio_id": ...}).
    void apply_template(const QString& name, const QVariantMap& params = {});

    /// Reset to an empty document.
    void clear_document();

    /// Replace the entire document state (used by load). Keeps the library row and file.
    void replace_document(const report::ReportDocument& doc);

    /// Make `doc` the open report: library row `library_id` (0 = a new row is created
    /// on the next save), linked file `file_path` (may be empty). Clears undo.
    void open_document(const report::ReportDocument& doc, qint64 library_id, const QString& file_path);
    /// A .fincept file read and parsed elsewhere (off the GUI thread): opened as the
    /// library row it was imported as before (or a new one), linked to `path`.
    void open_file_document(const report::ReportDocument& doc, const QString& path);
    /// "Save As": the open report becomes a NEW library report linked to `path` (the
    /// previous one keeps its last saved state), then is written to `path`.
    Result<void> fork_to_file(const QString& path);

    /// Detach the open report from its library row (the row was deleted); it is saved
    /// as a new row on the next edit.
    void detach_library(qint64 deleted_id);
    /// A delete was undone: re-attach the open report if it was detached from `id`.
    void reattach_library(qint64 id);

    /// Forget the "new blocks" notice.
    void mark_inbox_seen();

    // ── Undo ──────────────────────────────────────────────────────────────────

    QUndoStack* undo_stack();

    /// Open a macro that groups all subsequent mutations under one Ctrl+Z
    /// until end_macro() (or the LLM window timer fires). Re-entrant safe:
    /// nested begin_macro calls just bump a counter.
    void begin_macro(const QString& description);
    void end_macro();

    /// LLM-burst window: tool handlers call note_llm_mutation() which opens
    /// a macro on first call and (re)starts a 400ms timer; on timer expiry
    /// the macro is closed. Result: a chain of LLM mutations within ~400ms
    /// is one Ctrl+Z.
    void note_llm_mutation();

    // ── File ops ──────────────────────────────────────────────────────────────

    Result<void> save_to(const QString& path);
    Result<void> load_from(const QString& path);
    void set_current_file(const QString& path); // updates current_file_ + recent
    /// Remove paths that no longer exist from the recent list; returns what is left.
    QStringList prune_recent_files();

    // ── Library ──────────────────────────────────────────────────────────────

    /// Write the open report to its library row now (creates the row when there is
    /// none and the report has content). Asynchronous; never blocks.
    void save_to_library();
    /// Synchronous variant for shutdown (main thread, main DB connection).
    void save_to_library_blocking();

    // ── Stale saves (two computers editing one synced report) ──────────────────
    /// The library refused a save: the stored report changed since the open copy was
    /// loaded (another computer saved it). Library writes pause until resolved; the
    /// recovery copy keeps every edit meanwhile.
    struct LibraryConflict {
        qint64 id = 0;
        QString theirs_title;
        QString theirs_updated; ///< SQLite UTC text
        report::ReportDocument theirs;
    };
    bool has_library_conflict() const { return conflict_.id > 0; }
    LibraryConflict library_conflict() const { return conflict_; }
    enum class ConflictChoice {
        KeepMine,   ///< overwrite the stored version with the open one
        TakeTheirs, ///< open the stored version (the open edits are dropped)
        KeepBoth    ///< the open version becomes a new library report "… (my copy)"
    };
    void resolve_library_conflict(ConflictChoice choice);

    // ── Identity for new reports ─────────────────────────────────────────────
    /// Author / company a new report starts with: the user's own (Document info › "use for
    /// new reports"), else author "Analyst" and an empty company.
    static void apply_identity_defaults(report::ReportMetadata& meta);
    static QString default_company();
    static QString default_author();
    static void set_identity_defaults(const QString& author, const QString& company);

    // ── Open requests from elsewhere (Files screen, other tabs) ─────────────────
    struct OpenRequest {
        QString file_path;      ///< a .fincept file, or
        qint64 library_id = 0;  ///< a library row (0 with an empty path = the open report)
        int select_block_id = 0;
    };
    /// Park a request; the Report Builder screen takes it when it shows (or at once
    /// when it is visible — open_requested()).
    void request_open(const OpenRequest& request);
    bool take_open_request(OpenRequest* out);

    // ── Autosave ─────────────────────────────────────────────────────────────

    QString autosave_path() const;
    /// Write the recovery copy now (normally armed by the first edit after a save and
    /// run once, 60 s later — never on a timer while the report is unchanged).
    void trigger_autosave();

  signals:
    // Coarse — fires after any mutation. Screens can hook this for a full
    // re-render. Per-mutation signals fire alongside for incremental updates.
    void document_changed();

    void component_added(int id, int index);
    void component_updated(int id);
    void component_removed(int id, int prior_index);
    void component_moved(int id, int from_index, int to_index);
    void metadata_changed();
    void theme_changed();
    // Fired after load_from() (path = the file) and after replace_document() / a template
    // is applied (path empty). Views rebuild everything — canvas, structure, selection.
    void document_loaded(const QString& path);
    void document_cleared();
    void current_file_changed(const QString& path);
    void library_id_changed(qint64 id);
    /// The library copy was written (id) or could not be (error).
    void library_saved(qint64 id);
    void library_save_failed(const QString& error);
    /// The unseen-blocks list changed (blocks appended by another tab, or seen).
    void inbox_changed();
    /// A parked open request is waiting (see request_open()).
    void open_requested();
    /// A save was refused because the stored report changed elsewhere (see LibraryConflict).
    void library_conflict_detected();

    // Emitted when an LLM-driven mutation is about to happen and the report
    // builder is not currently visible. Listeners (NavigationBar) can react
    // to bring the tab into view.
    void navigate_requested();

  private:
    ReportBuilderService();

    // detail::ServiceFriend bypasses the public mutators (which would push
    // onto undo_stack_ recursively) and writes doc_ directly. It is used
    // exclusively by the QUndoCommand subclasses defined in the .cpp.
    friend class detail::ServiceFriend;

    void push_undo(QUndoCommand* cmd);
    void load_recent() const;
    void save_recent() const;
    void persist_current_file();
    void persist_library_id();
    void write_autosave(const QString& json);
    void set_library_id(qint64 id);
    bool worth_saving_locked() const;

    // Library writes, one chain per document identity (generation): at most one write
    // in flight per chain; a newer state waits as `next` and goes out with the row id the
    // previous write resolved (so a report never gets two rows, and the last state wins).
    struct WriteChain {
        qint64 id = 0;
        QString file;
        bool in_flight = false;
        bool has_next = false;
        QString next_title;
        QString next_json;
        report::ReportRevision expected; ///< the stored revision this chain's writes are based on
        bool conflict = false;           ///< writes paused: the row changed elsewhere
        bool force_next = false;         ///< the user chose "keep mine"
    };
    LibraryConflict conflict_;
    void request_library_write(quint64 generation, const QString& title, const QString& json);
    void start_library_write(quint64 generation);
    QHash<quint64, WriteChain> chains_;
    qint64 detached_from_ = 0;

    mutable QMutex mutex_;
    report::ReportDocument doc_;
    QUndoStack* undo_stack_;
    QTimer* autosave_timer_;
    QTimer* library_timer_;    // single-shot: library copy a few seconds after edits stop
    QTimer* llm_window_timer_; // single-shot, 400ms; closes the LLM macro
    int macro_depth_ = 0;
    QString current_file_;
    QString autosave_path_;
    std::size_t autosave_hash_ = 0; // hash of the JSON last written to the autosave file
    bool autosave_dirty_ = false;   // an edit is waiting for the autosave timer
    bool library_dirty_ = false;    // an edit is waiting for the library write
    bool library_saving_ = false;   // a write is in flight (a new edit re-arms after it)
    bool file_dirty_ = false;
    std::size_t library_hash_ = 0;  // hash of the JSON last written to the library
    qint64 library_id_ = 0;
    quint64 library_generation_ = 0; // bumped when the open report changes identity
    mutable QStringList recent_cache_;
    mutable bool recent_loaded_ = false;
    QVector<OpenRequest> open_requests_;
};

} // namespace fincept::services
