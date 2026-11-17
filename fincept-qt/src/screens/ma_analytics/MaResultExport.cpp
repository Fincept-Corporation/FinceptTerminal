#include "screens/ma_analytics/MaResultExport.h"

#include <QDateTime>

#include <cmath>

namespace fincept::screens::ma_export {

namespace {

namespace res = fincept::ui::result;

QString ma_exp_title(const QString& parent, const QString& own) {
    if (parent.isEmpty())
        return own;
    if (own.isEmpty())
        return parent;
    return parent + QStringLiteral(" · ") + own;
}

void ma_exp_walk(const res::Block& b, const QString& parent, const res::Options& o, Export* out) {
    const QString title = ma_exp_title(parent, b.title);
    switch (b.kind) {
        case res::Kind::Kpis:
            for (const res::Field& f : b.fields)
                out->kpis.append({parent.isEmpty() ? f.label : parent + QStringLiteral(" · ") + f.label,
                                  res::format_value(f.value, f.unit, o)});
            return;
        case res::Kind::Fields: {
            Table t;
            t.title = title.isEmpty() ? QStringLiteral("Details") : title;
            t.headers = {QStringLiteral("Item"), QStringLiteral("Value")};
            for (const res::Field& f : b.fields)
                t.rows.append({f.label, res::format_value(f.value, f.unit, o)});
            if (!t.rows.isEmpty())
                out->tables.append(t);
            return;
        }
        case res::Kind::Table: {
            Table t;
            t.title = title;
            t.headers = b.column_labels;
            for (int r = 0; r < b.rows.size(); ++r) {
                QStringList line;
                const auto& row = b.rows.at(r);
                for (int c = 0; c < row.size(); ++c) {
                    res::Unit u = c < b.column_units.size() ? b.column_units.at(c) : res::Unit::Plain;
                    if (c > 0 && r < b.row_units.size())
                        u = b.row_units.at(r);
                    line << res::format_value(row.at(c), u, o);
                }
                t.rows.append(line);
            }
            if (!t.rows.isEmpty())
                out->tables.append(t);
            return;
        }
        case res::Kind::Heatmap: {
            Table t;
            t.title = title;
            t.headers << (b.row_title.isEmpty() && b.col_title.isEmpty() ? QString()
                                                                         : b.row_title + QStringLiteral(" \\ ") + b.col_title);
            t.headers += b.col_labels;
            for (int r = 0; r < b.matrix.size(); ++r) {
                QStringList line{b.row_labels.value(r)};
                for (double v : b.matrix.at(r))
                    line << (std::isfinite(v) ? res::format_number(v, b.value_unit, o) : QStringLiteral("-"));
                t.rows.append(line);
            }
            if (!t.rows.isEmpty())
                out->tables.append(t);
            return;
        }
        case res::Kind::Line:
        case res::Kind::Bars:
        case res::Kind::Donut: {
            Table t;
            t.title = title;
            t.headers << (b.x_title.isEmpty() ? QStringLiteral("Item") : b.x_title);
            for (const res::Series& s : b.series)
                t.headers << s.name;
            const bool cats = b.kind != res::Kind::Line;
            const int n = cats ? static_cast<int>(b.categories.size()) : static_cast<int>(b.xs.size());
            for (int i = 0; i < n; ++i) {
                QString x;
                if (cats)
                    x = b.categories.at(i);
                else if (b.x_dates)
                    x = QDateTime::fromMSecsSinceEpoch(qint64(b.xs.at(i))).date().toString(Qt::ISODate);
                else
                    x = res::format_number(b.xs.at(i), b.x_unit, o);
                QStringList line{x};
                for (const res::Series& s : b.series) {
                    const double v = i < s.values.size() ? s.values.at(i) : std::nan("");
                    line << (std::isfinite(v) ? res::format_number(v, s.unit, o) : QString());
                }
                t.rows.append(line);
            }
            if (!t.rows.isEmpty())
                out->tables.append(t);
            return;
        }
        case res::Kind::Gauge:
            if (std::isfinite(b.value))
                out->kpis.append({title, res::format_number(b.value, res::Unit::Plain, o)});
            return;
        case res::Kind::Section:
            for (const res::Block& c : b.children)
                ma_exp_walk(c, title, o, out);
            return;
        case res::Kind::Caption:
        case res::Kind::List:
        case res::Kind::Distribution:
        case res::Kind::Fan:
            return;
    }
}

} // namespace

Export build(const QJsonValue& payload, const fincept::ui::result::Options& options) {
    Export e;
    const res::Block root = res::build(payload, options);
    ma_exp_walk(root, QString(), options, &e);
    return e;
}

QString to_tsv(const QString& title, const Export& e) {
    QStringList lines;
    if (!title.isEmpty())
        lines << title;
    for (const auto& kv : e.kpis)
        lines << kv.first + QLatin1Char('\t') + kv.second;
    for (const Table& t : e.tables) {
        lines << QString();
        if (!t.title.isEmpty())
            lines << t.title;
        lines << t.headers.join(QLatin1Char('\t'));
        for (const QStringList& r : t.rows)
            lines << r.join(QLatin1Char('\t'));
    }
    return lines.join(QLatin1Char('\n'));
}

QPair<QStringList, QVector<QStringList>> to_sheet(const Export& e) {
    QStringList headers{QStringLiteral("Section"), QStringLiteral("Item"), QStringLiteral("Value")};
    QVector<QStringList> rows;
    for (const auto& kv : e.kpis)
        rows.append({QStringLiteral("Key figures"), kv.first, kv.second});
    for (const Table& t : e.tables) {
        rows.append(QStringList());
        rows.append({t.title});
        rows.append(t.headers);
        for (const QStringList& r : t.rows)
            rows.append(r);
    }
    return {headers, rows};
}

} // namespace fincept::screens::ma_export
