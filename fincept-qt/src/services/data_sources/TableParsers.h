#pragma once
// TableParsers — bytes → DataTable for every text format a data source can answer with:
// CSV / TSV / any-delimiter text, JSON (array, nested rows under a path, object), NDJSON,
// XML (repeated record elements), and Server-Sent Events. Pure functions, QtCore only,
// safe on any thread (connections run them on worker threads).

#include "services/data_sources/SourceError.h"
#include "services/data_sources/TabularData.h"

#include <QByteArray>
#include <QJsonValue>
#include <QString>

namespace fincept::services::datasources {

enum class TextFormat { Auto, Csv, Tsv, Json, NdJson, Xml, EventStream };

QString text_format_name(TextFormat f);              ///< "auto" | "csv" | "tsv" | "json" | "ndjson" | "xml" | "sse"
TextFormat text_format_from_name(const QString& s);  ///< unknown → Auto

struct ParseOptions {
    TextFormat format = TextFormat::Auto;
    QChar delimiter;            ///< CSV: null = detect (',', ';', '\t', '|')
    bool has_header = true;     ///< CSV: first row holds the column names
    QString rows_path;          ///< JSON / XML: where the records are ("data.rows", "$.result[0].items", "//record");
                                ///< "$document" = the whole JSON document as one row in column "$"
    int max_rows = -1;          ///< stop after this many rows (sets `truncated`); -1 = all
    QString content_type;       ///< HTTP Content-Type, used by Auto
    QString file_name;          ///< file name / URL path, used by Auto (extension)
};

struct ParseOutcome {
    bool ok = false;
    DataTable table;
    bool truncated = false;
    TextFormat detected = TextFormat::Auto;
    QString note;               ///< e.g. "rows taken from data.rows", "delimiter ';'"
    SourceError error;          ///< !ok: bad_args / no_data with a plain message
    QJsonValue json;            ///< JSON formats: the parsed document (for a key/value tree view)
};

/// Decide the format from the options and the first bytes.
TextFormat detect_text_format(const QByteArray& data, const ParseOptions& opts);
/// Parse `data` into one table according to `opts`.
ParseOutcome parse_table(const QByteArray& data, const ParseOptions& opts);

/// The value at `path` in `root`: "a.b", "$.a.b", "a[0].b", "['odd key'].c", "[1]". Empty path / "$" =
/// root. Undefined when the path does not resolve.
QJsonValue json_at_path(const QJsonValue& root, const QString& path);
/// The best list of records inside `root` when no path is given: the root array itself, else the
/// largest array of objects found up to 3 levels deep. `found_path` receives where it was.
QJsonValue find_records(const QJsonValue& root, QString* found_path);

/// CSV split honouring quotes ("a,""b""", embedded newlines). Rows of fields.
QVector<QStringList> split_delimited(const QString& text, QChar delimiter, int max_rows = -1, bool* truncated = nullptr);
/// Most likely delimiter of `sample` among , ; \t |.
QChar detect_delimiter(const QString& sample);

/// Server-Sent Events: each event's `data:` payload (joined lines). JSON payloads become objects.
QJsonArray parse_event_stream(const QByteArray& data, int max_events = -1);

/// Cells of a delimited file are text; turn plain numbers / booleans into JSON numbers / bools
/// (leading zeros and very long digit runs stay text: ISINs, CUSIPs, zip codes).
QJsonValue typed_text_cell(const QString& text);

} // namespace fincept::services::datasources
