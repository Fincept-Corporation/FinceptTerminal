#pragma once
// ReportTableCodec.h — the one text format of a Report Builder table
// (component config["csv"]), shared by the canvas renderer and the properties-panel
// cell editor so both read and write exactly the same thing.
//
// Rows are separated by '|' or a line break, cells by ',' or a tab. A cell wrapped
// in double quotes may contain any of those characters ("1,234.5" stays one cell;
// "" inside quotes is a literal quote). Unquoted numbers with thousands separators
// still split — the editor quotes such cells when it writes them.

#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens::report_table {

inline QVector<QStringList> parse(const QString& text) {
    QVector<QStringList> rows;
    QStringList row;
    QString cell;
    bool in_quotes = false;
    bool row_has_content = false;

    auto end_cell = [&]() {
        row << cell.trimmed();
        cell.clear();
    };
    auto end_row = [&]() {
        end_cell();
        bool any = row_has_content;
        for (const QString& c : row)
            any = any || !c.isEmpty();
        if (any)
            rows.append(row);
        row.clear();
        row_has_content = false;
    };

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];
        if (in_quotes) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < text.size() && text[i + 1] == QLatin1Char('"')) {
                    cell += QLatin1Char('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                cell += ch;
            }
            continue;
        }
        if (ch == QLatin1Char('"') && cell.trimmed().isEmpty()) {
            in_quotes = true;
            cell.clear();
            row_has_content = true;
        } else if (ch == QLatin1Char(',') || ch == QLatin1Char('\t')) {
            end_cell();
            row_has_content = true;
        } else if (ch == QLatin1Char('|') || ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
            if (ch == QLatin1Char('\r') && i + 1 < text.size() && text[i + 1] == QLatin1Char('\n'))
                ++i;
            end_row();
        } else {
            cell += ch;
        }
    }
    end_row();
    return rows;
}

inline QString encode_cell(const QString& cell) {
    if (cell.contains(QLatin1Char(',')) || cell.contains(QLatin1Char('|')) || cell.contains(QLatin1Char('"')) ||
        cell.contains(QLatin1Char('\n')) || cell.contains(QLatin1Char('\t'))) {
        QString q = cell;
        q.replace(QLatin1String("\""), QLatin1String("\"\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    }
    return cell;
}

inline QString encode(const QVector<QStringList>& rows) {
    QStringList out;
    out.reserve(rows.size());
    for (const QStringList& r : rows) {
        QStringList cells;
        cells.reserve(r.size());
        for (const QString& c : r)
            cells << encode_cell(c);
        out << cells.join(QLatin1Char(','));
    }
    return out.join(QLatin1Char('|'));
}

} // namespace fincept::screens::report_table
