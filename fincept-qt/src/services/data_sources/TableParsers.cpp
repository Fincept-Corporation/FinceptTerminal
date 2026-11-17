// TableParsers.cpp — see TableParsers.h.

#include "services/data_sources/TableParsers.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>

namespace fincept::services::datasources {

namespace {

QString tp_tr(const char* text) {
    return QCoreApplication::translate("TableParsers", text);
}

QString tp_decode(const QByteArray& data) {
    QByteArray bytes = data;
    if (bytes.startsWith("\xEF\xBB\xBF"))
        bytes.remove(0, 3);
    // UTF-16 BOMs (Excel "Unicode text" exports)
    if (bytes.startsWith("\xFF\xFE") || bytes.startsWith("\xFE\xFF")) {
        QStringDecoder dec(bytes.startsWith("\xFF\xFE") ? QStringDecoder::Utf16LE : QStringDecoder::Utf16BE);
        return dec.decode(bytes.mid(2));
    }
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString s = utf8.decode(bytes);
    if (!utf8.hasError())
        return s;
    QStringDecoder latin(QStringDecoder::Latin1); // legacy exports: never fail on one bad byte
    return latin.decode(bytes);
}

QStringList tp_unique_headers(const QStringList& raw) {
    QStringList out;
    QHash<QString, int> seen;
    for (int i = 0; i < raw.size(); ++i) {
        QString h = raw[i].trimmed();
        if (h.isEmpty())
            h = QStringLiteral("column_%1").arg(i + 1);
        const int n = seen.value(h.toLower(), 0);
        seen.insert(h.toLower(), n + 1);
        if (n > 0)
            h += QStringLiteral("_%1").arg(n + 1);
        out << h;
    }
    return out;
}

/// Records of a JSON array that has a nested record array (World Bank: [{page meta}, [records]]).
QJsonValue tp_inner_records(const QJsonArray& a) {
    for (const QJsonValue& v : a) {
        if (!v.isArray())
            continue;
        const QJsonArray inner = v.toArray();
        int objs = 0;
        for (const QJsonValue& x : inner)
            if (x.isObject())
                ++objs;
        if (!inner.isEmpty() && objs * 2 > inner.size())
            return inner;
    }
    return QJsonValue(QJsonValue::Undefined);
}

struct TpCandidate {
    QString path;
    QJsonValue value;
    double score = -1;
};

void tp_consider(const QString& path, const QJsonValue& v, int depth, TpCandidate& best) {
    if (!v.isArray())
        return;
    const QJsonArray a = v.toArray();
    if (a.isEmpty())
        return;
    const int n = std::min<int>(int(a.size()), 50);
    int objs = 0;
    for (int i = 0; i < n; ++i)
        if (a.at(i).isObject())
            ++objs;
    double score;
    if (objs * 2 > n)
        score = 3e6 + double(a.size());
    else if (a.size() >= 2)
        score = 1e6 + double(a.size());
    else
        return;
    score -= depth;
    if (score > best.score) {
        best.path = path;
        best.value = v;
        best.score = score;
    }
}

void tp_walk(const QJsonObject& o, const QString& prefix, int depth, TpCandidate& best) {
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        const QString path = prefix.isEmpty() ? it.key() : prefix + QLatin1Char('.') + it.key();
        tp_consider(path, it.value(), depth, best);
        if (it->isObject() && depth < 3)
            tp_walk(it->toObject(), path, depth + 1, best);
    }
}

ParseOutcome tp_parse_delimited(const QByteArray& data, const ParseOptions& opts, TextFormat fmt) {
    ParseOutcome out;
    out.detected = fmt;
    const QString text = tp_decode(data);
    QChar delim = opts.delimiter;
    if (fmt == TextFormat::Tsv)
        delim = QLatin1Char('\t');
    if (delim.isNull())
        delim = detect_delimiter(text.left(8192));
    bool truncated = false;
    const int limit = opts.max_rows < 0 ? -1 : opts.max_rows + (opts.has_header ? 1 : 0);
    QVector<QStringList> rows = split_delimited(text, delim, limit, &truncated);
    // Drop trailing blank lines.
    while (!rows.isEmpty() && rows.last().size() == 1 && rows.last().first().trimmed().isEmpty())
        rows.removeLast();
    if (rows.isEmpty() || (opts.has_header && rows.size() < 1)) {
        out.error = SourceError::make(ErrorCode::NoData, tp_tr("The file is empty"));
        return out;
    }
    int width = 0;
    for (const QStringList& r : rows)
        width = std::max<int>(width, int(r.size()));
    QStringList header;
    int first = 0;
    if (opts.has_header) {
        header = rows.first();
        while (header.size() < width)
            header << QString();
        header = tp_unique_headers(header);
        first = 1;
    } else {
        for (int c = 0; c < width; ++c)
            header << QStringLiteral("column_%1").arg(c + 1);
    }
    out.table.columns = header;
    out.table.types = QVector<ColumnType>(header.size(), ColumnType::Unknown);
    out.table.rows.reserve(rows.size() - first);
    for (int r = first; r < rows.size(); ++r) {
        const QStringList& f = rows[r];
        if (f.size() == 1 && f.first().trimmed().isEmpty())
            continue; // blank line inside the file
        QJsonArray row;
        for (int c = 0; c < header.size(); ++c)
            row.append(c < f.size() ? typed_text_cell(f[c]) : QJsonValue(QJsonValue::Null));
        out.table.rows.append(row);
    }
    infer_column_types(out.table);
    out.truncated = truncated;
    out.note = delim == QLatin1Char('\t') ? tp_tr("tab separated") : tp_tr("delimiter '%1'").arg(delim);
    if (out.table.rows.isEmpty()) {
        out.error = SourceError::make(ErrorCode::NoData, tp_tr("The file has a header row but no data rows"));
        return out;
    }
    out.ok = true;
    return out;
}

ParseOutcome tp_table_from_json(const QJsonValue& root, const ParseOptions& opts, TextFormat fmt) {
    ParseOutcome out;
    out.detected = fmt;
    out.json = root;
    QJsonValue records;
    QString where;
    if (opts.rows_path.trimmed() == QLatin1String("$document")) {
        // The whole document as ONE row (column "$"): mappings that pick values by path inside it.
        out.table.columns = {QStringLiteral("$")};
        out.table.types = {ColumnType::Object};
        out.table.rows.append(QJsonArray{root});
        out.ok = true;
        out.note = tp_tr("the whole response is one row");
        return out;
    }
    if (!opts.rows_path.trimmed().isEmpty()) {
        records = json_at_path(root, opts.rows_path.trimmed());
        where = opts.rows_path.trimmed();
        if (records.isUndefined()) {
            out.error = SourceError::make(ErrorCode::BadArgs,
                                          tp_tr("Nothing at rows path \"%1\" in the response").arg(where),
                                          QStringLiteral("rowsPath"));
            return out;
        }
    } else {
        records = find_records(root, &where);
    }
    if (records.isArray()) {
        QJsonArray a = records.toArray();
        if (opts.max_rows >= 0 && a.size() > opts.max_rows) {
            QJsonArray cut;
            for (int i = 0; i < opts.max_rows; ++i)
                cut.append(a.at(i));
            a = cut;
            out.truncated = true;
        }
        out.table = table_from_json_array(a);
    } else if (records.isObject()) {
        out.table = table_from_key_values(records.toObject());
    } else if (!records.isUndefined() && !records.isNull()) {
        out.table.columns = {QStringLiteral("value")};
        out.table.types = {cell_type(records)};
        out.table.rows.append(QJsonArray{records});
    }
    if (!where.isEmpty() && where != QLatin1String("$"))
        out.note = tp_tr("rows taken from %1").arg(where);
    if (out.table.is_empty()) {
        out.error = SourceError::make(ErrorCode::NoData, tp_tr("The response holds no rows"));
        return out;
    }
    out.ok = true;
    return out;
}

/// The local name of the last segment of an XPath-ish rows path ("//record", "a/b/record", "record").
QString tp_xml_record_name(const QString& path) {
    QString p = path.trimmed();
    while (p.endsWith(QLatin1Char('/')))
        p.chop(1);
    const int slash = int(p.lastIndexOf(QLatin1Char('/')));
    p = p.mid(slash + 1);
    const int colon = int(p.indexOf(QLatin1Char(':')));
    return colon >= 0 ? p.mid(colon + 1) : p;
}

ParseOutcome tp_parse_xml(const QByteArray& data, const ParseOptions& opts) {
    ParseOutcome out;
    out.detected = TextFormat::Xml;
    QString record_name = tp_xml_record_name(opts.rows_path);
    int record_depth = -1;

    if (record_name.isEmpty()) {
        // Pass 1: the (depth, name) of elements that have children and repeat most often.
        QXmlStreamReader xr(data);
        QVector<QString> stack;
        QVector<bool> has_child;
        QHash<QString, int> counts;
        while (!xr.atEnd()) {
            xr.readNext();
            if (xr.isStartElement()) {
                if (!has_child.isEmpty())
                    has_child.last() = true;
                stack.push_back(xr.name().toString());
                has_child.push_back(!xr.attributes().isEmpty());
            } else if (xr.isEndElement()) {
                if (!stack.isEmpty()) {
                    if (has_child.last())
                        counts[QString::number(stack.size()) + QLatin1Char('|') + stack.last()] += 1;
                    stack.pop_back();
                    has_child.pop_back();
                }
            }
        }
        if (xr.hasError()) {
            out.error = SourceError::make(ErrorCode::BadArgs,
                                          tp_tr("The XML is not well formed: %1 (line %2)")
                                              .arg(xr.errorString())
                                              .arg(xr.lineNumber()));
            return out;
        }
        int best = 0;
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
            const int depth = it.key().section(QLatin1Char('|'), 0, 0).toInt();
            if (it.value() > best || (it.value() == best && depth > record_depth)) {
                best = it.value();
                record_depth = depth;
                record_name = it.key().section(QLatin1Char('|'), 1);
            }
        }
        if (best < 1 || record_name.isEmpty()) {
            out.error = SourceError::make(ErrorCode::NoData, tp_tr("No repeated record elements in the XML"));
            return out;
        }
    }

    // Pass 2: each record element → one row (attributes as @name, leaf children by name,
    // grandchildren as parent.child).
    QXmlStreamReader xr(data);
    QStringList columns;
    QSet<QString> seen_cols;
    QJsonArray records;
    int depth = 0;
    int in_record_at = -1;
    QJsonObject current;
    QVector<QString> path; // element names below the record
    QString text;
    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isStartElement()) {
            ++depth;
            const QString name = xr.name().toString();
            if (in_record_at < 0 && name == record_name && (record_depth < 0 || depth == record_depth)) {
                in_record_at = depth;
                current = QJsonObject();
                path.clear();
                text.clear();
                for (const auto& a : xr.attributes()) {
                    const QString col = QLatin1Char('@') + a.name().toString();
                    current.insert(col, typed_text_cell(a.value().toString()));
                    if (!seen_cols.contains(col)) {
                        seen_cols.insert(col);
                        columns << col;
                    }
                }
            } else if (in_record_at >= 0) {
                path.push_back(name);
                text.clear();
                for (const auto& a : xr.attributes()) {
                    const QString col = QStringList(path.begin(), path.end()).join(QLatin1Char('.')) + QLatin1Char('@') +
                                        a.name().toString();
                    current.insert(col, typed_text_cell(a.value().toString()));
                    if (!seen_cols.contains(col)) {
                        seen_cols.insert(col);
                        columns << col;
                    }
                }
            }
        } else if (xr.isCharacters() && in_record_at >= 0 && !xr.isWhitespace()) {
            text += xr.text().toString();
        } else if (xr.isEndElement()) {
            if (in_record_at >= 0 && depth == in_record_at) {
                if (!text.trimmed().isEmpty() && !current.contains(record_name)) {
                    // A record that is plain text (<price>1.5</price> repeated).
                    current.insert(record_name, typed_text_cell(text.trimmed()));
                    if (!seen_cols.contains(record_name)) {
                        seen_cols.insert(record_name);
                        columns << record_name;
                    }
                }
                text.clear();
                records.append(current);
                in_record_at = -1;
                if (opts.max_rows >= 0 && records.size() >= opts.max_rows) {
                    out.truncated = true;
                    break;
                }
            } else if (in_record_at >= 0 && !path.isEmpty()) {
                const QString col = QStringList(path.begin(), path.end()).join(QLatin1Char('.'));
                if (!text.isEmpty() || !current.contains(col)) {
                    current.insert(col, text.isEmpty() ? QJsonValue(QJsonValue::Null) : typed_text_cell(text.trimmed()));
                    if (!seen_cols.contains(col)) {
                        seen_cols.insert(col);
                        columns << col;
                    }
                }
                text.clear();
                path.pop_back();
            }
            --depth;
        }
    }
    if (xr.hasError() && !out.truncated) {
        out.error = SourceError::make(
            ErrorCode::BadArgs,
            tp_tr("The XML is not well formed: %1 (line %2)").arg(xr.errorString()).arg(xr.lineNumber()));
        return out;
    }
    // Container elements (that got children) were inserted as null before their children: drop
    // columns that are null in every row.
    QStringList kept;
    for (const QString& c : std::as_const(columns)) {
        bool any = false;
        for (const QJsonValue& r : std::as_const(records)) {
            const QJsonValue v = r.toObject().value(c);
            if (!v.isNull() && !v.isUndefined()) {
                any = true;
                break;
            }
        }
        if (any)
            kept << c;
    }
    out.table = table_from_records(records, kept, QJsonObject());
    out.note = tp_tr("records <%1>").arg(record_name);
    if (out.table.is_empty()) {
        out.error = SourceError::make(ErrorCode::NoData, tp_tr("No <%1> records in the XML").arg(record_name));
        return out;
    }
    out.ok = true;
    return out;
}

} // namespace

QString text_format_name(TextFormat f) {
    switch (f) {
        case TextFormat::Auto:
            return QStringLiteral("auto");
        case TextFormat::Csv:
            return QStringLiteral("csv");
        case TextFormat::Tsv:
            return QStringLiteral("tsv");
        case TextFormat::Json:
            return QStringLiteral("json");
        case TextFormat::NdJson:
            return QStringLiteral("ndjson");
        case TextFormat::Xml:
            return QStringLiteral("xml");
        case TextFormat::EventStream:
            return QStringLiteral("sse");
    }
    return QStringLiteral("auto");
}

TextFormat text_format_from_name(const QString& s) {
    const QString n = s.trimmed().toLower();
    if (n == QLatin1String("csv") || n == QLatin1String("text") || n == QLatin1String("delimited"))
        return TextFormat::Csv;
    if (n == QLatin1String("tsv") || n == QLatin1String("tab"))
        return TextFormat::Tsv;
    if (n == QLatin1String("json"))
        return TextFormat::Json;
    if (n == QLatin1String("ndjson") || n == QLatin1String("jsonl") || n == QLatin1String("json-lines"))
        return TextFormat::NdJson;
    if (n == QLatin1String("xml"))
        return TextFormat::Xml;
    if (n == QLatin1String("sse") || n == QLatin1String("event-stream"))
        return TextFormat::EventStream;
    return TextFormat::Auto;
}

TextFormat detect_text_format(const QByteArray& data, const ParseOptions& opts) {
    if (opts.format != TextFormat::Auto)
        return opts.format;
    const QString ct = opts.content_type.toLower();
    if (ct.contains(QLatin1String("event-stream")))
        return TextFormat::EventStream;
    if (ct.contains(QLatin1String("ndjson")) || ct.contains(QLatin1String("jsonl")) ||
        ct.contains(QLatin1String("json-seq")))
        return TextFormat::NdJson;
    if (ct.contains(QLatin1String("json")))
        return TextFormat::Json;
    if (ct.contains(QLatin1String("xml")))
        return TextFormat::Xml;
    if (ct.contains(QLatin1String("tab-separated")))
        return TextFormat::Tsv;
    if (ct.contains(QLatin1String("csv")))
        return TextFormat::Csv;
    const QString ext = opts.file_name.section(QLatin1Char('?'), 0, 0).section(QLatin1Char('.'), -1).toLower();
    if (ext == QLatin1String("csv") || ext == QLatin1String("txt"))
        return TextFormat::Csv;
    if (ext == QLatin1String("tsv") || ext == QLatin1String("tab"))
        return TextFormat::Tsv;
    if (ext == QLatin1String("ndjson") || ext == QLatin1String("jsonl"))
        return TextFormat::NdJson;
    if (ext == QLatin1String("json") || ext == QLatin1String("geojson"))
        return TextFormat::Json;
    if (ext == QLatin1String("xml") || ext == QLatin1String("rss") || ext == QLatin1String("atom"))
        return TextFormat::Xml;
    // Sniff the first non-blank bytes.
    QByteArray head = data.left(4096).trimmed();
    if (head.startsWith("\xEF\xBB\xBF"))
        head = head.mid(3).trimmed();
    if (head.startsWith('<'))
        return TextFormat::Xml;
    if (head.startsWith('{') || head.startsWith('[')) {
        // Several top-level objects on separate lines = NDJSON.
        const int nl = int(head.indexOf('\n'));
        if (head.startsWith('{') && nl > 0) {
            const QByteArray first = head.left(nl).trimmed();
            QJsonParseError pe{};
            QJsonDocument::fromJson(first, &pe);
            if (pe.error == QJsonParseError::NoError && head.mid(nl).trimmed().startsWith('{'))
                return TextFormat::NdJson;
        }
        return TextFormat::Json;
    }
    if (head.startsWith("data:") || head.startsWith("event:") || head.startsWith("id:"))
        return TextFormat::EventStream;
    return TextFormat::Csv;
}

ParseOutcome parse_table(const QByteArray& data, const ParseOptions& opts) {
    if (data.trimmed().isEmpty()) {
        ParseOutcome out;
        out.error = SourceError::make(ErrorCode::NoData, tp_tr("The source answered with an empty body"));
        return out;
    }
    const TextFormat fmt = detect_text_format(data, opts);
    switch (fmt) {
        case TextFormat::Csv:
        case TextFormat::Tsv:
            return tp_parse_delimited(data, opts, fmt);
        case TextFormat::Xml:
            return tp_parse_xml(data, opts);
        case TextFormat::NdJson: {
            QJsonArray rows;
            bool truncated = false;
            int line_no = 0;
            for (const QByteArray& line : data.split('\n')) {
                ++line_no;
                const QByteArray l = line.trimmed();
                if (l.isEmpty())
                    continue;
                QJsonParseError pe{};
                const QJsonDocument doc = QJsonDocument::fromJson(l, &pe);
                if (pe.error != QJsonParseError::NoError) {
                    ParseOutcome bad;
                    bad.detected = fmt;
                    bad.error = SourceError::make(ErrorCode::BadArgs,
                                                  tp_tr("Line %1 is not JSON: %2").arg(line_no).arg(pe.errorString()));
                    return bad;
                }
                rows.append(doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()));
                if (opts.max_rows >= 0 && rows.size() >= opts.max_rows) {
                    truncated = true;
                    break;
                }
            }
            ParseOptions o = opts;
            o.rows_path.clear();
            ParseOutcome out = tp_table_from_json(rows, o, fmt);
            out.truncated = out.truncated || truncated;
            return out;
        }
        case TextFormat::EventStream: {
            const QJsonArray events = parse_event_stream(data, opts.max_rows);
            ParseOptions o = opts;
            o.rows_path.clear();
            ParseOutcome out = tp_table_from_json(events, o, fmt);
            out.detected = fmt;
            return out;
        }
        case TextFormat::Json:
        case TextFormat::Auto: {
            QJsonParseError pe{};
            QByteArray body = data;
            if (body.startsWith("\xEF\xBB\xBF"))
                body.remove(0, 3);
            const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
            if (pe.error != QJsonParseError::NoError) {
                ParseOutcome bad;
                bad.detected = TextFormat::Json;
                const QString head = QString::fromUtf8(data.left(120)).simplified();
                bad.error = SourceError::make(
                    ErrorCode::BadArgs,
                    tp_tr("The response is not valid JSON (%1 at byte %2). It starts with: %3")
                        .arg(pe.errorString())
                        .arg(pe.offset)
                        .arg(head));
                return bad;
            }
            const QJsonValue root = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
            return tp_table_from_json(root, opts, TextFormat::Json);
        }
    }
    return ParseOutcome{};
}

QJsonValue json_at_path(const QJsonValue& root, const QString& path) {
    QString p = path.trimmed();
    if (p.isEmpty() || p == QLatin1String("$"))
        return root;
    if (p.startsWith(QLatin1Char('$')))
        p = p.mid(1);
    QJsonValue cur = root;
    int i = 0;
    const int n = int(p.size());
    while (i < n) {
        const QChar ch = p.at(i);
        if (ch == QLatin1Char('.')) {
            ++i;
            continue;
        }
        if (ch == QLatin1Char('[')) {
            const int close = int(p.indexOf(QLatin1Char(']'), i));
            if (close < 0)
                return QJsonValue(QJsonValue::Undefined);
            QString inside = p.mid(i + 1, close - i - 1).trimmed();
            i = close + 1;
            if ((inside.startsWith(QLatin1Char('\'')) && inside.endsWith(QLatin1Char('\''))) ||
                (inside.startsWith(QLatin1Char('"')) && inside.endsWith(QLatin1Char('"')))) {
                inside = inside.mid(1, inside.size() - 2);
                if (!cur.isObject())
                    return QJsonValue(QJsonValue::Undefined);
                cur = cur.toObject().value(inside);
            } else if (inside == QLatin1String("*")) {
                // Projection: apply the rest of the path to every element.
                if (!cur.isArray())
                    return QJsonValue(QJsonValue::Undefined);
                const QString rest = p.mid(i);
                QJsonArray out;
                for (const QJsonValue& el : cur.toArray()) {
                    const QJsonValue v = json_at_path(el, rest);
                    if (!v.isUndefined())
                        out.append(v);
                }
                return out;
            } else {
                bool ok = false;
                int idx = inside.toInt(&ok);
                if (!ok || !cur.isArray())
                    return QJsonValue(QJsonValue::Undefined);
                const QJsonArray a = cur.toArray();
                if (idx < 0)
                    idx += int(a.size());
                if (idx < 0 || idx >= a.size())
                    return QJsonValue(QJsonValue::Undefined);
                cur = a.at(idx);
            }
            if (cur.isUndefined())
                return cur;
            continue;
        }
        int j = i;
        while (j < n && p.at(j) != QLatin1Char('.') && p.at(j) != QLatin1Char('['))
            ++j;
        const QString key = p.mid(i, j - i);
        i = j;
        if (key == QLatin1String("*")) {
            if (cur.isArray())
                continue; // a.* on an array = the array
            if (!cur.isObject())
                return QJsonValue(QJsonValue::Undefined);
            QJsonArray vals;
            const QJsonObject o = cur.toObject();
            for (auto it = o.constBegin(); it != o.constEnd(); ++it)
                vals.append(it.value());
            cur = vals;
            continue;
        }
        if (cur.isArray()) {
            bool ok = false;
            const int idx = key.toInt(&ok);
            if (!ok)
                return QJsonValue(QJsonValue::Undefined);
            cur = cur.toArray().at(idx);
        } else if (cur.isObject()) {
            const QJsonObject o = cur.toObject();
            if (!o.contains(key))
                return QJsonValue(QJsonValue::Undefined);
            cur = o.value(key);
        } else {
            return QJsonValue(QJsonValue::Undefined);
        }
    }
    return cur;
}

QJsonValue find_records(const QJsonValue& root, QString* found_path) {
    if (found_path)
        found_path->clear();
    if (root.isArray()) {
        const QJsonArray a = root.toArray();
        int objs = 0;
        for (const QJsonValue& v : a)
            if (v.isObject())
                ++objs;
        if (objs * 2 > a.size() || a.isEmpty())
            return root;
        const QJsonValue inner = tp_inner_records(a);
        if (!inner.isUndefined()) {
            if (found_path)
                *found_path = QStringLiteral("[1]");
            return inner;
        }
        return root;
    }
    if (!root.isObject())
        return root;
    TpCandidate best;
    tp_walk(root.toObject(), QString(), 1, best);
    if (best.score >= 0) {
        if (found_path)
            *found_path = best.path;
        return best.value;
    }
    // Dict of records ({"AAPL": {...}, "MSFT": {...}}): one row per key.
    const QJsonObject o = root.toObject();
    int objs = 0;
    for (auto it = o.constBegin(); it != o.constEnd(); ++it)
        if (it->isObject())
            ++objs;
    if (o.size() >= 2 && objs * 5 >= o.size() * 4) {
        QJsonArray rows;
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            QJsonObject row{{"key", it.key()}};
            const QJsonObject inner = it->toObject();
            for (auto jt = inner.constBegin(); jt != inner.constEnd(); ++jt)
                row.insert(jt.key(), jt.value());
            rows.append(row);
        }
        return rows;
    }
    return root;
}

QVector<QStringList> split_delimited(const QString& text, QChar delimiter, int max_rows, bool* truncated) {
    QVector<QStringList> rows;
    QStringList row;
    QString field;
    bool in_quotes = false;
    bool field_started = false;
    if (truncated)
        *truncated = false;
    const int n = int(text.size());
    for (int i = 0; i < n; ++i) {
        const QChar ch = text.at(i);
        if (in_quotes) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < n && text.at(i + 1) == QLatin1Char('"')) {
                    field += QLatin1Char('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                field += ch;
            }
            continue;
        }
        if (ch == QLatin1Char('"') && !field_started) {
            in_quotes = true;
            field_started = true;
            continue;
        }
        if (ch == delimiter) {
            row << field;
            field.clear();
            field_started = false;
            continue;
        }
        if (ch == QLatin1Char('\r'))
            continue;
        if (ch == QLatin1Char('\n')) {
            row << field;
            field.clear();
            field_started = false;
            rows.append(row);
            row.clear();
            if (max_rows >= 0 && rows.size() >= max_rows) {
                // Anything but trailing whitespace left = more rows.
                if (truncated && !text.mid(i + 1).trimmed().isEmpty())
                    *truncated = true;
                return rows;
            }
            continue;
        }
        field += ch;
        field_started = true;
    }
    if (!field.isEmpty() || !row.isEmpty() || field_started) {
        row << field;
        rows.append(row);
    }
    return rows;
}

QChar detect_delimiter(const QString& sample) {
    const QChar candidates[] = {QLatin1Char(','), QLatin1Char(';'), QLatin1Char('\t'), QLatin1Char('|')};
    const QStringList lines = sample.split(QLatin1Char('\n'), Qt::SkipEmptyParts).mid(0, 20);
    QChar best = QLatin1Char(',');
    double best_score = -1;
    for (const QChar d : candidates) {
        QVector<int> counts;
        for (const QString& l : lines) {
            int c = 0;
            bool q = false;
            for (const QChar ch : l) {
                if (ch == QLatin1Char('"'))
                    q = !q;
                else if (!q && ch == d)
                    ++c;
            }
            counts << c;
        }
        if (counts.isEmpty() || counts.first() == 0)
            continue;
        int same = 0;
        for (int c : counts)
            if (c == counts.first())
                ++same;
        const double score = double(same) / counts.size() * 1000 + counts.first();
        if (score > best_score) {
            best_score = score;
            best = d;
        }
    }
    return best;
}

QJsonArray parse_event_stream(const QByteArray& data, int max_events) {
    QJsonArray out;
    const QString text = tp_decode(data);
    QString event_name;
    QString event_id;
    QStringList data_lines;
    auto flush = [&]() {
        if (data_lines.isEmpty()) {
            event_name.clear();
            return;
        }
        const QString payload = data_lines.join(QLatin1Char('\n'));
        QJsonParseError pe{};
        const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &pe);
        QJsonObject row;
        if (pe.error == QJsonParseError::NoError && doc.isObject()) {
            row = doc.object();
        } else if (pe.error == QJsonParseError::NoError && doc.isArray()) {
            row.insert(QStringLiteral("data"), doc.array());
        } else {
            row.insert(QStringLiteral("data"), typed_text_cell(payload));
        }
        if (!event_name.isEmpty() && !row.contains(QStringLiteral("event")))
            row.insert(QStringLiteral("event"), event_name);
        if (!event_id.isEmpty() && !row.contains(QStringLiteral("id")))
            row.insert(QStringLiteral("id"), event_id);
        out.append(row);
        data_lines.clear();
        event_name.clear();
    };
    for (const QString& raw : text.split(QLatin1Char('\n'))) {
        QString line = raw;
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (line.isEmpty()) {
            flush();
            if (max_events >= 0 && out.size() >= max_events)
                return out;
            continue;
        }
        if (line.startsWith(QLatin1Char(':')))
            continue; // comment / keep-alive
        const int colon = int(line.indexOf(QLatin1Char(':')));
        const QString field = colon < 0 ? line : line.left(colon);
        QString value = colon < 0 ? QString() : line.mid(colon + 1);
        if (value.startsWith(QLatin1Char(' ')))
            value = value.mid(1);
        if (field == QLatin1String("data"))
            data_lines << value;
        else if (field == QLatin1String("event"))
            event_name = value;
        else if (field == QLatin1String("id"))
            event_id = value;
    }
    flush();
    if (max_events >= 0) {
        while (out.size() > max_events)
            out.removeLast();
    }
    return out;
}

QJsonValue typed_text_cell(const QString& raw) {
    const QString t = raw.trimmed();
    if (t.isEmpty())
        return QJsonValue(QJsonValue::Null);
    // Plain integers / decimals only (no thousands separators, no locale forms — Data Mapping
    // parses those explicitly). Leading zeros and >15 digits stay text (ISIN, CUSIP, zip codes).
    static const QRegularExpression kNumber(QStringLiteral("^-?(0|[1-9]\\d{0,14})(\\.\\d+)?([eE][+-]?\\d{1,3})?$"));
    if (kNumber.match(t).hasMatch()) {
        bool ok = false;
        const double d = t.toDouble(&ok);
        if (ok && std::isfinite(d))
            return d;
    }
    if (t.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)
        return true;
    if (t.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0)
        return false;
    return raw;
}

} // namespace fincept::services::datasources
