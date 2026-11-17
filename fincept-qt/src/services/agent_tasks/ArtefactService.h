#pragma once
// ArtefactService.h — what runs produce, kept as files and rows (WP5).
//
// What becomes an artefact
//   * A task run's final report (Markdown) when it ends, and its tables (CSV).
//   * Each finished step's deliverable of a planned run, and its tables — as the
//     step ends, so a long run shows its results while it works.
//   * Whatever the user keeps: a tool result ("Save as artefact" on a tool card),
//     a reply. The cloud agent flags nothing as an artefact itself (its tool-end
//     event carries only an output preview), so tool results become artefacts
//     only through the user.
//   A plain chat reply makes none (its answer is in the conversation).
//
// Where it lives
//   * Files in run_folder(run_id) under the profile's files directory, written
//     atomically (QSaveFile) on a private worker thread; the row (agent_run_
//     artefacts, v063) is written after its file, so a crash leaves at most an
//     unreferenced file, which the start-up sweep removes.
//   * Rows: this service is the one index (TaskRunService::artefacts() asks it).
//   * Retention: unpinned artefacts are deleted with their run (and when their run
//     is pruned); above kArtefactQuotaBytes the oldest unpinned artefacts of
//     finished runs go. Pinned ones always stay, also after their run is deleted.
//
// "Add to report": the ARTEFACTS tab builds a report::ReportPayload from the
// loaded content (ReportHandoff.h) and sends it through ui::AddToReportButton /
// services::ReportInbox.
//
// Threading: GUI-thread singleton; all file and database work on one private
// worker thread (ordered); callbacks arrive on the GUI thread and are dropped
// when `ctx` died. initialize_deferred() is called by TaskRunService.

#include "services/agent_tasks/ArtefactLogic.h"
#include "services/agent_tasks/TaskRunTypes.h"

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>

namespace fincept::agent_tasks {

/// An artefact's content as the preview shows it (loaded off the GUI thread).
struct ArtefactContent {
    bool ok = false;
    QString error;
    QString kind;
    QString text;                          ///< report / text / json (pretty) content
    artefact_logic::ArtefactTable table;   ///< table / series / tabular json
    QImage image;                          ///< image artefacts
    QByteArray image_bytes;                ///< …and their encoded bytes ("Add to report")
    QString image_format;                  ///< "png" | "jpg"
    QString file_path;                     ///< the file on disk (empty for inline-only)
    qint64 bytes = 0;
};

class ArtefactService : public QObject {
    Q_OBJECT
  public:
    static ArtefactService& instance();
    ~ArtefactService() override;

    /// Idempotent. Connects to TaskRunService (step ends, run ends, deletions).
    void initialize_deferred();

    /// <profile files>/agent-runs
    static QString root_folder();
    /// Folder for the files of `run_id` (created on first write). Cheap: no I/O.
    static QString run_folder(const QString& run_id);
    /// Absolute path of a stored artefact's file; empty for inline-only ones. No I/O.
    static QString file_path(const RunArtefact& artefact);

    // ── Index (GUI thread, no I/O) ───────────────────────────────────────────
    bool is_loaded() const;
    /// The artefacts of `run_id`, oldest first.
    QVector<RunArtefact> artefacts(const QString& run_id) const;
    /// Every pinned artefact, newest first — those of deleted runs included.
    QVector<RunArtefact> pinned_artefacts() const;
    std::optional<RunArtefact> find(const QString& artefact_id) const;
    /// Bytes of all artefacts on disk (pinned included).
    qint64 total_bytes() const;

    using DoneCallback = std::function<void(bool ok, const QString& artefact_id, const QString& error)>;
    using ListCallback = std::function<void(bool ok, const QStringList& artefact_ids, const QString& error)>;
    using ResultCallback = std::function<void(bool ok, const QString& error)>;
    using ContentCallback = std::function<void(const ArtefactContent& content)>;
    using NoteCallback = std::function<void(bool ok, int note_id, const QString& error)>;

    // ── Making artefacts ─────────────────────────────────────────────────────
    /// Write the run's final answer as a markdown artefact.
    void save_final_report(const QString& run_id, QObject* ctx, DoneCallback cb);
    /// One CSV artefact per markdown table in `markdown` (a step's answer).
    void extract_tables(const QString& run_id, int step_idx, const QString& markdown, QObject* ctx, ListCallback cb);
    /// Keep text (Markdown for kind "report", plain for "text") as an artefact.
    void save_text(const QString& run_id, int step_idx, const QString& kind, const QString& title, const QString& text,
                   const QJsonObject& source, QObject* ctx, DoneCallback cb);
    /// Keep a table (CSV file; also inline when small).
    void save_table(const QString& run_id, int step_idx, const QString& title, const artefact_logic::ArtefactTable& table,
                    const QJsonObject& source, QObject* ctx, DoneCallback cb);
    /// Keep a tool's result: a table when it is tabular, else its JSON (or text).
    void save_tool_result(const QString& run_id, int step_idx, const QString& tool, const QJsonValue& result,
                          const QString& preview_text, const QJsonObject& source, QObject* ctx, DoneCallback cb);
    /// Make the artefacts the run's steps / final answer should have and do not
    /// have yet (deduplicated by their source key). `only_step` >= 0: that step.
    void ensure_extracted(const QString& run_id, int only_step = -1);
    /// An inline row (TaskRunService::add_artefact): recorded and persisted.
    QString add_row(const QString& run_id, const RunArtefact& artefact);

    // ── Using artefacts ──────────────────────────────────────────────────────
    void load_content(const QString& artefact_id, QObject* ctx, ContentCallback cb);
    /// Copy an artefact's content to `dest_path` (Save as…); the extension picks
    /// the format: .csv / .xlsx (tables), .md / .txt (reports), .json, or the file as is.
    void export_artefact(const QString& run_id, const QString& artefact_id, const QString& dest_path, QObject* ctx,
                         ResultCallback cb);
    /// Register an artefact's file with FileManagerService (Files screen).
    void register_with_file_manager(const QString& run_id, const QString& artefact_id, QObject* ctx, ResultCallback cb);
    /// Create a note (Notes) with the artefact's content and a back-reference to
    /// its run; `note_id` on success.
    void send_to_notes(const QString& artefact_id, QObject* ctx, NoteCallback cb);
    void set_pinned(const QString& artefact_id, bool pinned);
    void delete_artefact(const QString& artefact_id, QObject* ctx, ResultCallback cb);

    // ── Run lifecycle (TaskRunService) ───────────────────────────────────────
    /// The rows TaskRunService loaded at start-up; runs the retention sweep.
    void adopt_loaded(const QVector<RunArtefact>& rows);
    /// Runs were deleted: their unpinned artefacts go (rows and files).
    void on_runs_deleted(const QStringList& run_ids);

  signals:
    /// A file artefact finished writing (after TaskRunService::artefact_added for its row).
    void artefact_saved(const QString& run_id, const QString& artefact_id);
    /// Artefacts of `run_id` were added, changed (pin) or removed; empty: several runs.
    void artefacts_changed(const QString& run_id);

  private:
    ArtefactService();
    Q_DISABLE_COPY(ArtefactService)

    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::agent_tasks
