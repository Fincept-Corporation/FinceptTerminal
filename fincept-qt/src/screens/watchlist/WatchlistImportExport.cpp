#include "screens/watchlist/WatchlistImportExport.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace fincept::screens {

namespace {

QString wlio_tr(const char* s) {
    return QCoreApplication::translate("WatchlistImport", s);
}

QStringList wlio_split_line(const QString& line, QChar sep) {
    QStringList fields;
    QString current;
    bool in_quotes = false;
    for (int i = 0; i < line.length(); ++i) {
        const QChar c = line[i];
        if (c == QLatin1Char('"')) {
            if (in_quotes && i + 1 < line.length() && line[i + 1] == QLatin1Char('"')) {
                current += QLatin1Char('"');
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == sep && !in_quotes) {
            fields.append(current);
            current.clear();
        } else {
            current += c;
        }
    }
    fields.append(current);
    return fields;
}

QChar wlio_detect_separator(const QString& header) {
    const auto count = [&header](QChar c) { return header.count(c); };
    if (count(QLatin1Char('\t')) > 0 && count(QLatin1Char('\t')) >= count(QLatin1Char(',')))
        return QLatin1Char('\t');
    if (count(QLatin1Char(';')) > count(QLatin1Char(',')))
        return QLatin1Char(';');
    return QLatin1Char(',');
}

QString wlio_clean_symbol(QString s) {
    s = s.trimmed();
    if (s.startsWith(QLatin1Char('\'')))
        s.remove(0, 1); // a spreadsheet's formula guard
    return s.trimmed().toUpper();
}

void wlio_add(WatchlistImportParse& out, QSet<QString>& seen, WatchlistImportEntry e) {
    e.symbol = wlio_clean_symbol(e.symbol);
    if (e.symbol.isEmpty())
        return;
    if (!watchlist_is_ticker_shaped(e.symbol)) {
        if (!out.invalid.contains(e.symbol))
            out.invalid << e.symbol;
        return;
    }
    if (seen.contains(e.symbol)) {
        ++out.duplicates;
        return;
    }
    seen.insert(e.symbol);
    e.name = e.name.trimmed();
    e.notes = e.notes.trimmed();
    e.group = e.group.trimmed();
    e.exchange = e.exchange.trimmed();
    out.entries.append(e);
}

WatchlistImportParse wlio_parse_json(const QByteArray& bytes) {
    WatchlistImportParse out;
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError) {
        out.error = wlio_tr("Not valid JSON: %1").arg(err.errorString());
        return out;
    }
    QJsonArray arr;
    if (doc.isArray()) {
        arr = doc.array();
    } else if (doc.isObject()) {
        const QJsonObject o = doc.object();
        out.list_name = o.value(QStringLiteral("name")).toString();
        arr = o.value(QStringLiteral("symbols")).toArray();
        if (arr.isEmpty())
            arr = o.value(QStringLiteral("stocks")).toArray();
    }
    QSet<QString> seen;
    for (const QJsonValue& v : arr) {
        WatchlistImportEntry e;
        if (v.isString()) {
            e.symbol = v.toString();
        } else if (v.isObject()) {
            const QJsonObject o = v.toObject();
            e.symbol = o.value(QStringLiteral("symbol")).toString();
            if (e.symbol.isEmpty())
                e.symbol = o.value(QStringLiteral("ticker")).toString();
            e.name = o.value(QStringLiteral("name")).toString();
            e.notes = o.value(QStringLiteral("notes")).toString();
            e.group = o.value(QStringLiteral("group")).toString();
            e.exchange = o.value(QStringLiteral("exchange")).toString();
        } else {
            continue;
        }
        wlio_add(out, seen, e);
    }
    if (out.entries.isEmpty() && out.invalid.isEmpty())
        out.error = wlio_tr("The file lists no symbols.");
    return out;
}

WatchlistImportParse wlio_parse_text(const QString& text) {
    WatchlistImportParse out;
    QStringList lines = text.split(QRegularExpression(QStringLiteral("\\r\\n|\\n|\\r")));
    while (!lines.isEmpty() && lines.first().trimmed().isEmpty())
        lines.removeFirst();
    if (lines.isEmpty()) {
        out.error = wlio_tr("The file is empty.");
        return out;
    }
    const QChar sep = wlio_detect_separator(lines.first());
    const QStringList header = wlio_split_line(lines.first(), sep);
    int sym_col = -1, name_col = -1, notes_col = -1, group_col = -1, exch_col = -1;
    for (int i = 0; i < header.size(); ++i) {
        const QString h = header[i].trimmed().toUpper();
        if (sym_col < 0 && (h == QLatin1String("SYMBOL") || h == QLatin1String("TICKER") || h == QLatin1String("CODE")))
            sym_col = i;
        else if (name_col < 0 && (h == QLatin1String("NAME") || h == QLatin1String("DESCRIPTION")))
            name_col = i;
        else if (notes_col < 0 && (h == QLatin1String("NOTES") || h == QLatin1String("NOTE")))
            notes_col = i;
        else if (group_col < 0 && (h == QLatin1String("GROUP") || h == QLatin1String("CATEGORY")))
            group_col = i;
        else if (exch_col < 0 && h == QLatin1String("EXCHANGE"))
            exch_col = i;
    }
    int first_data = 1;
    if (sym_col < 0) {
        if (header.size() == 1) {
            // A plain list, one symbol per line: the first line is data.
            sym_col = 0;
            first_data = 0;
        } else {
            out.error = wlio_tr("No SYMBOL (or TICKER) column in the header row.");
            return out;
        }
    }
    QSet<QString> seen;
    for (int l = first_data; l < lines.size(); ++l) {
        if (lines[l].trimmed().isEmpty())
            continue;
        const QStringList f = wlio_split_line(lines[l], sep);
        if (f.size() <= sym_col)
            continue;
        WatchlistImportEntry e;
        e.symbol = f[sym_col];
        if (name_col >= 0 && name_col < f.size())
            e.name = f[name_col];
        if (notes_col >= 0 && notes_col < f.size())
            e.notes = f[notes_col];
        if (group_col >= 0 && group_col < f.size())
            e.group = f[group_col];
        if (exch_col >= 0 && exch_col < f.size())
            e.exchange = f[exch_col];
        wlio_add(out, seen, e);
    }
    if (out.entries.isEmpty() && out.invalid.isEmpty())
        out.error = wlio_tr("The file lists no symbols.");
    return out;
}

} // namespace

bool watchlist_is_ticker_shaped(const QString& symbol) {
    static const QRegularExpression kValid(QStringLiteral("^[A-Z0-9^][A-Z0-9.\\-=^&_/:]{0,24}$"));
    return kValid.match(symbol).hasMatch();
}

QStringList watchlist_split_symbols(const QString& text) {
    static const QRegularExpression kSeparators(QStringLiteral("[,;\\s]+"));
    return text.trimmed().toUpper().split(kSeparators, Qt::SkipEmptyParts);
}

WatchlistImportParse watchlist_parse_import(const QByteArray& bytes, const QString& file_name) {
    QByteArray data = bytes;
    if (data.startsWith("\xEF\xBB\xBF"))
        data.remove(0, 3);
    const QByteArray trimmed = data.trimmed();
    const bool json = file_name.endsWith(QLatin1String(".json"), Qt::CaseInsensitive) || trimmed.startsWith('{') ||
                      trimmed.startsWith('[');
    if (json)
        return wlio_parse_json(data);
    return wlio_parse_text(QString::fromUtf8(data));
}

QByteArray watchlist_export_json(const fincept::Watchlist& list, const QVector<fincept::WatchlistStock>& stocks) {
    QJsonArray arr;
    for (const auto& s : stocks) {
        QJsonObject o;
        o.insert(QStringLiteral("symbol"), s.symbol);
        if (!s.name.isEmpty())
            o.insert(QStringLiteral("name"), s.name);
        if (!s.exchange.isEmpty())
            o.insert(QStringLiteral("exchange"), s.exchange);
        if (!s.notes.isEmpty())
            o.insert(QStringLiteral("notes"), s.notes);
        if (!s.group_name.isEmpty())
            o.insert(QStringLiteral("group"), s.group_name);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("fincept-watchlist"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("name"), list.name);
    if (!list.description.isEmpty())
        root.insert(QStringLiteral("description"), list.description);
    root.insert(QStringLiteral("exported_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("symbols"), arr);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace fincept::screens
