#include "services/web_scrape/WebScrapeService.h"

#include "core/logging/Logger.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QStringDecoder>
#include <QTimer>
#include <QXmlStreamReader>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::services {

namespace web_scrape_detail {

constexpr const char* kWebScrapeTag = "WebScrape";
constexpr const char* kWebScrapeDefaultUA = "FinceptTerminal/1.0 (compatible; WebScraperWidget)";
constexpr int kWebScrapeMaxRedirects = 5;

// Compiled once per parse call and shared by every cell of that parse. Not
// function-local statics: the parsers run on pool threads, and per-call
// instances keep each parse independent of any other running concurrently.
struct HtmlRegexes {
    QRegularExpression entity{QStringLiteral("&(#x[0-9a-fA-F]+|#[0-9]+|[a-zA-Z]+[0-9]*);")};
    QRegularExpression script{QStringLiteral("<script[^>]*>.*?</script>"),
                              QRegularExpression::CaseInsensitiveOption |
                                  QRegularExpression::DotMatchesEverythingOption};
    QRegularExpression style{QStringLiteral("<style[^>]*>.*?</style>"),
                             QRegularExpression::CaseInsensitiveOption |
                                 QRegularExpression::DotMatchesEverythingOption};
    QRegularExpression br{QStringLiteral("<br\\s*/?>|</(p|div|li|tr)>"), QRegularExpression::CaseInsensitiveOption};
    QRegularExpression tag{QStringLiteral("<[^>]+>")};
    QRegularExpression ws{QStringLiteral("\\s+")};
    QRegularExpression table{QStringLiteral("<table\\b[^>]*>(.*?)</table>"),
                             QRegularExpression::CaseInsensitiveOption |
                                 QRegularExpression::DotMatchesEverythingOption};
    QRegularExpression row{QStringLiteral("<tr\\b[^>]*>(.*?)</tr>"),
                           QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption};
    QRegularExpression cell{QStringLiteral("<(t[hd])\\b([^>]*)>(.*?)</\\1>"),
                            QRegularExpression::CaseInsensitiveOption |
                                QRegularExpression::DotMatchesEverythingOption};
    QRegularExpression colspan{QStringLiteral("colspan\\s*=\\s*\"?(\\d+)\"?"), QRegularExpression::CaseInsensitiveOption};
    QRegularExpression caption{QStringLiteral("<caption[^>]*>(.*?)</caption>"),
                               QRegularExpression::CaseInsensitiveOption |
                                   QRegularExpression::DotMatchesEverythingOption};
};

// Decode common HTML entities + numeric char refs. Not exhaustive but covers
// the overwhelming majority of real-world tables.
QString decode_html_entities(const QString& s, const HtmlRegexes& rx) {
    static const QHash<QString, QString> kNamed = {
        {"amp", "&"},   {"lt", "<"},     {"gt", ">"},     {"quot", "\""},  {"apos", "'"},   {"nbsp", " "},
        {"ndash", "–"}, {"mdash", "—"},  {"hellip", "…"}, {"laquo", "«"},  {"raquo", "»"},  {"copy", "©"},
        {"reg", "®"},   {"trade", "™"},  {"euro", "€"},   {"pound", "£"},  {"yen", "¥"},    {"cent", "¢"},
        {"deg", "°"},   {"plusmn", "±"}, {"times", "×"},  {"divide", "÷"}, {"middot", "·"}, {"bull", "•"},
        {"larr", "←"},  {"rarr", "→"},   {"uarr", "↑"},   {"darr", "↓"},   {"lsquo", "'"},  {"rsquo", "'"},
        {"ldquo", "“"}, {"rdquo", "”"},
    };
    QString out;
    out.reserve(s.size());
    qsizetype pos = 0;
    auto it = rx.entity.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        out.append(s.mid(pos, m.capturedStart() - pos));
        const QString ref = m.captured(1);
        if (ref.startsWith("#x") || ref.startsWith("#X")) {
            bool ok = false;
            const char32_t cp = ref.mid(2).toUInt(&ok, 16);
            if (ok && cp)
                out.append(QString::fromUcs4(&cp, 1));
        } else if (ref.startsWith('#')) {
            bool ok = false;
            const char32_t cp = ref.mid(1).toUInt(&ok, 10);
            if (ok && cp)
                out.append(QString::fromUcs4(&cp, 1));
        } else {
            auto nit = kNamed.find(ref.toLower());
            if (nit != kNamed.end())
                out.append(nit.value());
            else
                out.append(m.captured(0)); // leave unknown entity as-is
        }
        pos = m.capturedEnd();
    }
    out.append(s.mid(pos));
    return out;
}

// Strip tags and collapse whitespace within a single table cell.
QString strip_tags(const QString& frag, const HtmlRegexes& rx) {
    QString s = frag;
    s.remove(rx.script);
    s.remove(rx.style);
    s.replace(rx.br, " ");
    s.remove(rx.tag);
    s = decode_html_entities(s, rx);
    s.replace(rx.ws, " ");
    return s.trimmed();
}

// Guess encoding: 1) explicit override, 2) charset in Content-Type,
// 3) <meta charset=…> sniff, 4) utf-8.
QString detect_charset(const QByteArray& head_bytes, const QString& content_type, const QString& override_enc) {
    if (!override_enc.isEmpty())
        return override_enc;
    const QRegularExpression ct_re(QStringLiteral("charset\\s*=\\s*([\\w\\-]+)"),
                                   QRegularExpression::CaseInsensitiveOption);
    if (const auto m = ct_re.match(content_type); m.hasMatch())
        return m.captured(1);
    const QString head = QString::fromLatin1(head_bytes.left(2048));
    const QRegularExpression meta_re(QStringLiteral("<meta[^>]+charset\\s*=\\s*[\"']?([\\w\\-]+)"),
                                     QRegularExpression::CaseInsensitiveOption);
    if (const auto m = meta_re.match(head); m.hasMatch())
        return m.captured(1);
    return QStringLiteral("utf-8");
}

QString decode_text(const QByteArray& body, const QString& content_type, const QString& encoding) {
    const QString enc = detect_charset(body, content_type, encoding);
    auto decoder = QStringDecoder(enc.toUtf8().constData());
    return decoder.isValid() ? decoder.decode(body) : QString::fromUtf8(body);
}

// Flatten a JSON value into a row cell string.
QString json_scalar(const QJsonValue& v) {
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (d == static_cast<double>(static_cast<qint64>(d)))
            return QString::number(static_cast<qint64>(d));
        return QString::number(d, 'g', 10);
    }
    if (v.isString())
        return v.toString();
    if (v.isNull())
        return {};
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return {};
}

QJsonValue walk_json_path(const QJsonValue& root, const QString& dotted) {
    if (dotted.isEmpty())
        return root;
    QJsonValue cur = root;
    const QStringList parts = dotted.split('.', Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        bool idx_ok = false;
        const int idx = p.toInt(&idx_ok);
        if (idx_ok && cur.isArray()) {
            cur = cur.toArray().at(idx);
        } else if (cur.isObject()) {
            cur = cur.toObject().value(p);
        } else {
            return {};
        }
    }
    return cur;
}

QVector<ScrapedTable> parse_html_tables(const QString& html) {
    const HtmlRegexes rx;
    QVector<ScrapedTable> out;

    int table_idx = 0;
    auto tit = rx.table.globalMatch(html);
    while (tit.hasNext()) {
        const auto tm = tit.next();
        const QString inner = tm.captured(1);
        ++table_idx;

        ScrapedTable tbl;
        const auto cap = rx.caption.match(inner);
        if (cap.hasMatch()) {
            const QString c = strip_tags(cap.captured(1), rx);
            if (!c.isEmpty())
                tbl.label = c.left(60);
        }
        if (tbl.label.isEmpty())
            tbl.label = QStringLiteral("Table %1").arg(table_idx);

        QVector<QStringList> rows;
        qsizetype max_cols = 0;
        bool first_row_is_header = false;

        auto rit = rx.row.globalMatch(inner);
        int row_no = 0;
        while (rit.hasNext()) {
            const auto rm = rit.next();
            const QString row_html = rm.captured(1);
            QStringList cells;
            bool row_has_th = false;
            auto cit = rx.cell.globalMatch(row_html);
            while (cit.hasNext()) {
                const auto cm = cit.next();
                const QString tag = cm.captured(1).toLower();
                const QString attrs = cm.captured(2);
                const QString content = strip_tags(cm.captured(3), rx);
                int colspan = 1;
                if (const auto cs = rx.colspan.match(attrs); cs.hasMatch())
                    colspan = std::clamp(cs.captured(1).toInt(), 1, 16);
                cells.append(content);
                for (int i = 1; i < colspan; ++i)
                    cells.append(QString());
                if (tag == QLatin1String("th"))
                    row_has_th = true;
            }
            if (cells.isEmpty())
                continue;
            if (row_no == 0 && row_has_th)
                first_row_is_header = true;
            max_cols = std::max<qsizetype>(max_cols, cells.size());
            rows.append(cells);
            ++row_no;
        }
        if (rows.isEmpty())
            continue;

        if (first_row_is_header) {
            tbl.headers = rows.takeFirst();
        } else {
            tbl.headers.reserve(max_cols);
            for (qsizetype i = 0; i < max_cols; ++i)
                tbl.headers.append(QStringLiteral("Col %1").arg(i + 1));
        }
        // Pad ragged rows to max_cols.
        for (QStringList& r : rows) {
            while (r.size() < max_cols)
                r.append(QString());
        }
        while (tbl.headers.size() < max_cols)
            tbl.headers.append(QStringLiteral("Col %1").arg(tbl.headers.size() + 1));

        tbl.rows = rows;
        out.append(std::move(tbl));
    }
    return out;
}

QVector<ScrapedTable> parse_json_tables(const QByteArray& body, const QString& json_path) {
    QVector<ScrapedTable> out;
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError)
        return out;

    QJsonValue root = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
    if (!json_path.isEmpty())
        root = walk_json_path(root, json_path);

    auto table_from_array = [](const QJsonArray& arr, const QString& label) -> ScrapedTable {
        ScrapedTable t;
        t.label = label;
        if (arr.isEmpty())
            return t;
        // Array of objects → union of keys becomes headers.
        const bool obj_array = std::all_of(arr.begin(), arr.end(), [](const QJsonValue& v) { return v.isObject(); });
        if (obj_array) {
            QStringList keys;
            QHash<QString, int> key_idx;
            for (const auto& v : arr) {
                for (const QString& k : v.toObject().keys()) {
                    if (!key_idx.contains(k)) {
                        key_idx.insert(k, static_cast<int>(keys.size()));
                        keys.append(k);
                    }
                }
            }
            t.headers = keys;
            for (const auto& v : arr) {
                const QJsonObject o = v.toObject();
                QStringList row;
                row.reserve(keys.size());
                for (const QString& k : keys)
                    row.append(json_scalar(o.value(k)));
                t.rows.append(row);
            }
            return t;
        }
        // Array of arrays.
        const bool arr_array = std::all_of(arr.begin(), arr.end(), [](const QJsonValue& v) { return v.isArray(); });
        if (arr_array) {
            qsizetype max_cols = 0;
            for (const auto& v : arr)
                max_cols = std::max<qsizetype>(max_cols, v.toArray().size());
            for (qsizetype i = 0; i < max_cols; ++i)
                t.headers.append(QStringLiteral("Col %1").arg(i + 1));
            for (const auto& v : arr) {
                const QJsonArray inner = v.toArray();
                QStringList row;
                row.reserve(max_cols);
                for (qsizetype i = 0; i < max_cols; ++i)
                    row.append(json_scalar(inner.at(i)));
                t.rows.append(row);
            }
            return t;
        }
        // Array of scalars.
        t.headers = {QStringLiteral("value")};
        for (const auto& v : arr)
            t.rows.append({json_scalar(v)});
        return t;
    };

    if (root.isArray()) {
        ScrapedTable t = table_from_array(root.toArray(), QStringLiteral("root[]"));
        if (!t.rows.isEmpty())
            out.append(std::move(t));
    } else if (root.isObject()) {
        const QJsonObject o = root.toObject();
        // Surface every array-valued field at this level as its own table.
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            if (it.value().isArray()) {
                ScrapedTable t = table_from_array(it.value().toArray(), it.key() + "[]");
                if (!t.rows.isEmpty())
                    out.append(std::move(t));
            }
        }
        // Fallback: render the object itself as key/value table.
        if (out.isEmpty()) {
            ScrapedTable kv;
            kv.label = QStringLiteral("object");
            kv.headers = {QStringLiteral("key"), QStringLiteral("value")};
            for (auto it = o.constBegin(); it != o.constEnd(); ++it)
                kv.rows.append({it.key(), json_scalar(it.value())});
            if (!kv.rows.isEmpty())
                out.append(std::move(kv));
        }
    } else if (!root.isUndefined()) {
        // Scalar root after json_path walk.
        ScrapedTable kv;
        kv.label = json_path.isEmpty() ? QStringLiteral("value") : json_path;
        kv.headers = {QStringLiteral("value")};
        kv.rows.append({json_scalar(root)});
        out.append(std::move(kv));
    }
    return out;
}

// RFC-4180-ish parser: quoted fields, escaped quotes, embedded newlines.
QVector<ScrapedTable> parse_csv_tables(const QString& text, QChar delim) {
    QVector<QStringList> rows;
    QStringList cur_row;
    QString cur_field;
    bool in_quotes = false;
    qsizetype max_cols = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    cur_field.append('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                cur_field.append(c);
            }
        } else {
            if (c == '"' && cur_field.isEmpty()) {
                in_quotes = true;
            } else if (c == delim) {
                cur_row.append(cur_field);
                cur_field.clear();
            } else if (c == '\r') {
                // swallow; \n will flush
            } else if (c == '\n') {
                cur_row.append(cur_field);
                cur_field.clear();
                max_cols = std::max<qsizetype>(max_cols, cur_row.size());
                rows.append(cur_row);
                cur_row.clear();
            } else {
                cur_field.append(c);
            }
        }
    }
    if (!cur_field.isEmpty() || !cur_row.isEmpty()) {
        cur_row.append(cur_field);
        max_cols = std::max<qsizetype>(max_cols, cur_row.size());
        rows.append(cur_row);
    }
    if (rows.isEmpty() || max_cols < 2)
        return {};

    ScrapedTable t;
    t.label = delim == '\t' ? QStringLiteral("TSV") : QStringLiteral("CSV");
    t.headers = rows.takeFirst();
    while (t.headers.size() < max_cols)
        t.headers.append(QStringLiteral("Col %1").arg(t.headers.size() + 1));
    for (QStringList& r : rows) {
        while (r.size() < max_cols)
            r.append(QString());
    }
    t.rows = rows;
    return {t};
}

QVector<ScrapedTable> parse_xml_tables(const QByteArray& body) {
    QVector<ScrapedTable> out;
    QXmlStreamReader xml(body);

    // Collect element records keyed by tag name, with their child scalar
    // fields. Repeating element names (RSS <item>, Atom <entry>, <row>, …)
    // with at least two occurrences become tables.
    struct Record {
        QMap<QString, QString> fields;
    };
    QHash<QString, QVector<Record>> by_tag;

    while (!xml.atEnd() && !xml.hasError()) {
        const auto tok = xml.readNext();
        if (tok != QXmlStreamReader::StartElement)
            continue;
        const QString tag = xml.name().toString();
        Record rec;
        for (const auto& attr : xml.attributes())
            rec.fields.insert(QStringLiteral("@") + attr.name().toString(), attr.value().toString());
        // Read children as scalars (one level deep; nested text is concatenated).
        int depth = 1;
        while (!xml.atEnd() && depth > 0) {
            const auto t = xml.readNext();
            if (t == QXmlStreamReader::StartElement) {
                const QString ck = xml.name().toString();
                QString val;
                int sub_depth = 1;
                while (!xml.atEnd() && sub_depth > 0) {
                    const auto tt = xml.readNext();
                    if (tt == QXmlStreamReader::StartElement)
                        ++sub_depth;
                    else if (tt == QXmlStreamReader::EndElement)
                        --sub_depth;
                    else if (tt == QXmlStreamReader::Characters)
                        val.append(xml.text().toString());
                }
                const QString norm = val.simplified();
                if (!norm.isEmpty())
                    rec.fields.insert(ck, norm);
            } else if (t == QXmlStreamReader::EndElement) {
                --depth;
            }
        }
        if (!rec.fields.isEmpty())
            by_tag[tag].append(rec);
    }

    for (auto it = by_tag.cbegin(); it != by_tag.cend(); ++it) {
        const QVector<Record>& recs = it.value();
        if (recs.size() < 2)
            continue;
        QStringList keys;
        QHash<QString, int> key_idx;
        for (const auto& r : recs) {
            for (auto fit = r.fields.cbegin(); fit != r.fields.cend(); ++fit) {
                if (!key_idx.contains(fit.key())) {
                    key_idx.insert(fit.key(), static_cast<int>(keys.size()));
                    keys.append(fit.key());
                }
            }
        }
        if (keys.isEmpty())
            continue;
        ScrapedTable t;
        t.label = QStringLiteral("<%1> × %2").arg(it.key()).arg(recs.size());
        t.headers = keys;
        for (const auto& r : recs) {
            QStringList row;
            row.reserve(keys.size());
            for (const QString& k : keys)
                row.append(r.fields.value(k));
            t.rows.append(row);
        }
        out.append(std::move(t));
    }

    // Prefer RSS/Atom "item"/"entry" tables first for usability.
    std::stable_sort(out.begin(), out.end(), [](const ScrapedTable& a, const ScrapedTable& b) {
        auto rank = [](const QString& label) {
            if (label.startsWith(QLatin1String("<item>")))
                return 0;
            if (label.startsWith(QLatin1String("<entry>")))
                return 1;
            return 2;
        };
        return rank(a.label) < rank(b.label);
    });
    return out;
}

/// Pure function of (request, body, content type) — runs on a pool thread.
WebScrapeResult parse_body(const WebScrapeRequest& req, const QByteArray& body, const QString& content_type) {
    WebScrapeResult res;
    res.bytes = body.size();
    QVector<ScrapedTable> tables;
    QString fmt;

    auto try_html = [&]() {
        tables = parse_html_tables(decode_text(body, content_type, req.encoding));
        if (!tables.isEmpty())
            fmt = QStringLiteral("HTML");
    };
    auto try_json = [&]() {
        tables = parse_json_tables(body, req.json_path);
        if (!tables.isEmpty())
            fmt = QStringLiteral("JSON");
    };
    auto try_csv = [&](QChar delim, const QString& label) {
        tables = parse_csv_tables(decode_text(body, content_type, req.encoding), delim);
        if (!tables.isEmpty())
            fmt = label;
    };
    auto try_xml = [&]() {
        tables = parse_xml_tables(body);
        if (!tables.isEmpty())
            fmt = QStringLiteral("XML");
    };

    const QString forced = req.force_format.toLower();
    if (forced == QLatin1String("html")) {
        try_html();
    } else if (forced == QLatin1String("json")) {
        try_json();
    } else if (forced == QLatin1String("csv")) {
        try_csv(',', QStringLiteral("CSV"));
    } else if (forced == QLatin1String("tsv")) {
        try_csv('\t', QStringLiteral("TSV"));
    } else if (forced == QLatin1String("xml")) {
        try_xml();
    } else {
        // Content-Type first, then a content sniff, then HTML and CSV as a
        // last resort (text/plain bodies that embed markup or delimited data).
        const QString ct = content_type.toLower();
        if (ct.contains(QLatin1String("json")))
            try_json();
        else if (ct.contains(QLatin1String("csv")))
            try_csv(',', QStringLiteral("CSV"));
        else if (ct.contains(QLatin1String("tab-separated")) || ct.contains(QLatin1String("tsv")))
            try_csv('\t', QStringLiteral("TSV"));
        else if (ct.contains(QLatin1String("xml")) || ct.contains(QLatin1String("rss")) ||
                 ct.contains(QLatin1String("atom")))
            try_xml();
        else if (ct.contains(QLatin1String("html")))
            try_html();

        if (tables.isEmpty()) {
            const QByteArray head = body.left(256).trimmed();
            if (head.startsWith('{') || head.startsWith('['))
                try_json();
            else if (head.startsWith("<?xml") || head.startsWith("<rss") || head.startsWith("<feed"))
                try_xml();
            else if (head.contains("<table") || head.contains("<TABLE") || head.contains("<html") ||
                     head.contains("<!DOCTYPE"))
                try_html();
        }
        if (tables.isEmpty())
            try_html();
        if (tables.isEmpty())
            try_csv(',', QStringLiteral("CSV"));
    }

    res.ok = true; // the fetch worked; an empty table list is reported by the caller
    res.format = fmt;
    res.tables = std::move(tables);
    return res;
}

} // namespace web_scrape_detail

namespace wsd = web_scrape_detail;

WebScrapeService& WebScrapeService::instance() {
    static WebScrapeService s;
    return s;
}

WebScrapeService::WebScrapeService(QObject* parent) : QObject(parent) {}

QString WebScrapeService::validate_url(const QUrl& url) {
    if (!url.isValid() || url.isRelative())
        return tr("Invalid URL");
    // Web pages only. QNetworkAccessManager also serves file:// and qrc://,
    // which would let a tile render arbitrary local files.
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return tr("Only http:// and https:// URLs are supported");
    if (url.host().isEmpty())
        return tr("URL has no host");
    // Credentials in the URL end up in logs, layouts and screenshots; the
    // headers field exists for authentication.
    if (!url.userInfo().isEmpty())
        return tr("Put credentials in the request headers, not in the URL");
    return {};
}

QString WebScrapeService::redacted(const QUrl& url) {
    return url.toString(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment);
}

quint64 WebScrapeService::scrape(const WebScrapeRequest& req, QObject* context, Callback cb) {
    // fromUserInput() turns "example.com/x" into http://example.com/x and a
    // local path into a file:// URL — which validate_url() then rejects.
    const QUrl url = QUrl::fromUserInput(req.url.trimmed());
    if (const QString why = validate_url(url); !why.isEmpty()) {
        WebScrapeResult res;
        res.error = why;
        if (cb)
            cb(res);
        return 0;
    }

    if (!nam_)
        nam_ = new QNetworkAccessManager(this);

    QNetworkRequest nr(url);
    nr.setTransferTimeout(kTransferTimeoutMs);
    nr.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    nr.setMaximumRedirectsAllowed(wsd::kWebScrapeMaxRedirects);
    nr.setHeader(QNetworkRequest::UserAgentHeader,
                 req.user_agent.isEmpty() ? QString::fromLatin1(wsd::kWebScrapeDefaultUA) : req.user_agent);
    nr.setRawHeader("Accept",
                    "text/html,application/xhtml+xml,application/json,text/csv,application/xml;q=0.9,*/*;q=0.8");
    nr.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    for (auto it = req.headers.cbegin(); it != req.headers.cend(); ++it) {
        const QByteArray name = it.key().trimmed().toLatin1();
        // A header name with CR/LF or ':' would be rejected (or worse) by the
        // stack — skip it rather than sending a malformed request.
        if (name.isEmpty() || name.contains('\r') || name.contains('\n') || name.contains(':'))
            continue;
        QByteArray value = it.value().toUtf8();
        value.replace('\r', ' ').replace('\n', ' ');
        nr.setRawHeader(name, value);
    }

    const quint64 id = next_id_++;
    QNetworkReply* reply = nam_->get(nr);

    Pending p;
    p.reply = reply;
    p.context = context;
    p.cb = std::move(cb);
    p.req = req;
    pending_.insert(id, std::move(p));

    // The whole body is buffered, and the URL is user-configured: abort as soon
    // as either the advertised or the received size crosses the cap.
    connect(reply, &QNetworkReply::downloadProgress, this, [this, id, reply](qint64 received, qint64 total) {
        if (received <= kMaxBodyBytes && total <= kMaxBodyBytes)
            return;
        auto it = pending_.find(id);
        if (it == pending_.end() || it->too_large)
            return;
        it->too_large = true;
        reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, id]() { on_finished(id); });

    // setTransferTimeout() only fires on inactivity; a server trickling bytes
    // forever would never trip it. Hard wall-clock deadline on top.
    QPointer<QNetworkReply> guard(reply);
    QTimer::singleShot(kDeadlineMs, this, [this, id, guard]() {
        auto it = pending_.find(id);
        if (it == pending_.end() || !guard || guard->isFinished())
            return;
        it->deadline_hit = true;
        guard->abort();
    });

    LOG_DEBUG(wsd::kWebScrapeTag, QString("Fetching %1").arg(redacted(url)));
    return id;
}

void WebScrapeService::cancel(quint64 id) {
    auto it = pending_.find(id);
    if (it == pending_.end())
        return;
    QPointer<QNetworkReply> reply = it->reply;
    pending_.erase(it); // erase first: abort() emits finished() synchronously
    if (reply) {
        reply->abort();
        reply->deleteLater();
    }
}

void WebScrapeService::on_finished(quint64 id) {
    auto it = pending_.find(id);
    if (it == pending_.end())
        return; // cancelled
    QNetworkReply* reply = it->reply.data();
    if (!reply) {
        pending_.erase(it);
        return;
    }
    reply->deleteLater();
    it->reply.clear();

    const QUrl url = reply->url();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (it->too_large || it->deadline_hit || reply->error() != QNetworkReply::NoError) {
        WebScrapeResult res;
        res.http_status = status;
        if (it->too_large)
            res.error = tr("Response is larger than the %1 MB limit").arg(kMaxBodyBytes / (1024 * 1024));
        else if (it->deadline_hit)
            res.error = tr("Timed out after %1 s").arg(kDeadlineMs / 1000);
        else if (reply->error() == QNetworkReply::OperationCanceledError ||
                 reply->error() == QNetworkReply::TimeoutError)
            res.error = tr("No data for %1 s — timed out").arg(kTransferTimeoutMs / 1000);
        else if (status >= 400)
            res.error = tr("HTTP %1 %2")
                            .arg(status)
                            .arg(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
        else
            res.error = reply->errorString();
        LOG_WARN(wsd::kWebScrapeTag, QString("Fetch %1 failed: %2").arg(redacted(url), res.error));
        const auto ctx = it->context;
        const auto cb = it->cb;
        pending_.erase(it);
        deliver(ctx, cb, res);
        return;
    }

    const QByteArray body = reply->readAll();
    const QString content_type = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    if (body.isEmpty()) {
        WebScrapeResult res;
        res.http_status = status;
        res.error = tr("Empty response");
        const auto ctx = it->context;
        const auto cb = it->cb;
        pending_.erase(it);
        deliver(ctx, cb, res);
        return;
    }

    // Parse on a pool thread; the entry stays in pending_ so cancel() still
    // works while the parse runs.
    const WebScrapeRequest req = it->req;
    QPointer<WebScrapeService> self = this;
    (void)QtConcurrent::run([self, id, req, body, content_type, status]() {
        WebScrapeResult res = wsd::parse_body(req, body, content_type);
        res.http_status = status;
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, id, res]() {
                if (!self)
                    return;
                auto pit = self->pending_.find(id);
                if (pit == self->pending_.end())
                    return; // cancelled while parsing
                const auto ctx = pit->context;
                const auto cb = pit->cb;
                self->pending_.erase(pit);
                self->deliver(ctx, cb, res);
            },
            Qt::QueuedConnection);
    });
}

void WebScrapeService::deliver(QPointer<QObject> context, const Callback& cb, const WebScrapeResult& result) {
    // `context` is the caller's lifetime guard (scrape() requires one): a tile
    // closed mid-fetch must not have its callback run on a dead object.
    if (!context || !cb)
        return;
    cb(result);
}

} // namespace fincept::services
