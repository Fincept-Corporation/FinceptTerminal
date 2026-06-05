#include "services/feeds/FeedParseUtil.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QTime>

namespace fincept::feeds {

QString strip_html(const QString& in, int max_chars) {
    QString s = in;
    static const QRegularExpression tag("<[^>]*>");
    s.remove(tag);
    s.replace("&amp;", "&")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&quot;", "\"")
        .replace("&#39;", "'")
        .replace("&nbsp;", " ");
    static const QRegularExpression ws("\\s+");
    s = s.replace(ws, " ").trimmed();
    if (max_chars > 0 && s.size() > max_chars)
        s = s.left(max_chars).trimmed() + QStringLiteral("…");
    return s;
}

namespace {

// Common timezone abbreviations -> fixed UTC offset (RFC822 style).
const QHash<QString, QString>& zone_offsets() {
    static const QHash<QString, QString> z = {
        {"GMT", "+0000"}, {"UTC", "+0000"}, {"UT", "+0000"},   {"Z", "+0000"},   {"EST", "-0500"},
        {"EDT", "-0400"}, {"CST", "-0600"}, {"CDT", "-0500"},  {"MST", "-0700"}, {"MDT", "-0600"},
        {"PST", "-0800"}, {"PDT", "-0700"}, {"IST", "+0530"},  {"CET", "+0100"}, {"CEST", "+0200"},
        {"BST", "+0100"}, {"JST", "+0900"}, {"AEST", "+1000"}, {"AEDT", "+1100"},{"SGT", "+0800"},
        {"HKT", "+0800"}, {"NZST", "+1200"},
    };
    return z;
}

// Explicit formats tried with the C locale, so text month/day names parse
// regardless of the user's system locale.
const QStringList& fallback_formats() {
    static const QStringList f = {
        "ddd, dd MMM yyyy HH:mm:ss", "ddd, d MMM yyyy HH:mm:ss", "dd MMM yyyy HH:mm:ss",
        "d MMM yyyy HH:mm:ss",       "ddd, dd MMM yyyy HH:mm",   "dd MMM yyyy HH:mm",
        // Text-month with dash / slash / dot separators (e.g. "05-Jun-2026 14:35:36").
        "dd-MMM-yyyy HH:mm:ss",      "d-MMM-yyyy HH:mm:ss",      "dd-MMM-yyyy HH:mm",
        "dd-MMM-yyyy",               "dd/MMM/yyyy HH:mm:ss",     "dd/MMM/yyyy HH:mm",
        "dd/MMM/yyyy",               "dd.MMM.yyyy HH:mm:ss",     "dd.MMM.yyyy",
        "MMM-dd-yyyy HH:mm:ss",      "MMM dd yyyy HH:mm:ss",
        "yyyy-MM-dd HH:mm:ss",       "yyyy-MM-dd'T'HH:mm:ss",    "yyyy-MM-dd HH:mm",
        "yyyy/MM/dd HH:mm:ss",       "yyyy/MM/dd HH:mm",         "MM/dd/yyyy HH:mm:ss",
        "MM/dd/yyyy hh:mm AP",       "MM/dd/yyyy",               "dd-MM-yyyy HH:mm:ss",
        "dd-MM-yyyy HH:mm",          "dd-MM-yyyy",               "dd.MM.yyyy HH:mm:ss",
        "dd.MM.yyyy",                "MMMM d, yyyy h:mm AP",      "MMMM d, yyyy",
        "MMM d, yyyy",               "ddd MMM d HH:mm:ss yyyy",  "yyyyMMddHHmmss",
        "yyyyMMdd",                  "yyyy-MM-dd",               "dd MMM yyyy",
    };
    return f;
}

// Pure 10-13 digit integer -> unix epoch (seconds, or ms downscaled). -1 otherwise.
qint64 try_epoch(const QString& t) {
    static const QRegularExpression re("^\\d{10,13}$");
    if (!re.match(t).hasMatch())
        return -1;
    bool ok = false;
    qint64 v = t.toLongLong(&ok);
    if (!ok)
        return -1;
    if (t.size() >= 13)
        v /= 1000; // milliseconds -> seconds
    return v;
}

} // namespace

qint64 parse_feed_datetime(const QString& text_in, QString& display, const QString& fmt) {
    const QString text = text_in.trimmed();
    if (text.isEmpty()) {
        display.clear();
        return 0;
    }

    auto finish = [&](const QDateTime& d) -> qint64 {
        display = d.toLocalTime().toString("MMM dd, yyyy HH:mm");
        return d.toSecsSinceEpoch();
    };

    QDateTime dt;

    // 1. Explicit user-provided format (locale-independent).
    if (!fmt.isEmpty()) {
        dt = QLocale::c().toDateTime(text, fmt);
        if (dt.isValid())
            return finish(dt);
    }

    // 2. Unix epoch (seconds or milliseconds).
    const qint64 ep = try_epoch(text);
    if (ep >= 0)
        return finish(QDateTime::fromSecsSinceEpoch(ep));

    // 3. RFC 2822 (RSS) and ISO 8601 (Atom) — locale-independent, keep the offset.
    dt = QDateTime::fromString(text, Qt::RFC2822Date);
    if (dt.isValid())
        return finish(dt);
    dt = QDateTime::fromString(text, Qt::ISODate);
    if (dt.isValid())
        return finish(dt);
    dt = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (dt.isValid())
        return finish(dt);

    // 4. Named timezone -> numeric offset, then retry RFC2822 (preserves offset).
    {
        static const QRegularExpression tzRe("\\s+([A-Za-z]{2,5})$");
        const auto m = tzRe.match(text);
        if (m.hasMatch() && zone_offsets().contains(m.captured(1).toUpper())) {
            const QString repl = text.left(m.capturedStart(0)).trimmed() + " " + zone_offsets().value(m.captured(1).toUpper());
            dt = QDateTime::fromString(repl, Qt::RFC2822Date);
            if (dt.isValid())
                return finish(dt);
        }
    }

    // 5. Strip timezone cruft and try the explicit-format battery (C locale).
    QString core = text;
    core.remove(QRegularExpression("\\s*\\([^)]*\\)\\s*$"));            // "(UTC)"
    core.remove(QRegularExpression("\\s*(Z|[+-]\\d{2}:?\\d{2})\\s*$")); // trailing offset / Z
    {                                                                  // trailing known zone abbrev (not AM/PM)
        static const QRegularExpression zr("\\s+([A-Za-z]{2,5})$");
        const auto m = zr.match(core);
        if (m.hasMatch() && zone_offsets().contains(m.captured(1).toUpper()))
            core = core.left(m.capturedStart(0)).trimmed();
    }
    core = core.trimmed();
    for (const QString& f : fallback_formats()) {
        dt = QLocale::c().toDateTime(core, f);
        if (dt.isValid())
            return finish(dt);
    }

    // 6. Last resort: pull a leading ISO date out (date-only, local midnight).
    static const QRegularExpression isoLead("(\\d{4})-(\\d{2})-(\\d{2})");
    const auto im = isoLead.match(text);
    if (im.hasMatch()) {
        const QDate d(im.captured(1).toInt(), im.captured(2).toInt(), im.captured(3).toInt());
        if (d.isValid())
            return finish(QDateTime(d, QTime(0, 0)));
    }

    display = text.left(22);
    return 0;
}

bool looks_like_html(const QByteArray& body) {
    const QByteArray head = body.trimmed().left(64).toLower();
    return head.contains("<html") || head.contains("<!doctype html");
}

FeedFormat sniff_format(const QByteArray& body) {
    const QByteArray t = body.trimmed();
    if (t.isEmpty())
        return FeedFormat::Auto;
    const char c = t.front();
    if (c == '{' || c == '[')
        return FeedFormat::Json;
    if (c == '<') {
        const QByteArray low = t.left(512).toLower();
        if (low.contains("<feed"))
            return FeedFormat::Atom;
        if (low.contains("<rss") || low.contains("<rdf"))
            return FeedFormat::Rss;
        if (looks_like_html(body))
            return FeedFormat::Html;
        return FeedFormat::Xml;
    }
    const qsizetype nl = t.indexOf('\n');
    const QByteArray firstLine = nl >= 0 ? t.left(nl) : t;
    if (firstLine.contains(',') || firstLine.contains('\t'))
        return FeedFormat::Csv;
    return FeedFormat::Auto;
}

} // namespace fincept::feeds
