#pragma once
// Generalized feed subsystem — shared value types.
// Plain data; no Qt widget or equity dependency (engine is reusable across screens).

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <utility>

namespace fincept::feeds {

enum class ParseMode { Auto, Manual };
enum class FeedFormat { Auto, Rss, Atom, Xml, Json, Csv, Html };
enum class DisplayMode { Cards, Table };
enum class FeedStatus { Idle, Fetching, Ok, Error };

/// One mapped field: an output column with a display `label`, sourced from `path`
/// (a discovered feed tag), with an optional semantic `role`. Roles drive the card
/// view; role == "" means a plain extra column shown only in the table view.
///   path syntax (auto-filled by discovery, rarely typed by hand):
///     XML : element name, optional "@attr" (e.g. "guid", "link@href")
///     JSON: dot-path (e.g. "meta.url")
///     CSV : header name, or "#<index>" (0-based)
struct MappedField {
    QString label;       // display column name
    QString path;        // source tag/path
    QString role;        // "" | "title" | "summary" | "link" | "time"
};

/// Manual field mapping — an unlimited, user-editable list of MappedFields plus the
/// repeating-record selector. Populated by auto-discovery (FeedScraper::discover).
struct FieldMapping {
    QString item_selector; // XML repeating element ("item"/"entry"/custom); JSON array path; CSV = ""
    QString time_format;   // optional QDateTime format hint for the "time" role
    QVector<MappedField> fields;

    /// Source path for the field tagged with `role`, or empty if none.
    QString role_path(const QString& role) const {
        for (const auto& f : fields)
            if (f.role == role)
                return f.path;
        return {};
    }

    QJsonObject to_json() const {
        QJsonObject o;
        o["item_selector"] = item_selector;
        o["time_format"] = time_format;
        QJsonArray arr;
        for (const auto& f : fields) {
            QJsonObject e;
            e["label"] = f.label;
            e["path"] = f.path;
            e["role"] = f.role;
            arr.append(e);
        }
        o["fields"] = arr;
        return o;
    }
    static FieldMapping from_json(const QJsonObject& o) {
        FieldMapping m;
        m.item_selector = o["item_selector"].toString();
        m.time_format = o["time_format"].toString();
        if (o.contains("fields")) {
            for (const auto& v : o["fields"].toArray()) {
                const auto e = v.toObject();
                m.fields.append({e["label"].toString(), e["path"].toString(), e["role"].toString()});
            }
        } else {
            // Back-compat with the original fixed-field shape.
            auto add = [&](const QString& role, const QString& path, const QString& label) {
                if (!path.isEmpty())
                    m.fields.append({label, path, role});
            };
            add("title", o["title_path"].toString(), "Title");
            add("summary", o["summary_path"].toString(), "Summary");
            add("link", o["link_path"].toString(), "Link");
            add("time", o["time_path"].toString(), "Time");
            for (const auto& v : o["extra"].toArray()) {
                const auto e = v.toObject();
                add("", e["path"].toString(), e["label"].toString());
            }
        }
        return m;
    }
};

/// A user feed configuration row (persisted in feed_subscriptions).
struct FeedSubscription {
    QString id; // "feed-<uuid>"
    QString name;
    QString url;
    int refresh_sec = 300;
    ParseMode parse_mode = ParseMode::Auto;
    FeedFormat format = FeedFormat::Auto;
    FieldMapping mapping; // used when parse_mode == Manual
    DisplayMode display_mode = DisplayMode::Cards;
    QHash<QString, QString> headers; // extra request headers
    bool enabled = true;
    bool persist = true; // store fetched items in feed_items (offline cache + history)
    int sort_order = 0;
};

/// One parsed record from a feed.
struct FeedItem {
    QString id; // stable: hash(feedId + (link|title))
    QString title;
    QString summary;
    QString link;
    QString source;
    qint64 sort_ts = 0; // epoch secs; 0 = unknown
    QString time;       // display string
    QHash<QString, QString> fields; // mapped extra columns (manual mode)
};

// String <-> enum helpers (stable storage tokens).
inline QString to_token(ParseMode m) { return m == ParseMode::Manual ? "manual" : "auto"; }
inline ParseMode parse_mode_from(const QString& s) { return s == "manual" ? ParseMode::Manual : ParseMode::Auto; }

inline QString to_token(DisplayMode m) { return m == DisplayMode::Table ? "table" : "cards"; }
inline DisplayMode display_mode_from(const QString& s) { return s == "table" ? DisplayMode::Table : DisplayMode::Cards; }

inline QString to_token(FeedFormat f) {
    switch (f) {
        case FeedFormat::Rss: return "rss";
        case FeedFormat::Atom: return "atom";
        case FeedFormat::Xml: return "xml";
        case FeedFormat::Json: return "json";
        case FeedFormat::Csv: return "csv";
        case FeedFormat::Html: return "html";
        default: return "auto";
    }
}
inline FeedFormat feed_format_from(const QString& s) {
    if (s == "rss") return FeedFormat::Rss;
    if (s == "atom") return FeedFormat::Atom;
    if (s == "xml") return FeedFormat::Xml;
    if (s == "json") return FeedFormat::Json;
    if (s == "csv") return FeedFormat::Csv;
    if (s == "html") return FeedFormat::Html;
    return FeedFormat::Auto;
}

inline QJsonObject headers_to_json(const QHash<QString, QString>& h) {
    QJsonObject o;
    for (auto it = h.begin(); it != h.end(); ++it)
        o[it.key()] = it.value();
    return o;
}
inline QHash<QString, QString> headers_from_json(const QJsonObject& o) {
    QHash<QString, QString> h;
    for (auto it = o.begin(); it != o.end(); ++it)
        h.insert(it.key(), it.value().toString());
    return h;
}

} // namespace fincept::feeds
