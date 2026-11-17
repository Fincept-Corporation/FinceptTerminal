#pragma once
// MappingSpec — what a Data Mapping is (v2): a source (saved connection / catalogue connector /
// file / inline HTTP request) whose rows are mapped, column by column, onto target fields with
// type-safe casts, explicit date / period parsing, units, defaults and duplicate handling.
//
// Stored as JSON in data_mappings.spec_json (migration v062); every save is kept as a revision.
// Mappings saved before v062 (one HTTP request + JSONPath per field, one record per run) are
// converted by spec_from_legacy() without losing anything.
// QtCore only.

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::datamapping {

enum class TargetType { String, Integer, Number, Bool, Date, DateTime, Period, Json };

QString target_type_name(TargetType t);           ///< "string" … "json"
TargetType target_type_from_name(const QString& s); ///< also accepts the legacy schema words ("datetime", "enum")

struct SourceRef {
    /// "connection" (saved Data Sources connection), "connector" (catalogue connector),
    /// "file" (local file read directly), "http" (inline request — mappings from before v062).
    QString kind;
    QString ref;          ///< connection id / connector name / file path / base URL
    QString entity;       ///< connection: table / endpoint path / sheet; connector: command
    QStringList args;     ///< connector arguments
    QJsonObject params;   ///< connection fetch parameters (query, limit, rowsPath, format, …)
    QString table;        ///< which table of a multi-table result ("" = the first)
    QJsonObject http;     ///< kind "http": base_url, endpoint, method, auth_type, headers, body, rows_path

    QJsonObject to_json() const;
    static SourceRef from_json(const QJsonObject& o);
    bool is_set() const { return !kind.isEmpty() && (!ref.isEmpty() || !http.isEmpty()); }
    /// One-line description: "connection prices_db · table prices", "connector data_fred · series GDP".
    QString describe() const;
};

struct FieldMapping {
    QString target;          ///< output column
    TargetType type = TargetType::String;
    QString source;          ///< input column ("" = use default / constant only)
    QString path;            ///< optional path inside the input cell (objects): "a.b", "[0].price"
    QString format;          ///< dates: "auto" | "iso" | "year" | "quarter" | "month" | "epoch_s" | "epoch_ms" |
                             ///< a Qt date format ("dd.MM.yyyy", "MM/dd/yyyy"); periods: "start" | "end" | "code"
    double scale = 1.0;      ///< numbers: multiply (0.01 = minor units → major; 1000 = thousands)
    QString default_value;   ///< used when the cell is null / empty (also a constant when source is "")
    bool required = false;   ///< null after the default = an error on that cell
    QString transform;       ///< legacy text transforms: upper | lower | trim | abs | unix_to_iso …

    QJsonObject to_json() const;
    static FieldMapping from_json(const QJsonObject& o);
};

struct MappingOptions {
    /// Number text: "auto" (detect 1,234.5 vs 1.234,5 per value), "dot" (1,234.5), "comma" (1.234,5).
    QString decimal = QStringLiteral("auto");
    /// Cell text that means "no value" (compared case-insensitively after trimming).
    QStringList null_tokens = {QString(), QStringLiteral("null"), QStringLiteral("none"), QStringLiteral("nan"),
                               QStringLiteral("n/a"), QStringLiteral("na"), QStringLiteral("-"), QStringLiteral("--"),
                               QStringLiteral(".")};
    QStringList key_fields;  ///< target fields that identify a row (duplicate detection)
    QString duplicates = QStringLiteral("keep"); ///< "keep" | "drop" | "flag"
    bool drop_invalid_rows = false;              ///< leave rows with any cell error out of the output
    bool trim_text = true;

    QJsonObject to_json() const;
    static MappingOptions from_json(const QJsonObject& o);
};

struct MappingSpec {
    int version = 2;
    QString target_schema;   ///< "OHLCV", "QUOTE", …, or "CUSTOM"
    SourceRef source;
    QVector<FieldMapping> fields;
    MappingOptions options;

    QJsonObject to_json() const;
    static MappingSpec from_json(const QJsonObject& o);
    int index_of(const QString& target) const;
};

/// A mapping saved before v062: an HTTP request + [{target, expression, transform, default_val}].
struct LegacyMapping {
    QString schema_name;
    QString base_url, endpoint, method, auth_type, headers, body, parser;
    QJsonArray field_mappings;
};
/// Converts a legacy mapping. Expressions that all project one array ("$.data[*].close") become a
/// rows path + per-row columns; anything else keeps the old "one record per run" behaviour
/// (rows path "$": the whole response is one row and each expression a path inside it).
MappingSpec spec_from_legacy(const LegacyMapping& legacy);

// ── Target schemas ────────────────────────────────────────────────────────

struct SchemaField {
    QString name;
    TargetType type;
    bool required;
    QString description;
};
struct TargetSchema {
    QString name;
    QString description;
    QVector<SchemaField> fields;
};
/// OHLCV, QUOTE, TICK, ORDER, POSITION, PORTFOLIO, INSTRUMENT, TIMESERIES, MACRO.
const QVector<TargetSchema>& target_schemas();
const TargetSchema* find_target_schema(const QString& name);

/// Best source column for a target field (exact, case-insensitive, then synonyms: timestamp ↔
/// date/time, close ↔ price/last/adj_close, volume ↔ vol, symbol ↔ ticker/code, …). "" when none.
QString suggest_source_column(const QString& target, const QStringList& source_columns);

} // namespace fincept::services::datamapping
