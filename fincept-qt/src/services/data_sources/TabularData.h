#pragma once
// TabularData — the one tabular model every data source produces and Data Mapping consumes.
//
// A saved connection (HTTP, file, SQL, stream, …), a catalogue connector run (the
// fincept_connector.py envelope) and a mapping run all end up as a TabularResult: one or
// more named tables of columns + column types + rows of JSON cells. Nothing downstream has
// to guess a shape again.
//
// QtCore only (unit-tested in tests/tst_data_sources_core.cpp).

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::datasources {

/// Column kinds — the connector contract's "types" vocabulary plus Unknown.
enum class ColumnType { Unknown, String, Integer, Number, Bool, Date, DateTime, Object };

/// "string" | "integer" | "number" | "bool" | "date" | "datetime" | "object" | "" (Unknown).
QString column_type_name(ColumnType t);
/// Inverse of column_type_name(); also accepts "int", "float", "double", "boolean", "text", "timestamp".
ColumnType column_type_from_name(const QString& name);

/// The kind of one JSON cell (Unknown for null / undefined).
ColumnType cell_type(const QJsonValue& v);

struct DataTable {
    QString name;                ///< "" for the single table of a result, else its name (dotted path)
    QStringList columns;
    QVector<ColumnType> types;   ///< same length as columns (Unknown where not known)
    QVector<QJsonArray> rows;    ///< each row has columns.size() cells

    int row_count() const { return int(rows.size()); }
    int column_count() const { return int(columns.size()); }
    int column_index(const QString& name) const;
    /// The cell, or Undefined when out of range.
    QJsonValue cell(int row, int column) const;
    bool is_empty() const { return rows.isEmpty() || columns.isEmpty(); }
};

struct TabularResult {
    QVector<DataTable> tables;   ///< at least one table on success
    QString source;              ///< human source label ("World Bank Open Data", "prices.csv", …)
    QDateTime as_of;             ///< when the data was produced (UTC)
    bool truncated = false;      ///< the source had more rows than were fetched
    QString next;                ///< pagination cursor, when the source reports one
    QJsonObject meta;            ///< anything else worth keeping (envelope meta.extra, driver notes)

    int index_of(const QString& table_name) const; ///< -1 when absent
    QStringList table_names() const;
    int total_rows() const;
    bool is_empty() const;
};

// ── Building ──────────────────────────────────────────────────────────────

/// Rows from an array of objects (columns in first-seen order across the first `scan_rows`
/// rows, capped at `max_columns`), arrays (columns "0", "1", …) or scalars (column "value").
DataTable table_from_json_array(const QJsonArray& rows, const QString& name = QString(), int scan_rows = 2000,
                                int max_columns = 256);
/// Same, with the caller's column order and type hints (the connector envelope's columns/types).
DataTable table_from_records(const QJsonArray& rows, const QStringList& columns, const QJsonObject& types,
                             const QString& name = QString());
/// A single object as one key/value table ("key", "value").
DataTable table_from_key_values(const QJsonObject& object, const QString& name = QString());
/// Fill Unknown column types from the first `scan_rows` non-null cells (mixed → String,
/// Integer+Number → Number). ISO dates in strings are recognised as Date / DateTime.
void infer_column_types(DataTable& table, int scan_rows = 2000);

// ── Serialising (cache, Data Mapping hand-off, AutoLoadPane payloads) ──────

QJsonObject table_to_json(const DataTable& table);
DataTable table_from_json(const QJsonObject& obj);
QJsonObject result_to_json(const TabularResult& result);
TabularResult result_from_json(const QJsonObject& obj);

// ── Export ────────────────────────────────────────────────────────────────

/// RFC 4180 CSV (header + rows, "\r\n" line ends, fields quoted when needed). Objects and arrays
/// are written as compact JSON. `max_rows` < 0 = all.
QByteArray table_to_csv(const DataTable& table, int max_rows = -1);
/// [ {column: value, …}, … ]
QJsonArray table_to_records(const DataTable& table);
/// Display text of a cell (numbers without trailing zeros, objects as compact JSON).
QString cell_text(const QJsonValue& v);

} // namespace fincept::services::datasources
