#pragma once
// ArtefactLogic.h — the pure rules behind run artefacts (WP5): markdown tables
// found in an agent's answer, CSV / Markdown / JSON forms of a table, safe file
// names, how a tool result is shown (table, key / value, text, error), which
// artefacts a run's steps and final answer produce, and which artefacts the
// retention / size policy removes.
//
// Qt Core only (unit-tested by tests/tst_run_artefacts): no files, no database,
// no widgets. ArtefactService does the I/O around these decisions.

#include "services/agent_tasks/TaskRunTypes.h"
#include "storage/repositories/NoteBoardTypes.h" // row types only (Qt Core), no repository

#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace fincept::agent_tasks::artefact_logic {

// ── Limits ───────────────────────────────────────────────────────────────────

inline constexpr qint64 kArtefactMaxBytes = 32LL * 1024 * 1024;      ///< one artefact's file
inline constexpr qint64 kArtefactQuotaBytes = 1024LL * 1024 * 1024;  ///< all unpinned artefacts together
inline constexpr double kQuotaTargetFraction = 0.9;                  ///< pruning stops below this share
inline constexpr int kInlineTableMaxCells = 4000;  ///< tables up to this size are also kept inline (instant preview)
inline constexpr int kMaxTablesPerText = 20;       ///< tables taken from one answer
inline constexpr int kMaxTableRows = 100000;       ///< rows read from one table
inline constexpr int kMaxTableColumns = 64;

// ── Tables ───────────────────────────────────────────────────────────────────

struct ArtefactTable {
    QStringList columns;
    QVector<QStringList> rows; ///< each padded / cut to columns.size()
    bool isEmpty() const { return columns.isEmpty() || rows.isEmpty(); }
    int cell_count() const { return static_cast<int>(columns.size() * rows.size()); }
};

struct MarkdownTable {
    ArtefactTable table;
    QString caption;    ///< the heading / lead-in line above it; empty when none
    int start_line = 0; ///< 0-based line of its header row
};

/// GitHub-style pipe tables in `markdown` (header row, delimiter row, body rows),
/// outside fenced code blocks; emphasis marks and <br> are removed from cells.
QVector<MarkdownTable> find_markdown_tables(const QString& markdown, int max_tables = kMaxTablesPerText);

/// RFC 4180 CSV (CRLF rows). Cells holding a comma, quote, line break or '|' are
/// quoted. A text cell that a spreadsheet would run as a formula (= + - @ first,
/// and not a number such as -1.5 or +2%) gets a leading apostrophe.
QString table_to_csv(const ArtefactTable& table);
/// Reads CSV (comma or tab separated, quoted cells, CRLF or LF); the first row is the header.
ArtefactTable table_from_csv(const QString& csv);
/// A GitHub-style pipe table; `max_rows` > 0 cuts the body with a note row.
QString table_to_markdown(const ArtefactTable& table, int max_rows = -1);
/// {"columns": [...], "rows": [[...], ...]}
QJsonObject table_to_json(const ArtefactTable& table);
/// Accepts {"columns", "rows"}, an array of objects (columns = every key; the
/// identifying ones — symbol, ticker, name, date, time … — first, the rest in
/// alphabetical order, as QJsonObject keeps no source order) and an array of
/// arrays (first row = header when it is all text).
ArtefactTable table_from_json(const QJsonValue& value);
/// Whether the text is a plain number as finance tables write them ("-1.5", "+2%",
/// "1,234.5", "$12", "3.2x").
bool looks_numeric(const QString& cell);

// ── Names ────────────────────────────────────────────────────────────────────

/// A file-name stem from a title: letters, digits, '-', '_' (others become '-'),
/// at most `max_chars`; "artefact" when nothing is left.
QString safe_file_stem(const QString& title, int max_chars = 48);
/// "<first 8 of id>-<stem>.<ext>" — unique per artefact, readable in a file browser.
QString stored_name_for(const QString& artefact_id, const QString& title, const QString& ext);
/// MIME type and file extension for a kind ("report" → text/markdown, md).
QString mime_for_kind(const QString& kind);
QString extension_for_kind(const QString& kind);

// ── Tool results ─────────────────────────────────────────────────────────────

enum class ResultShape { Empty, Table, KeyValue, Text, Error };

struct ToolResultView {
    ResultShape shape = ResultShape::Empty;
    ArtefactTable table;                       ///< Table
    QVector<QPair<QString, QString>> fields;   ///< KeyValue
    QString text;                              ///< Text / Error (and a caption for Table)
    QJsonValue value;                          ///< the JSON the view was made from (Undefined for text)
};

/// How a tool's result is shown. `value` is the full JSON when known (terminal
/// tools carry {success, data, error}); otherwise `fallback_text` (the cloud's
/// output preview) is parsed as JSON when it is JSON, else shown as text.
ToolResultView classify_tool_result(const QJsonValue& value, const QString& fallback_text, bool success);
/// One line for the arguments preview of a tool call (whitespace collapsed, cut).
QString summarize_arguments(const QString& args_preview, int max_chars = 120);

// ── Artefacts a run produces ─────────────────────────────────────────────────

struct ArtefactDraft {
    QString key;   ///< dedupe key (RunArtefact::source["key"]): "final", "step:2", "table:final:1", "table:step2:1"
    QString kind;  ///< kArtefact*
    QString title;
    int step_idx = -1;
    QString text;         ///< report / text content
    ArtefactTable table;  ///< table content
    QJsonObject source;   ///< provenance (origin, key, step, deliverable …)
};

/// Whether `run` makes artefacts by itself: planned / local / scheduled / NEW RUN
/// tasks do; a plain chat reply does not (its answer is in the conversation).
bool run_produces_artefacts(const TaskRun& run);

/// The artefacts `run` should have that are not in `existing_keys` yet: each
/// finished step's deliverable (when the plan has more than one step) and its
/// tables; once the run has ended, its final report (or partial results) and
/// that report's tables. `only_step` >= 0 limits it to that step.
QVector<ArtefactDraft> plan_run_artefacts(const TaskRun& run, const QVector<RunStep>& steps,
                                          const QSet<QString>& existing_keys, int only_step = -1);

/// The dedupe key of an artefact (source["key"]); empty for user-saved ones.
QString artefact_key(const RunArtefact& a);

// ── Retention ────────────────────────────────────────────────────────────────

struct RetentionInput {
    QVector<RunArtefact> artefacts;
    QSet<QString> live_run_ids;   ///< runs that still exist
    QSet<QString> active_run_ids; ///< runs that are not finished (their artefacts are never pruned for size)
    qint64 quota_bytes = kArtefactQuotaBytes;
};

struct RetentionPlan {
    QStringList delete_ids; ///< artefacts to remove (rows and files)
    qint64 bytes_before = 0;
    qint64 bytes_after = 0;
};

/// Unpinned artefacts whose run is gone are removed; then, while the unpinned
/// artefacts of finished runs together exceed the quota, the oldest go until
/// they are below kQuotaTargetFraction of it. Pinned artefacts always stay.
RetentionPlan plan_retention(const RetentionInput& in);

/// Names in `files` (one run folder) that no artefact row refers to — leftovers
/// of a crash between writing a file and recording it, or of a deleted row.
QStringList unreferenced_files(const QStringList& files, const QSet<QString>& referenced_names);

// ── Notes hand-off ("Send to Notes") ─────────────────────────────────────────

/// Payload of the "notes.created" event published once the note exists: its id,
/// the source the Notes board reads as "created from a Chat Mode run"
/// (notes_board::kOriginAgentRuns), the run id and — when the artefact belongs to
/// one step — the 0-based step. The Notes screen keeps the run id as the note's
/// origin_ref, which its OPEN CHAT MODE action hands to Chat Mode.
inline QVariantMap note_created_event(int note_id, const QString& run_id, int step_idx) {
    QVariantMap ev{{QStringLiteral("id"), note_id}, {QStringLiteral("source"), notes_board::kOriginAgentRuns}};
    if (!run_id.isEmpty())
        ev.insert(QStringLiteral("run_id"), run_id);
    if (step_idx >= 0)
        ev.insert(QStringLiteral("step"), step_idx);
    return ev;
}

/// The Notes board row of a note made from run `run_id` — what the Notes screen
/// would write for a new note of `category` (default status, newest-first rank),
/// plus the run it came from, so the link survives when the Notes screen was not
/// open to hear the event. `now_secs`: seconds since the epoch.
inline notes_board::NoteBoardMeta note_board_row_for_run(int note_id, const QString& category, const QString& run_id,
                                                         qint64 now_secs) {
    notes_board::NoteBoardMeta m;
    m.note_id = note_id;
    m.status = notes_board::default_status(category, false);
    m.rank = notes_board::default_rank(now_secs);
    m.origin = notes_board::kOriginAgentRuns;
    m.origin_ref = run_id;
    m.stored = true;
    return m;
}

} // namespace fincept::agent_tasks::artefact_logic
