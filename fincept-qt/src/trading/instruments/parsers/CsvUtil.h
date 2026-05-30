#pragma once
// CsvUtil — minimal CSV helpers for instrument-master parsing. Header-only.
//
// Several broker masters double-quote fields and embed commas inside names
// (Paytm fully-quoted, 5paisa FullName, etc.), so a plain split(',') corrupts
// rows. split_csv_line handles RFC-4180-style quoting. header_index builds a
// case-insensitive column-name → index map so parsers read by name rather than
// fragile positional offsets.

#include <QHash>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace fincept::trading::csv {

/// Split one CSV line, honouring double-quoted fields ("" = literal quote).
inline QStringList split_csv_line(QStringView line) {
    QStringList out;
    QString field;
    bool in_quotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line[i];
        if (in_quotes) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { // escaped quote
                    field += '"';
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                field += ch;
            }
        } else if (ch == '"') {
            in_quotes = true;
        } else if (ch == ',') {
            out.append(field);
            field.clear();
        } else {
            field += ch;
        }
    }
    out.append(field);
    return out;
}

/// Build a case-insensitive {column name (trimmed, upper) → index} map from a
/// header line.
inline QHash<QString, int> header_index(QStringView header_line) {
    QHash<QString, int> idx;
    const QStringList cols = split_csv_line(header_line);
    for (int i = 0; i < cols.size(); ++i)
        idx.insert(cols[i].trimmed().toUpper(), i);
    return idx;
}

/// Safe field read by header name. Returns "" if the column or row cell is absent.
inline QString field(const QStringList& row, const QHash<QString, int>& idx, const QString& name) {
    const auto it = idx.constFind(name.toUpper());
    if (it == idx.constEnd() || it.value() >= row.size())
        return {};
    return row[it.value()].trimmed();
}

} // namespace fincept::trading::csv
