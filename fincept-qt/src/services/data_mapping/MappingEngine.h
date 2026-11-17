#pragma once
// MappingEngine — applies a MappingSpec to a DataTable: one output row per input row, every
// cell cast to its target type with an explicit, per-field rule. A cell that cannot be cast is
// marked (row, column, reason) and left null — the run never fails as a whole because one value
// is bad. Pure (QtCore only); runs on worker threads for large tables.

#include "services/data_mapping/MappingSpec.h"
#include "services/data_sources/TabularData.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonValue>
#include <QString>
#include <QVector>

namespace fincept::services::datamapping {

struct CastResult {
    QJsonValue value;    ///< the cast value (Null when empty or failed)
    bool empty = false;  ///< the input was null / a null token
    QString error;       ///< non-empty = the value could not be cast (plain sentence)
};

struct CellIssue {
    int row = -1;        ///< input row index
    int column = -1;     ///< output column index (field index)
    QString message;
};

struct FieldStats {
    int ok = 0;
    int empty = 0;
    int errors = 0;
    QString first_error; ///< "row 12: '31.02.2024' is not a date in format dd.MM.yyyy"
};

struct MappingOutput {
    fincept::services::datasources::DataTable table; ///< target columns, target types, cast rows
    QVector<int> source_rows;           ///< output row → input row
    QVector<CellIssue> issues;          ///< every cell problem (capped at kMaxIssues)
    QHash<qint64, int> issue_index;     ///< (output row << 20 | column) → index into issues
    QVector<FieldStats> field_stats;    ///< per field
    int rows_in = 0;
    int rows_out = 0;
    int rows_dropped_invalid = 0;
    int duplicates = 0;                 ///< rows sharing key fields with an earlier row
    int rows_dropped_duplicate = 0;
    int error_cells = 0;
    QStringList spec_problems;          ///< problems with the mapping itself (unknown source column, …)

    static constexpr int kMaxIssues = 20000;
    /// The issue of an output cell, or nullptr.
    const CellIssue* issue_at(int out_row, int column) const;
    bool clean() const { return error_cells == 0 && spec_problems.isEmpty(); }
};

/// Apply `spec` to `input`. `max_rows` < 0 = all rows (a preview passes e.g. 200).
MappingOutput apply_mapping(const fincept::services::datasources::DataTable& input, const MappingSpec& spec,
                            int max_rows = -1);

// ── The individual casts (exposed for tests and the grid's live hints) ─────

CastResult cast_cell(const QJsonValue& input, const FieldMapping& field, const MappingOptions& options);

/// Number text in "dot" (1,234.5), "comma" (1.234,5) or "auto" convention; "12%" → 12, "(5)" → -5,
/// currency signs and spaces stripped. Sets *ok.
double parse_number_text(const QString& text, const QString& decimal, bool* ok);
/// true / false from yes/no, y/n, 1/0, on/off, true/false (any case). Sets *ok.
bool parse_bool_text(const QString& text, bool* ok);

/// A period code ("2024", "2024-Q1", "2024Q1", "Q1 2024", "2024-03", "2024M03", "2024-H1", "FY2024")
/// as its canonical code ("2024", "2024-Q1", "2024-03", "2024-H1") plus first/last day.
struct Period {
    QString code;
    QDate start;
    QDate end;
    bool valid() const { return start.isValid() && end.isValid(); }
};
Period parse_period(const QString& text);

/// A date/time from text or a number by `format` (see FieldMapping::format). Date-only inputs give
/// a midnight UTC datetime. `*is_date_only` tells the caller whether a time was present.
QDateTime parse_datetime(const QJsonValue& v, const QString& format, bool* ok, bool* is_date_only = nullptr,
                         QString* why = nullptr);

} // namespace fincept::services::datamapping
