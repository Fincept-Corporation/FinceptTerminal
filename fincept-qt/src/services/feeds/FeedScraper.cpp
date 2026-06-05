#include "services/feeds/FeedScraper.h"

#include "services/feeds/FeedParseUtil.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>

#include <functional>
#include <initializer_list>

namespace fincept::feeds {

namespace {

constexpr int kSampleMax = 60;

QString make_id(const QString& feed_id, const FeedItem& it) {
    const QString basis = feed_id + "|" + (it.link.isEmpty() ? it.title : it.link);
    return QString::fromLatin1(QCryptographicHash::hash(basis.toUtf8(), QCryptographicHash::Sha1).toHex());
}

// Resolve a JSON dot-path ("data.items" / "meta.url"). Returns the value or undefined.
QJsonValue json_path(const QJsonValue& root, const QString& path) {
    if (path.isEmpty())
        return root;
    QJsonValue cur = root;
    for (const QString& key : path.split('.', Qt::SkipEmptyParts)) {
        if (cur.isObject())
            cur = cur.toObject().value(key);
        else
            return QJsonValue::Undefined;
    }
    return cur;
}

QString json_str(const QJsonValue& v) {
    if (v.isString())
        return v.toString();
    if (v.isDouble())
        return QString::number(v.toDouble());
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isArray())
        return QStringLiteral("[…]");
    if (v.isObject())
        return QStringLiteral("{…}");
    return {};
}

// Apply a subscription's manual mapping to one record, given a value extractor.
// Roles drive the card view (title/summary/link/time); other fields become table
// columns. Returns false if the record produced nothing useful.
bool build_mapped_item(const FeedSubscription& sub, const std::function<QString(const QString&)>& get, FeedItem& it) {
    it = {};
    it.source = sub.name;
    for (const auto& f : sub.mapping.fields) {
        const QString val = get(f.path);
        if (f.role == "title")
            it.title = val.left(300);
        else if (f.role == "summary")
            it.summary = strip_html(val);
        else if (f.role == "link")
            it.link = val;
        else if (f.role == "time")
            it.sort_ts = parse_feed_datetime(val, it.time, sub.mapping.time_format);
        else if (!f.label.isEmpty())
            it.fields.insert(f.label, val);
    }
    if (it.title.isEmpty() && it.link.isEmpty() && it.fields.isEmpty())
        return false;
    if (it.time.isEmpty())
        it.time = QDateTime::currentDateTime().toString("MMM dd, HH:mm");
    if (it.sort_ts == 0)
        it.sort_ts = QDateTime::currentSecsSinceEpoch();
    it.id = make_id(sub.id, it);
    return true;
}

} // namespace

QVector<FeedItem> FeedScraper::parse(const QByteArray& body, const FeedSubscription& sub) {
    if (looks_like_html(body))
        return {}; // caller surfaces access-denied as Error

    FeedFormat fmt = sub.format;
    if (fmt == FeedFormat::Auto)
        fmt = sniff_format(body);

    if (sub.parse_mode == ParseMode::Manual) {
        switch (fmt) {
            case FeedFormat::Json: return parse_json(body, sub);
            case FeedFormat::Csv: return parse_csv(body, sub);
            default: return parse_xml_manual(body, sub); // rss/atom/xml all element-based
        }
    }
    switch (fmt) {
        case FeedFormat::Json: return parse_json(body, sub);
        case FeedFormat::Csv: return parse_csv(body, sub);
        case FeedFormat::Html: return {}; // auto HTML table extraction out of scope for v1 auto mode
        default: return parse_rss_atom(body, sub);
    }
}

// ── Auto RSS/Atom (ported from NewsService::parse_rss_xml, emitting FeedItem) ──
QVector<FeedItem> FeedScraper::parse_rss_atom(const QByteArray& xml, const FeedSubscription& sub) {
    QVector<FeedItem> items;
    QXmlStreamReader reader(xml);
    bool in_item = false;
    FeedItem cur;
    QString tag;

    while (!reader.atEnd()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::StartElement) {
            tag = reader.name().toString();
            if (tag == "item" || tag == "entry") {
                in_item = true;
                cur = {};
                cur.source = sub.name;
            }
            if (in_item && tag == "link") {
                const auto href = reader.attributes().value("href").toString();
                const auto rel = reader.attributes().value("rel").toString();
                if (!href.isEmpty() && (rel.isEmpty() || rel == "alternate") && cur.link.isEmpty())
                    cur.link = href;
            }
        } else if (token == QXmlStreamReader::Characters && in_item) {
            const QString text = reader.text().toString().trimmed();
            if (text.isEmpty())
                continue;
            if (tag == "title" && cur.title.isEmpty()) {
                cur.title = text.left(200);
            } else if ((tag == "description" || tag == "summary" || tag == "encoded") && cur.summary.isEmpty()) {
                cur.summary = strip_html(text);
            } else if (tag == "link" && cur.link.isEmpty()) {
                cur.link = text;
            } else if ((tag == "guid" || tag == "id") && cur.link.isEmpty() && text.startsWith("http")) {
                cur.link = text;
            } else if (tag == "pubDate" || tag == "published" || tag == "updated" || tag == "date") {
                if (cur.sort_ts == 0)
                    cur.sort_ts = parse_feed_datetime(text, cur.time);
            }
        } else if (token == QXmlStreamReader::EndElement) {
            const QString end = reader.name().toString();
            if ((end == "item" || end == "entry") && in_item) {
                in_item = false;
                if (cur.title.isEmpty())
                    continue;
                if (cur.time.isEmpty())
                    cur.time = QDateTime::currentDateTime().toString("MMM dd, HH:mm");
                if (cur.sort_ts == 0)
                    cur.sort_ts = QDateTime::currentSecsSinceEpoch();
                cur.id = make_id(sub.id, cur);
                items.append(std::move(cur));
            }
        }
    }
    return items;
}

// ── Manual XML: read each item_selector element into a tag bucket, then map ──
QVector<FeedItem> FeedScraper::parse_xml_manual(const QByteArray& xml, const FeedSubscription& sub) {
    const QString itemTag = sub.mapping.item_selector.isEmpty() ? QStringLiteral("item") : sub.mapping.item_selector;
    QVector<FeedItem> items;
    QXmlStreamReader reader(xml);
    bool in_item = false;
    QHash<QString, QString> bucket; // element-name (or name@attr / @attr) -> text

    QString tag;
    while (!reader.atEnd()) {
        const auto token = reader.readNext();
        if (token == QXmlStreamReader::StartElement) {
            tag = reader.name().toString();
            if (tag == itemTag) {
                in_item = true;
                bucket.clear();
                for (const auto& a : reader.attributes())
                    bucket.insert("@" + a.name().toString(), a.value().toString());
            } else if (in_item) {
                for (const auto& a : reader.attributes())
                    bucket.insert(tag + "@" + a.name().toString(), a.value().toString());
            }
        } else if (token == QXmlStreamReader::Characters && in_item) {
            const QString text = reader.text().toString().trimmed();
            if (!text.isEmpty() && tag != itemTag && !bucket.contains(tag))
                bucket.insert(tag, text);
        } else if (token == QXmlStreamReader::EndElement) {
            if (reader.name().toString() == itemTag && in_item) {
                in_item = false;
                FeedItem it;
                if (build_mapped_item(sub, [&](const QString& p) { return bucket.value(p); }, it))
                    items.append(std::move(it));
            }
        }
    }
    return items;
}

// ── JSON: auto maps common keys; manual applies sub.mapping ──
QVector<FeedItem> FeedScraper::parse_json(const QByteArray& body, const FeedSubscription& sub) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError)
        return {};

    const bool manual = sub.parse_mode == ParseMode::Manual;
    const QJsonValue root = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
    QJsonValue arrVal =
        manual && !sub.mapping.item_selector.isEmpty() ? json_path(root, sub.mapping.item_selector) : root;
    if (!arrVal.isArray() && doc.isObject()) {
        for (const char* k : {"items", "data", "results", "articles", "entries"}) {
            const QJsonValue v = doc.object().value(QLatin1String(k));
            if (v.isArray()) {
                arrVal = v;
                break;
            }
        }
    }
    if (!arrVal.isArray())
        return {};

    QVector<FeedItem> items;
    for (const auto& v : arrVal.toArray()) {
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();
        FeedItem it;
        if (manual) {
            if (build_mapped_item(sub, [&](const QString& p) { return json_str(json_path(o, p)); }, it))
                items.append(std::move(it));
            continue;
        }
        // Auto: map common keys.
        auto pick = [&](std::initializer_list<const char*> defs) -> QString {
            for (const char* d : defs) {
                const auto val = o.value(QLatin1String(d));
                if (!val.isUndefined())
                    return json_str(val);
            }
            return {};
        };
        it.source = sub.name;
        it.title = pick({"title", "headline", "name"});
        it.summary = strip_html(pick({"summary", "description", "abstract"}));
        it.link = pick({"url", "link", "href"});
        const QString ts = pick({"published", "date", "pubDate", "created_at", "time"});
        if (!ts.isEmpty())
            it.sort_ts = parse_feed_datetime(ts, it.time);
        if (it.title.isEmpty())
            continue;
        if (it.time.isEmpty())
            it.time = QDateTime::currentDateTime().toString("MMM dd, HH:mm");
        if (it.sort_ts == 0)
            it.sort_ts = QDateTime::currentSecsSinceEpoch();
        it.id = make_id(sub.id, it);
        items.append(std::move(it));
    }
    return items;
}

// ── CSV: header row -> columns; auto = all columns; manual = mapped ──
QVector<FeedItem> FeedScraper::parse_csv(const QByteArray& body, const FeedSubscription& sub) {
    const QString text = QString::fromUtf8(body);
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 2)
        return {};
    const QChar delim = lines.first().contains('\t') ? QChar('\t') : QChar(',');
    const QStringList headers = lines.first().split(delim);
    const bool manual = sub.parse_mode == ParseMode::Manual;

    auto col_index = [&](const QString& path) -> int {
        if (path.isEmpty())
            return -1;
        if (path.startsWith('#'))
            return path.mid(1).toInt();
        return headers.indexOf(path);
    };

    QVector<FeedItem> items;
    for (int r = 1; r < lines.size(); ++r) {
        const QStringList cells = lines[r].split(delim);
        auto cell = [&](int i) { return (i >= 0 && i < cells.size()) ? cells[i].trimmed() : QString(); };
        FeedItem it;
        if (manual) {
            if (build_mapped_item(sub, [&](const QString& p) { return cell(col_index(p)); }, it))
                items.append(std::move(it));
            continue;
        }
        it.source = sub.name;
        it.title = cell(0);
        for (int c = 0; c < headers.size() && c < cells.size(); ++c)
            it.fields.insert(headers[c].trimmed(), cell(c));
        if (it.title.isEmpty() && it.fields.isEmpty())
            continue;
        it.time = QDateTime::currentDateTime().toString("MMM dd, HH:mm");
        it.sort_ts = QDateTime::currentSecsSinceEpoch();
        it.id = make_id(sub.id, it);
        items.append(std::move(it));
    }
    return items;
}

// ════════════════════════════ Discovery ════════════════════════════════════════

DiscoveredSchema FeedScraper::discover(const QByteArray& body, const FeedSubscription& sub) {
    if (looks_like_html(body))
        return {};
    FeedFormat fmt = sub.format;
    if (fmt == FeedFormat::Auto)
        fmt = sniff_format(body);
    switch (fmt) {
        case FeedFormat::Json: return discover_json(body, sub);
        case FeedFormat::Csv: return discover_csv(body);
        case FeedFormat::Html: return {};
        default: return discover_xml(body);
    }
}

DiscoveredSchema FeedScraper::discover_xml(const QByteArray& xml) {
    DiscoveredSchema s;

    // Pass 1: count element occurrences + first-seen depth to find the repeating record.
    QHash<QString, int> count;
    QHash<QString, int> firstDepth;
    QStringList order;
    bool sawFeedRoot = false;
    bool sawRssRoot = false;
    {
        QXmlStreamReader r(xml);
        int depth = 0;
        while (!r.atEnd()) {
            const auto t = r.readNext();
            if (t == QXmlStreamReader::StartElement) {
                ++depth;
                const QString n = r.name().toString();
                if (n == "feed")
                    sawFeedRoot = true;
                if (n == "rss" || n == "rdf" || n == "RDF")
                    sawRssRoot = true;
                if (!count.contains(n)) {
                    firstDepth[n] = depth;
                    order << n;
                }
                ++count[n];
            } else if (t == QXmlStreamReader::EndElement) {
                --depth;
            }
        }
    }

    QStringList cands;
    for (const QString& n : order)
        if (count.value(n) >= 2 && firstDepth.value(n) >= 2)
            cands << n;

    QString item;
    if (cands.contains("item"))
        item = "item";
    else if (cands.contains("entry"))
        item = "entry";
    else if (!cands.isEmpty()) {
        item = cands.first();
        int best = firstDepth.value(item);
        for (const QString& n : cands)
            if (firstDepth.value(n) < best) {
                best = firstDepth.value(n);
                item = n;
            }
    } else {
        for (const QString& n : order)
            if (firstDepth.value(n) == 2) {
                item = n;
                break;
            }
    }

    s.format = sawFeedRoot ? "atom" : (sawRssRoot ? "rss" : "xml");
    s.item_selector = item;
    s.item_options = cands.isEmpty() ? order : cands;
    if (item.isEmpty())
        return s;

    // Pass 2: collect the chosen record's child tags + attributes (first occurrence).
    QStringList fieldOrder;
    QHash<QString, QString> sample;
    auto addF = [&](const QString& p, const QString& v) {
        if (p.isEmpty())
            return;
        if (!sample.contains(p)) {
            fieldOrder << p;
            sample[p] = v.left(kSampleMax);
        }
    };
    {
        QXmlStreamReader r(xml);
        bool in = false;
        bool done = false;
        QString tag;
        while (!r.atEnd() && !done) {
            const auto t = r.readNext();
            if (t == QXmlStreamReader::StartElement) {
                tag = r.name().toString();
                if (tag == item) {
                    in = true;
                    for (const auto& a : r.attributes())
                        addF("@" + a.name().toString(), a.value().toString());
                } else if (in) {
                    for (const auto& a : r.attributes())
                        addF(tag + "@" + a.name().toString(), a.value().toString());
                }
            } else if (t == QXmlStreamReader::Characters && in) {
                const QString txt = r.text().toString().trimmed();
                if (!txt.isEmpty() && tag != item)
                    addF(tag, txt);
            } else if (t == QXmlStreamReader::EndElement) {
                if (r.name().toString() == item && in)
                    done = true;
            }
        }
    }
    for (const QString& p : fieldOrder)
        s.fields.append({p, sample.value(p)});
    s.ok = !s.fields.isEmpty();
    return s;
}

DiscoveredSchema FeedScraper::discover_json(const QByteArray& body, const FeedSubscription& sub) {
    DiscoveredSchema s;
    s.format = "json";
    QJsonParseError e;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &e);
    if (e.error != QJsonParseError::NoError)
        return s;

    QJsonArray arr;
    QString path;
    if (doc.isArray()) {
        arr = doc.array();
    } else {
        const QJsonObject obj = doc.object();
        QStringList tryKeys{"items", "data", "results", "articles", "entries"};
        for (auto it = obj.begin(); it != obj.end(); ++it)
            if (it.value().isArray() && !tryKeys.contains(it.key()))
                tryKeys << it.key();
        for (const QString& k : tryKeys) {
            if (obj.value(k).isArray()) {
                arr = obj.value(k).toArray();
                path = k;
                s.item_options << k;
                break;
            }
        }
    }
    // Honour an explicit override if the user already picked an array path.
    if (!sub.mapping.item_selector.isEmpty()) {
        const QJsonValue v = json_path(doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object()),
                                       sub.mapping.item_selector);
        if (v.isArray()) {
            arr = v.toArray();
            path = sub.mapping.item_selector;
        }
    }
    s.item_selector = path;
    if (!path.isEmpty() && !s.item_options.contains(path))
        s.item_options << path;
    if (arr.isEmpty())
        return s;

    const QJsonObject first = arr.first().toObject();
    for (auto it = first.begin(); it != first.end(); ++it) {
        if (it.value().isObject()) {
            const QJsonObject sub2 = it.value().toObject();
            for (auto j = sub2.begin(); j != sub2.end(); ++j)
                s.fields.append({it.key() + "." + j.key(), json_str(j.value()).left(kSampleMax)});
        } else {
            s.fields.append({it.key(), json_str(it.value()).left(kSampleMax)});
        }
    }
    s.ok = !s.fields.isEmpty();
    return s;
}

DiscoveredSchema FeedScraper::discover_csv(const QByteArray& body) {
    DiscoveredSchema s;
    s.format = "csv";
    const QString text = QString::fromUtf8(body);
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty())
        return s;
    const QChar delim = lines.first().contains('\t') ? QChar('\t') : QChar(',');
    const QStringList headers = lines.first().split(delim);
    const QStringList firstRow = lines.size() > 1 ? lines[1].split(delim) : QStringList();
    for (int i = 0; i < headers.size(); ++i)
        s.fields.append({headers[i].trimmed(), i < firstRow.size() ? firstRow[i].trimmed().left(kSampleMax) : QString()});
    s.ok = !s.fields.isEmpty();
    return s;
}

} // namespace fincept::feeds
