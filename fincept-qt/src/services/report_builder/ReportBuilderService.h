#pragma once
// ReportBuilderService — Single source of truth for the active report document.
//
// Why a service: per CLAUDE.md §P6 (Service Layer Separation), screens render
// UI only. The report document state must live outside the screen so:
//   • LLM/MCP tools can mutate the document even when the screen isn't open.
//   • State survives screen open/close (factory-registered screen recreates).
//   • Manual edits and LLM-driven edits go through the same undo stack.
//   • Autosave runs regardless of UI visibility.
//
// Threading: the service object lives on the main thread. All public mutators
// must be invoked there. Workers (e.g. LLM tool handlers running on
// QtConcurrent::run threads) post via QMetaObject::invokeMethod with
// Qt::BlockingQueuedConnection, which is what the dispatch helper does
// internally. Reads (components()/metadata()/theme()) are guarded by a mutex
// and are safe from any thread.

#include "core/report/ReportDocument.h"
#include "core/result/Result.h"

#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUndoStack>
#include <QVector>

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

    QString current_file() const;
    QStringList recent_files() const;

    // Snapshot the entire doc. Useful for serialization, MCP get_state,
    // PDF export.
    report::ReportDocument snapshot() const;

    // ── Write API (main thread) ──────────────────────────────────────────────
    //
    // All mutators push onto undo_stack_ (so manual undo covers LLM edits).
    // They emit the appropriate fine-grained signal AND document_changed.
    //
    // Workers should call dispatch_blocking(...) (see helpers below) rather
    // than these directly.

    /// Insert a component. insert_at: -1 = end, otherwise clamped to [0,size].
    /// Returns the assigned stable id.
    int add_component(const report::ReportComponent& comp, int insert_at = -1);

    /// Update content/config by stable id. config replaces (not merges).
    /// Returns true if the id was found.
    bool update_component(int id, const QString& content, const QMap<QString, QString>& config);

    /// Patch update — updates content if non-null, merges config (only the
    /// keys present in `partial_config` are touched).
    bool patch_component(int id, const QString* content_or_null, const QMap<QString, QString>& partial_config);

    /// Remove by stable id. Returns true if removed.
    bool remove_component(int id);

    /// Move by stable id to new index. new_index clamped to [0,size-1].
    bool move_component(int id, int new_index);

    void set_metadata(const report::ReportMetadata& meta);
    void set_theme(const report::ReportTheme& theme);

    /// Apply a built-in template by name (e.g. "Investment Memo"). Clears
    /// the doc, sets metadata, populates components. Pushes a single
    /// macro onto the undo stack.
    void apply_template(const QString& name);

    /// Reset to an empty document.
    void clear_document();

    /// Replace the entire document state (used by load).
    void replace_document(const report::ReportDocument& doc);

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

    // ── Autosave ─────────────────────────────────────────────────────────────

    QString autosave_path() const;
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
    void document_loaded(const QString& path);
    void document_cleared();
    void current_file_changed(const QString& path);

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

    mutable QMutex mutex_;
    report::ReportDocument doc_;
    QUndoStack* undo_stack_;
    QTimer* autosave_timer_;
    QTimer* llm_window_timer_; // single-shot, 400ms; closes the LLM macro
    int macro_depth_ = 0;
    QString current_file_;
    QString autosave_path_;
    mutable QStringList recent_cache_;
    mutable bool recent_loaded_ = false;
};

} // namespace fincept::services
