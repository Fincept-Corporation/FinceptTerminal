// src/storage/repositories/NoteBoardTypes.h
//
// Row types of the Notes board tables (migration v066) and the one place that
// decides a note's default board status. Qt Core only: the migration, the
// repository, the Notes screen and the unit tests all include it, so the SQL
// backfill and the C++ default can never disagree.
//
//   note_board_meta     one row per note: workflow status, manual order (rank),
//                       review date, origin ("agent_runs" + run id), times
//   note_board_history  status / review events per note (the detail pane's history)
//   note_boards         named boards = saved views (definition JSON)
//   note_wall_items     free-form wall position of a note on a board
//   note_wall_groups    user-drawn titled groups on a board's wall
#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

namespace fincept::notes_board {

// ── Workflow status ─────────────────────────────────────────────────────────

inline const QString kStatusIdea = QStringLiteral("IDEA");
inline const QString kStatusResearching = QStringLiteral("RESEARCHING");
inline const QString kStatusActive = QStringLiteral("ACTIVE");
inline const QString kStatusReview = QStringLiteral("REVIEW");
inline const QString kStatusClosed = QStringLiteral("CLOSED");

/// The workflow, in lane order.
inline QStringList status_codes() {
    return {kStatusIdea, kStatusResearching, kStatusActive, kStatusReview, kStatusClosed};
}

/// A status as stored ("active " -> "ACTIVE"); anything unknown -> IDEA.
inline QString normalize_status(const QString& s) {
    const QString u = s.trimmed().toUpper();
    return status_codes().contains(u) ? u : kStatusIdea;
}

/// Category -> status for notes that have no board row yet (rows written by the
/// migration for existing notes, and notes created later by the dashboard tile,
/// Chat Mode, MCP tools or a cloud pull). Archived notes are CLOSED.
struct DefaultStatusRule {
    const char* category; ///< category_key() form (upper case)
    const char* status;
};
inline constexpr DefaultStatusRule kDefaultStatusRules[] = {
    {"TRADE_IDEA", "IDEA"},           {"RESEARCH", "RESEARCHING"}, {"MARKET_ANALYSIS", "RESEARCHING"},
    {"EARNINGS", "RESEARCHING"},      {"ECONOMIC", "RESEARCHING"}, {"PORTFOLIO", "ACTIVE"},
};

inline QString default_status(const QString& category, bool archived) {
    if (archived)
        return kStatusClosed;
    const QString c = category.trimmed().toUpper();
    for (const auto& r : kDefaultStatusRules)
        if (c == QLatin1String(r.category))
            return QString::fromLatin1(r.status);
    return kStatusIdea;
}

/// The same rule as an SQL expression over financial_notes columns (used by the
/// migration backfill). Built from kDefaultStatusRules, never written by hand.
inline QString default_status_sql(const QString& category_col, const QString& archived_col) {
    QString sql = QStringLiteral("CASE WHEN COALESCE(%1, 0) <> 0 THEN '%2'").arg(archived_col, kStatusClosed);
    for (const auto& r : kDefaultStatusRules)
        sql += QStringLiteral(" WHEN UPPER(TRIM(COALESCE(%1, ''))) = '%2' THEN '%3'")
                   .arg(category_col, QLatin1String(r.category), QLatin1String(r.status));
    sql += QStringLiteral(" ELSE '%1' END").arg(kStatusIdea);
    return sql;
}

/// Default manual order: newest edit first. `updated_secs` = seconds since the
/// epoch of the note's last edit (creation when never edited). Ranks sort
/// ascending, so a more recent note gets a smaller (more negative) rank.
inline double default_rank(qint64 updated_secs) {
    return -static_cast<double>(updated_secs);
}

/// SQL form of default_rank over financial_notes columns (SQLite reads both the
/// "YYYY-MM-DD HH:MM:SS" UTC form and ISO 8601).
inline QString default_rank_sql(const QString& updated_col, const QString& created_col) {
    return QStringLiteral("-CAST(COALESCE(strftime('%s', NULLIF(%1, '')), strftime('%s', NULLIF(%2, '')), '0') AS REAL)")
        .arg(updated_col, created_col);
}

/// Tag Chat Mode puts on the notes it saves ("Send to Notes" in the artefacts panel).
inline const QString kAgentRunTag = QStringLiteral("agent-run");
/// note_board_meta.origin of a note created from a Chat Mode run.
inline const QString kOriginAgentRuns = QStringLiteral("agent_runs");

// ── Rows ─────────────────────────────────────────────────────────────────────

struct NoteBoardMeta {
    int note_id = 0;
    QString status = kStatusIdea;
    double rank = 0.0;
    QString review_on;            ///< "yyyy-MM-dd" or empty
    QString origin;               ///< "" | "agent_runs" | "import" | "template"
    QString origin_ref;           ///< e.g. the Chat Mode run id
    qint64 status_changed_ms = 0; ///< last status change (0 = never recorded)
    qint64 reviewed_ms = 0;       ///< last "mark reviewed"
    bool stored = false;          ///< a row exists in note_board_meta

    QDate review_date() const { return review_on.isEmpty() ? QDate() : QDate::fromString(review_on, Qt::ISODate); }
    bool same_values(const NoteBoardMeta& o) const {
        return note_id == o.note_id && status == o.status && rank == o.rank && review_on == o.review_on &&
               origin == o.origin && origin_ref == o.origin_ref && status_changed_ms == o.status_changed_ms &&
               reviewed_ms == o.reviewed_ms;
    }
};

struct NoteBoardEvent {
    qint64 id = 0;
    int note_id = 0;
    qint64 at_ms = 0;
    QString kind; ///< "status" | "review" | "reviewed" | "created"
    QString from_value;
    QString to_value;
};

struct NoteBoardRecord {
    qint64 id = 0;
    QString name;
    int position = 0;
    QString definition_json; ///< NotesBoardLogic BoardDef::to_json
    qint64 created_ms = 0;
    qint64 updated_ms = 0;
};

struct NoteWallItem {
    int note_id = 0;
    double x = 0.0;
    double y = 0.0;
};

struct NoteWallGroup {
    qint64 id = 0;
    qint64 board_id = 0;
    QString title;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    int tone = 0; ///< index into the categorical chart colours
};

} // namespace fincept::notes_board
