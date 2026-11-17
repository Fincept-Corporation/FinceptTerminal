#pragma once
// src/services/news/NewsFeedParse.h
//
// Pure, header-only feed reading for the news service: RSS 0.9x/2.0, RSS 1.0
// (RDF) and Atom parsing, publication-time parsing and the time policy, the
// canonical newest-first order and the per-source burst cap. Qt Core only (no
// network, no widgets, no database) so it can be unit-tested on its own
// (tests/tst_news_feed_parse.cpp) and run on worker threads.
//
// ── Time policy (one place, used by every consumer) ────────────────────────
//  * Every timestamp is UTC unix seconds. A text without a zone is read in the
//    feed's declared wall-clock offset (`naive_offset_min`, 0 = UTC).
//  * An item's time is its own publication time: RSS pubDate, Atom published,
//    dc:date / dcterms:issued; Atom updated / dcterms:modified only when the
//    item carries no publication time.
//  * A date more than kMaxFeedClockErrorSec in the future is garbage and is
//    treated as missing. When a feed's newest item is still ahead of the clock
//    by more than kMaxFutureSkewSec the feed is mislabelling its zone (local
//    wall-clock time sent as GMT): the whole feed is moved back by that much,
//    rounded up to 15 minutes, so its items keep their order among themselves
//    and never sort above stories that really are newer.
//  * An undated item inside a dated feed takes the time of its nearest dated
//    neighbour in feed order (feeds list newest first), never "now".
//  * A feed with no item dates at all: its items are UNDATED (item_time()).
//    Their time is when we FIRST SAW them (remembered across refreshes, capped
//    by the feed's own channel date) — used only to age them out of the time
//    window, never as a publication time. In the canonical order every undated
//    item comes after every dated one (newer_first), and the screens label it
//    "undated" instead of an age: on a fresh profile "first seen" is "now", and
//    an item stamped "now" used to rank above stories published minutes ago.
//  * Ties (equal times) are broken by article id, so the order is the same on
//    every refresh, in every view, whatever order the feeds answered in.

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringDecoder>
#include <QStringList>
#include <QTime>
#include <QTimeZone>
#include <QVector>
#include <QXmlStreamReader>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace fincept::services::news_feed {

/// Clock skew between a publisher and us that is simply tolerated.
inline constexpr qint64 kMaxFutureSkewSec = 600;
/// Further ahead than this a date is not a zone mistake but garbage.
inline constexpr qint64 kMaxFeedClockErrorSec = 15 * 3600;
/// Oldest plausible publication date (rejects 1970 / year-0 defaults).
inline constexpr qint64 kMinPlausibleTs = 946684800; // 2000-01-01

// ── Small text helpers ──────────────────────────────────────────────────────

/// Named HTML entities that feeds put in their XML or inside CDATA.
inline const QHash<QString, char32_t>& html_entities() {
    static const QHash<QString, char32_t> e = {
        {"nbsp", 160},   {"amp", 38},     {"lt", 60},      {"gt", 62},      {"quot", 34},    {"apos", 39},
        {"ndash", 8211}, {"mdash", 8212}, {"lsquo", 8216}, {"rsquo", 8217}, {"ldquo", 8220}, {"rdquo", 8221},
        {"sbquo", 8218}, {"bdquo", 8222}, {"hellip", 8230}, {"bull", 8226}, {"middot", 183}, {"laquo", 171},
        {"raquo", 187},  {"lsaquo", 8249}, {"rsaquo", 8250}, {"prime", 8242}, {"Prime", 8243}, {"dagger", 8224},
        {"copy", 169},   {"reg", 174},    {"trade", 8482}, {"euro", 8364},  {"pound", 163},  {"yen", 165},
        {"cent", 162},   {"deg", 176},    {"times", 215},  {"divide", 247}, {"plusmn", 177}, {"sect", 167},
        {"para", 182},   {"frac12", 189}, {"frac14", 188}, {"frac34", 190}, {"iexcl", 161},  {"iquest", 191},
        {"shy", 173},    {"ensp", 8194},  {"emsp", 8195},  {"thinsp", 8201}, {"zwnj", 8204}, {"zwj", 8205},
        {"lrm", 8206},   {"rlm", 8207},   {"aacute", 225}, {"Aacute", 193}, {"agrave", 224}, {"Agrave", 192},
        {"acirc", 226},  {"Acirc", 194},  {"atilde", 227}, {"Atilde", 195}, {"auml", 228},   {"Auml", 196},
        {"aring", 229},  {"Aring", 197},  {"aelig", 230},  {"AElig", 198},  {"ccedil", 231}, {"Ccedil", 199},
        {"eacute", 233}, {"Eacute", 201}, {"egrave", 232}, {"Egrave", 200}, {"ecirc", 234},  {"Ecirc", 202},
        {"euml", 235},   {"Euml", 203},   {"iacute", 237}, {"Iacute", 205}, {"igrave", 236}, {"Igrave", 204},
        {"icirc", 238},  {"Icirc", 206},  {"iuml", 239},   {"Iuml", 207},   {"ntilde", 241}, {"Ntilde", 209},
        {"oacute", 243}, {"Oacute", 211}, {"ograve", 242}, {"Ograve", 210}, {"ocirc", 244},  {"Ocirc", 212},
        {"otilde", 245}, {"Otilde", 213}, {"ouml", 246},   {"Ouml", 214},   {"oslash", 248}, {"Oslash", 216},
        {"uacute", 250}, {"Uacute", 218}, {"ugrave", 249}, {"Ugrave", 217}, {"ucirc", 251},  {"Ucirc", 219},
        {"uuml", 252},   {"Uuml", 220},   {"yacute", 253}, {"Yacute", 221}, {"yuml", 255},   {"szlig", 223},
        {"scaron", 353}, {"Scaron", 352}, {"zcaron", 382}, {"Zcaron", 381}, {"oelig", 339},  {"OElig", 338},
    };
    return e;
}

/// Decode HTML character references (&#8217; &#x2019; &rsquo; &amp; ...) left
/// in text. Unknown names are kept as written.
inline QString decode_entities(const QString& in) {
    if (!in.contains(QLatin1Char('&')))
        return in;
    QString out;
    out.reserve(in.size());
    const int n = static_cast<int>(in.size());
    for (int i = 0; i < n; ++i) {
        const QChar c = in.at(i);
        if (c != QLatin1Char('&')) {
            out.append(c);
            continue;
        }
        const int semi = static_cast<int>(in.indexOf(QLatin1Char(';'), i + 1));
        if (semi < 0 || semi - i > 12) {
            out.append(c);
            continue;
        }
        const QString name = in.mid(i + 1, semi - i - 1);
        char32_t cp = 0;
        if (name.startsWith(QLatin1Char('#'))) {
            bool ok = false;
            const uint v = (name.size() > 1 && (name.at(1) == QLatin1Char('x') || name.at(1) == QLatin1Char('X')))
                               ? name.mid(2).toUInt(&ok, 16)
                               : name.mid(1).toUInt(&ok, 10);
            if (ok && v > 0 && v <= 0x10FFFF && (v < 0xD800 || v > 0xDFFF))
                cp = v;
        } else {
            cp = html_entities().value(name, 0);
        }
        if (cp == 0) {
            out.append(c);
            continue;
        }
        if (cp == 160)
            cp = U' ';
        out.append(QString::fromUcs4(&cp, 1));
        i = semi;
    }
    return out;
}

/// Plain text of a feed field: HTML tags removed, character references
/// decoded, whitespace collapsed. A '<' that does not open a tag is kept.
/// Markup that was escaped twice (an Atom type="html" title written as
/// "&amp;lt;b&amp;gt;") only appears after decoding, so tags are removed
/// again then.
inline QString clean_text(const QString& in) {
    static const QRegularExpression tag_re(QStringLiteral("<(?:/?[A-Za-z][^>]*|!--.*?--)>"),
                                           QRegularExpression::DotMatchesEverythingOption);
    QString s = in;
    if (s.contains(QLatin1Char('<')))
        s.remove(tag_re);
    s = decode_entities(s);
    if (s.contains(QLatin1Char('<')))
        s.remove(tag_re);
    return s.simplified();
}

// ── Publication time ────────────────────────────────────────────────────────

/// UTC offset in minutes of a zone abbreviation; false when unknown. IST is
/// India (the feeds that use it), BST British Summer Time.
inline bool zone_offset(const QString& abbrev, int& minutes) {
    static const QHash<QString, int> z = {
        {"GMT", 0},     {"UT", 0},      {"UTC", 0},     {"Z", 0},       {"WET", 0},     {"WEST", 60},
        {"BST", 60},    {"IST", 330},   {"CET", 60},    {"CEST", 120},  {"MET", 60},    {"MEST", 120},
        {"EET", 120},   {"EEST", 180},  {"MSK", 180},   {"TRT", 180},   {"GST", 240},   {"PKT", 300},
        {"NPT", 345},   {"BDT", 360},   {"ICT", 420},   {"WIB", 420},   {"SGT", 480},   {"HKT", 480},
        {"PHT", 480},   {"MYT", 480},   {"AWST", 480},  {"CST8", 480},  {"JST", 540},   {"KST", 540},
        {"ACST", 570},  {"ACDT", 630},  {"AEST", 600},  {"AEDT", 660},  {"NZST", 720},  {"NZDT", 780},
        {"EST", -300},  {"EDT", -240},  {"CST", -360},  {"CDT", -300},  {"MST", -420},  {"MDT", -360},
        {"PST", -480},  {"PDT", -420},  {"AKST", -540}, {"AKDT", -480}, {"HST", -600},  {"AST", -240},
        {"ADT", -180},  {"NST", -210},  {"NDT", -150},  {"BRT", -180},  {"ART", -180},  {"CLT", -240},
        {"COT", -300},  {"PET", -300},  {"SAST", 120},  {"WAT", 60},    {"CAT", 120},   {"EAT", 180},
    };
    const auto it = z.constFind(abbrev.toUpper());
    if (it == z.constEnd())
        return false;
    minutes = it.value();
    return true;
}

/// "+0530", "+05:30", "-05", "+5:30" (sign included) -> minutes; false if not one.
inline bool numeric_offset(QStringView t, int& minutes) {
    if (t.size() < 2 || (t.front() != QLatin1Char('+') && t.front() != QLatin1Char('-')))
        return false;
    const bool neg = t.front() == QLatin1Char('-');
    QString digits;
    int colon = -1;
    for (int i = 1; i < t.size(); ++i) {
        if (t.at(i) == QLatin1Char(':')) {
            if (colon >= 0)
                return false;
            colon = static_cast<int>(digits.size());
            continue;
        }
        if (!t.at(i).isDigit())
            return false;
        digits.append(t.at(i));
    }
    int h = 0, m = 0;
    if (colon >= 0) {
        h = digits.left(colon).toInt();
        m = digits.mid(colon).toInt();
        if (colon == 0 || digits.size() - colon != 2)
            return false;
    } else if (digits.size() == 4) {
        h = digits.left(2).toInt();
        m = digits.mid(2).toInt();
    } else if (digits.size() <= 2 && !digits.isEmpty()) {
        h = digits.toInt();
    } else {
        return false;
    }
    if (h > 14 || m > 59)
        return false;
    minutes = (neg ? -1 : 1) * (h * 60 + m);
    return true;
}

namespace detail {

inline int month_from_name(const QString& t) {
    static const char* const names[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                        "jul", "aug", "sep", "oct", "nov", "dec"};
    if (t.size() < 3)
        return 0;
    const QString low = t.toLower();
    for (int i = 0; i < 12; ++i)
        if (low.startsWith(QLatin1String(names[i])))
            return i + 1;
    return 0;
}

inline bool is_day_name(const QString& t) {
    static const char* const names[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    if (t.size() < 3)
        return false;
    const QString low = t.toLower();
    for (const char* n : names)
        if (low.startsWith(QLatin1String(n)))
            return true;
    return false;
}

/// Build UTC seconds from wall-clock fields and the zone offset; 0 if invalid.
inline qint64 to_utc(int y, int mo, int d, int h, int mi, int s, int offset_min) {
    const QDate date(y, mo, d);
    const QTime time(h, mi, std::min(s, 59));
    if (!date.isValid() || !time.isValid() || y < 1990 || y > 2100)
        return 0;
    const qint64 wall = QDateTime(date, time, QTimeZone::UTC).toSecsSinceEpoch();
    return wall - static_cast<qint64>(offset_min) * 60;
}

/// "07:22", "07:22:41", "07:22:41.123", "7:22pm" -> fields. AM/PM may be
/// attached; returns the trailing remainder (e.g. "pm") through `rest`.
inline bool parse_clock(const QString& t, int& h, int& mi, int& s, QString& rest) {
    static const QRegularExpression re(QStringLiteral("^(\\d{1,2}):(\\d{2})(?::(\\d{2})(?:[.,]\\d+)?)?([A-Za-z.]*)$"));
    const auto m = re.match(t);
    if (!m.hasMatch())
        return false;
    h = m.captured(1).toInt();
    mi = m.captured(2).toInt();
    s = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
    rest = m.captured(4);
    return true;
}

inline bool apply_ampm(const QString& tok, int& h) {
    QString t = tok.toLower();
    t.remove(QLatin1Char('.'));
    if (t != QLatin1String("am") && t != QLatin1String("pm"))
        return false;
    if (h < 1 || h > 12)
        return true; // "13:00 PM" — keep the 24 h reading
    if (t == QLatin1String("am"))
        h = (h == 12) ? 0 : h;
    else
        h = (h == 12) ? 12 : h + 12;
    return true;
}

} // namespace detail

/// Parse a feed timestamp to UTC unix seconds; 0 when it is not a date.
///
/// Handles RFC 822 / 1123 / 2822 ("Mon, 16 Nov 2026 07:22:41 GMT", numeric
/// zones with or without a colon, named zones, two-digit years, missing
/// seconds or day name, "(UTC)" comments), ISO 8601 / Atom / dc:date
/// ("2026-11-16T07:22:41.123+05:30", "Z", basic offsets, space separator,
/// date only), "Nov 16, 2026 7:22 PM EST", ctime ("Mon Nov 16 07:22:41 UTC
/// 2026") and unix epochs (seconds or milliseconds). A text that carries no
/// zone is read at `naive_offset_min` from UTC. Never consults the local zone.
inline qint64 parse_time(const QString& text_in, int naive_offset_min = 0) {
    QString text = text_in.trimmed();
    if (text.isEmpty())
        return 0;
    // Trailing comment: "... +0000 (UTC)", "(Coordinated Universal Time)".
    static const QRegularExpression comment_re(QStringLiteral("\\s*\\([^)]*\\)\\s*$"));
    text.remove(comment_re);
    if (text.isEmpty())
        return 0;

    // Unix epoch, seconds or milliseconds.
    static const QRegularExpression epoch_re(QStringLiteral("^\\d{10}(\\d{3})?$"));
    if (epoch_re.match(text).hasMatch()) {
        qint64 v = text.toLongLong();
        if (text.size() == 13)
            v /= 1000;
        return v >= kMinPlausibleTs ? v : 0;
    }

    // ISO 8601 / RFC 3339 (Atom, dc:date, JSON-ish feeds).
    static const QRegularExpression iso_re(QStringLiteral(
        "^(\\d{4})[-/](\\d{1,2})[-/](\\d{1,2})"
        "(?:[Tt ]+(\\d{1,2}):(\\d{2})(?::(\\d{2})(?:[.,]\\d+)?)?)?"
        "\\s*(Z|z|UTC|GMT|[+-]\\d{1,2}(?::?\\d{2})?)?$"));
    if (const auto m = iso_re.match(text); m.hasMatch()) {
        int offset = naive_offset_min;
        const QString zone = m.captured(7);
        if (!zone.isEmpty()) {
            if (zone.front().isLetter())
                offset = 0;
            else if (!numeric_offset(zone, offset))
                return 0;
        }
        const bool has_time = !m.captured(4).isEmpty();
        return detail::to_utc(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt(),
                              has_time ? m.captured(4).toInt() : 0, has_time ? m.captured(5).toInt() : 0,
                              m.captured(6).isEmpty() ? 0 : m.captured(6).toInt(), has_time ? offset : 0);
    }

    // Token form: RFC 822 family, "Nov 16, 2026 7:22 PM", ctime.
    static const QRegularExpression split_re(QStringLiteral("[\\s,]+"));
    QStringList tokens = text.split(split_re, Qt::SkipEmptyParts);
    int day = 0, month = 0, year = 0, h = 0, mi = 0, s = 0;
    bool have_time = false, have_zone = false;
    int offset = naive_offset_min;
    QVector<int> numbers; // bare integers in order (day / year candidates)
    for (int i = 0; i < tokens.size(); ++i) {
        QString t = tokens.at(i);
        if (t == QLatin1String("-") || t == QLatin1String("at") || t == QLatin1String("|"))
            continue;
        if (t.endsWith(QLatin1Char('.')) && t.size() > 1 && t.at(0).isLetter())
            t.chop(1);
        QString rest;
        if (!have_time && detail::parse_clock(t, h, mi, s, rest)) {
            have_time = true;
            if (!rest.isEmpty())
                detail::apply_ampm(rest, h);
            else if (i + 1 < tokens.size() && detail::apply_ampm(tokens.at(i + 1), h))
                ++i;
            continue;
        }
        int off = 0;
        if (t.front() == QLatin1Char('+') || t.front() == QLatin1Char('-')) {
            if (!have_zone && numeric_offset(t, off)) {
                offset = off;
                have_zone = true;
            }
            continue;
        }
        if (t.at(0).isLetter()) {
            // "GMT+05:30", "UTC-4"
            static const QRegularExpression gmt_off_re(QStringLiteral("^(?:GMT|UTC|UT)([+-].+)$"),
                                                       QRegularExpression::CaseInsensitiveOption);
            if (const auto gm = gmt_off_re.match(t); gm.hasMatch() && numeric_offset(gm.captured(1), off)) {
                offset = off;
                have_zone = true;
                continue;
            }
            if (month == 0) {
                const int mo = detail::month_from_name(t);
                if (mo > 0 && !detail::is_day_name(t)) {
                    month = mo;
                    continue;
                }
            }
            if (detail::is_day_name(t))
                continue;
            if (!have_zone && zone_offset(t, off)) {
                offset = off;
                have_zone = true;
                continue;
            }
            continue; // unknown word ("EDT"-like zones we do not know read as naive)
        }
        // "16-Nov-2026" / "16/Nov/2026"
        static const QRegularExpression dmy_re(QStringLiteral("^(\\d{1,2})[-/.]([A-Za-z]{3,9})[-/.](\\d{2,4})$"));
        if (const auto dm = dmy_re.match(t); dm.hasMatch()) {
            day = dm.captured(1).toInt();
            month = detail::month_from_name(dm.captured(2));
            year = dm.captured(3).toInt();
            continue;
        }
        bool ok = false;
        const int v = t.toInt(&ok);
        if (ok)
            numbers.append(v);
        else
            return 0; // something we do not understand — refuse rather than guess
    }
    if (month == 0)
        return 0;
    for (int v : std::as_const(numbers)) {
        if (day == 0 && v >= 1 && v <= 31 && !(year == 0 && v > 31))
            day = v;
        else if (year == 0)
            year = v;
    }
    if (day == 0 || year == 0)
        return 0;
    if (year < 100)
        year += year >= 70 ? 1900 : 2000;
    return detail::to_utc(year, month, day, have_time ? h : 0, have_time ? mi : 0, have_time ? s : 0,
                          have_time ? offset : 0);
}

// ── Feed parsing ────────────────────────────────────────────────────────────

struct Item {
    QString title;
    QString link;
    QString summary;
    qint64 published = 0; ///< pubDate / published / dc:date / issued
    qint64 updated = 0;   ///< Atom updated / dcterms:modified
    qint64 ts = 0;        ///< chosen publication time (after normalize_times); 0 = no date at all
    bool ts_estimated = false; ///< ts was inferred (neighbour), not stated by the item
};

struct Feed {
    QVector<Item> items;
    qint64 feed_ts = 0;       ///< channel / feed level date (pubDate, updated, lastBuildDate)
    bool well_formed = true;  ///< false: the XML had an error (items before it are kept)
    QString error;            ///< reader error text when !well_formed
    QString declared_encoding;
};

namespace detail {

/// Bytes after a BOM and any whitespace / junk before the first '<'.
inline QByteArray skip_preamble(const QByteArray& in) {
    qsizetype i = 0;
    if (in.startsWith("\xEF\xBB\xBF"))
        i = 3;
    while (i < in.size() && in.at(i) != '<')
        ++i;
    return i == 0 ? in : in.mid(i);
}

inline QString declared_encoding(const QByteArray& xml) {
    if (!xml.startsWith("<?xml"))
        return {};
    const qsizetype end = xml.indexOf("?>");
    const QByteArray decl = xml.left(end > 0 ? end : 200);
    static const QRegularExpression enc_re(QStringLiteral("encoding\\s*=\\s*[\"']([^\"']+)[\"']"));
    const auto m = enc_re.match(QString::fromLatin1(decl));
    return m.hasMatch() ? m.captured(1).trimmed().toLower() : QString();
}

inline bool valid_utf8_with_multibyte(const QByteArray& b) {
    bool multibyte = false;
    const auto* p = reinterpret_cast<const unsigned char*>(b.constData());
    const qsizetype n = b.size();
    for (qsizetype i = 0; i < n;) {
        const unsigned char c = p[i];
        if (c < 0x80) {
            ++i;
            continue;
        }
        int len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
        if (len == 0 || i + len > n || c == 0xC0 || c == 0xC1)
            return false;
        for (int k = 1; k < len; ++k)
            if ((p[i + k] & 0xC0) != 0x80)
                return false;
        multibyte = true;
        i += len;
    }
    return multibyte;
}

/// windows-1252, the real encoding behind most feeds labelled ISO-8859-1.
inline QString decode_cp1252(const QByteArray& b) {
    static const char16_t high[32] = {0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
                                      0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
                                      0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                                      0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178};
    QString out;
    out.resize(b.size());
    for (qsizetype i = 0; i < b.size(); ++i) {
        const auto c = static_cast<unsigned char>(b.at(i));
        out[i] = (c >= 0x80 && c < 0xA0) ? QChar(high[c - 0x80]) : QChar(c);
    }
    return out;
}

/// Replace the XML declaration's encoding with UTF-8 (the text was re-encoded).
inline QByteArray as_utf8_document(const QString& text) {
    QByteArray out = text.toUtf8();
    if (out.startsWith("<?xml")) {
        const qsizetype end = out.indexOf("?>");
        if (end > 0) {
            static const QRegularExpression enc_re(QStringLiteral("encoding\\s*=\\s*[\"'][^\"']*[\"']"));
            QString decl = QString::fromLatin1(out.left(end));
            decl.replace(enc_re, QStringLiteral("encoding=\"UTF-8\""));
            out = decl.toLatin1() + out.mid(end);
        }
    }
    return out;
}

/// Bring the payload to UTF-8 bytes QXmlStreamReader always understands.
inline QByteArray normalise_encoding(const QByteArray& xml, const QString& enc) {
    if (enc.isEmpty() || enc == QLatin1String("utf-8") || enc == QLatin1String("utf8"))
        return xml;
    const bool single_byte = enc == QLatin1String("iso-8859-1") || enc == QLatin1String("latin1") ||
                             enc == QLatin1String("latin-1") || enc == QLatin1String("iso-8859-15") ||
                             enc == QLatin1String("windows-1252") || enc == QLatin1String("cp1252") ||
                             enc == QLatin1String("us-ascii") || enc == QLatin1String("ascii");
    if (single_byte) {
        // Many feeds declare ISO-8859-1 but send UTF-8: trust the bytes.
        if (valid_utf8_with_multibyte(xml))
            return as_utf8_document(QString::fromUtf8(xml));
        return as_utf8_document(decode_cp1252(xml));
    }
    if (enc.startsWith(QLatin1String("utf-16")) || enc.startsWith(QLatin1String("utf-32")))
        return xml; // the reader detects these from the byte order itself
    QStringDecoder dec(enc.toLatin1().constData());
    if (!dec.isValid())
        return xml; // let the reader try (it errors cleanly on what it cannot decode)
    const QString text = dec.decode(xml);
    if (dec.hasError())
        return xml;
    return as_utf8_document(text);
}

/// Second chance for a payload the XML reader rejected: HTML entities that
/// XML does not define become numeric references, and a bare '&' (AT&T)
/// becomes &amp;. CDATA sections and comments are left alone.
inline QByteArray repair_entities(const QByteArray& xml) {
    const QString in = QString::fromUtf8(xml);
    QString out;
    out.reserve(in.size() + 64);
    const int n = static_cast<int>(in.size());
    static const QRegularExpression ref_re(QStringLiteral("^&(#\\d{1,7}|#[xX][0-9a-fA-F]{1,6}|[A-Za-z][A-Za-z0-9]{1,10});"));
    for (int i = 0; i < n; ++i) {
        if (in.at(i) == QLatin1Char('<')) {
            if (QStringView(in).mid(i, 9) == QLatin1String("<![CDATA[")) {
                const int end = static_cast<int>(in.indexOf(QLatin1String("]]>"), i));
                const int stop = end < 0 ? n : end + 3;
                out.append(QStringView(in).mid(i, stop - i));
                i = stop - 1;
                continue;
            }
            if (QStringView(in).mid(i, 4) == QLatin1String("<!--")) {
                const int end = static_cast<int>(in.indexOf(QLatin1String("-->"), i));
                const int stop = end < 0 ? n : end + 3;
                out.append(QStringView(in).mid(i, stop - i));
                i = stop - 1;
                continue;
            }
        }
        if (in.at(i) != QLatin1Char('&')) {
            out.append(in.at(i));
            continue;
        }
        const auto m = ref_re.match(in.mid(i, 16));
        if (!m.hasMatch()) {
            out.append(QLatin1String("&amp;"));
            continue;
        }
        const QString name = m.captured(1);
        static const QSet<QString> xml_names = {"amp", "lt", "gt", "quot", "apos"};
        if (name.startsWith(QLatin1Char('#')) || xml_names.contains(name)) {
            out.append(m.captured(0));
        } else if (const char32_t cp = html_entities().value(name, 0); cp != 0) {
            out.append(QStringLiteral("&#%1;").arg(static_cast<uint>(cp)));
        } else {
            out.append(QLatin1String("&amp;")).append(QStringView(m.captured(0)).mid(1));
        }
        i += static_cast<int>(m.capturedLength(0)) - 1;
    }
    return out.toUtf8();
}

inline const QString& ns_media() {
    static const QString s = QStringLiteral("http://search.yahoo.com/mrss/");
    return s;
}

/// One pass of the reader over `xml`.
inline Feed read_feed(const QByteArray& xml, int naive_offset_min) {
    Feed feed;
    QXmlStreamReader r(xml);
    r.setNamespaceProcessing(true);

    int depth = 0;
    int item_depth = -1; // depth of the open <item>/<entry>, -1 = none
    Item cur;
    QString cur_published_text, cur_updated_text;
    QString summary_primary, summary_content;
    QString guid_link;
    QString atom_alt_link;
    // Direct child of the item currently being read.
    QString child_name, child_ns;
    QString child_text;
    bool in_child = false;
    // Channel-level dates.
    qint64 channel_date = 0, build_date = 0;
    QString chan_name;
    QString chan_text;
    bool in_chan_date = false;

    while (!r.atEnd()) {
        const auto tok = r.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            ++depth;
            const QString name = r.name().toString();
            if (item_depth < 0 && (name == QLatin1String("item") || name == QLatin1String("entry"))) {
                item_depth = depth;
                cur = Item{};
                cur_published_text.clear();
                cur_updated_text.clear();
                summary_primary.clear();
                summary_content.clear();
                guid_link.clear();
                atom_alt_link.clear();
                continue;
            }
            if (item_depth >= 0 && depth == item_depth + 1) {
                in_child = true;
                child_name = name;
                child_ns = r.namespaceUri().toString();
                child_text.clear();
                if (name == QLatin1String("link")) {
                    const auto attrs = r.attributes();
                    const QString href = attrs.value(QLatin1String("href")).toString().trimmed();
                    const QString rel = attrs.value(QLatin1String("rel")).toString();
                    if (!href.isEmpty() && (rel.isEmpty() || rel == QLatin1String("alternate"))) {
                        const QString type = attrs.value(QLatin1String("type")).toString();
                        if (atom_alt_link.isEmpty() || type == QLatin1String("text/html"))
                            atom_alt_link = href;
                    }
                }
                continue;
            }
            if (item_depth < 0 && (name == QLatin1String("pubDate") || name == QLatin1String("lastBuildDate") ||
                                   name == QLatin1String("updated") || name == QLatin1String("date") ||
                                   name == QLatin1String("modified"))) {
                in_chan_date = true;
                chan_name = name;
                chan_text.clear();
            }
        } else if (tok == QXmlStreamReader::Characters) {
            if (in_child)
                child_text.append(r.text());
            else if (in_chan_date)
                chan_text.append(r.text());
        } else if (tok == QXmlStreamReader::EndElement) {
            if (item_depth >= 0 && depth == item_depth) {
                // </item> / </entry>
                cur.published = parse_time(cur_published_text, naive_offset_min);
                cur.updated = parse_time(cur_updated_text, naive_offset_min);
                cur.ts = cur.published > 0 ? cur.published : cur.updated;
                if (cur.link.isEmpty())
                    cur.link = !atom_alt_link.isEmpty() ? atom_alt_link : guid_link;
                cur.summary = !summary_primary.isEmpty() ? summary_primary : summary_content;
                if (!cur.title.isEmpty())
                    feed.items.append(cur);
                item_depth = -1;
                in_child = false;
            } else if (in_child && depth == item_depth + 1) {
                const QString text = child_text.trimmed();
                const QString& n = child_name;
                const bool media = child_ns == ns_media();
                if (n == QLatin1String("title") && !media) {
                    if (cur.title.isEmpty())
                        cur.title = clean_text(text);
                } else if (n == QLatin1String("link")) {
                    if (cur.link.isEmpty() && !text.isEmpty() && !child_ns.contains(QLatin1String("Atom"), Qt::CaseInsensitive))
                        cur.link = text;
                } else if (n == QLatin1String("guid") || n == QLatin1String("id")) {
                    if (guid_link.isEmpty() && text.startsWith(QLatin1String("http")))
                        guid_link = text;
                } else if ((n == QLatin1String("description") || n == QLatin1String("summary") ||
                            n == QLatin1String("abstract")) && !media) {
                    if (summary_primary.isEmpty())
                        summary_primary = text;
                } else if (n == QLatin1String("encoded") || (n == QLatin1String("content") && !media)) {
                    if (summary_content.isEmpty())
                        summary_content = text;
                } else if (n == QLatin1String("pubDate") || n == QLatin1String("published") ||
                           n == QLatin1String("issued") || n == QLatin1String("date") ||
                           n == QLatin1String("created")) {
                    if (cur_published_text.isEmpty() || parse_time(cur_published_text, naive_offset_min) == 0)
                        cur_published_text = text;
                } else if (n == QLatin1String("updated") || n == QLatin1String("modified")) {
                    if (cur_updated_text.isEmpty())
                        cur_updated_text = text;
                }
                in_child = false;
            } else if (in_chan_date && item_depth < 0) {
                const qint64 t = parse_time(chan_text.trimmed(), naive_offset_min);
                if (chan_name == QLatin1String("lastBuildDate")) {
                    if (build_date == 0)
                        build_date = t;
                } else if (channel_date == 0) {
                    channel_date = t;
                }
                in_chan_date = false;
            }
            --depth;
        }
    }
    if (r.hasError() && r.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        feed.well_formed = false;
        feed.error = QStringLiteral("line %1: %2").arg(r.lineNumber()).arg(r.errorString());
    } else if (r.hasError() && feed.items.isEmpty()) {
        feed.well_formed = false;
        feed.error = r.errorString();
    }
    feed.feed_ts = channel_date > 0 ? channel_date : build_date;
    return feed;
}

} // namespace detail

/// True when the payload is an HTML page (access-denied / captcha / error
/// page), not a feed. BOM and leading whitespace are skipped.
inline bool looks_like_html(const QByteArray& data) {
    const QByteArray head = detail::skip_preamble(data).left(256).toLower();
    return head.startsWith("<!doctype html") || head.startsWith("<html") ||
           (head.startsWith("<!doctype") && head.contains("html"));
}

/// Parse an RSS 0.9x / 2.0, RSS 1.0 (RDF) or Atom document. Tolerates a BOM or
/// junk before the first tag, ISO-8859-1 / windows-1252 (and UTF-8 mislabelled
/// as either), other declared encodings Qt can decode, CDATA, namespaced
/// elements, and — on a second pass — HTML entities and bare ampersands that
/// make the XML ill-formed. Item times are as stated; run normalize_times().
inline Feed parse_feed(const QByteArray& payload, int naive_offset_min = 0) {
    const QByteArray raw = detail::skip_preamble(payload);
    const QString enc = detail::declared_encoding(raw);
    const QByteArray xml = detail::normalise_encoding(raw, enc);
    Feed feed = detail::read_feed(xml, naive_offset_min);
    if (!feed.well_formed) {
        Feed repaired = detail::read_feed(detail::repair_entities(xml), naive_offset_min);
        if (repaired.items.size() > feed.items.size() || (repaired.well_formed && !feed.items.isEmpty() &&
                                                          repaired.items.size() == feed.items.size()))
            feed = std::move(repaired);
    }
    feed.declared_encoding = enc;
    return feed;
}

/// Apply the time policy (see the top of this file) to one feed's items.
/// Returns the shift applied for a feed running ahead of the clock (0 = none).
inline qint64 normalize_times(QVector<Item>& items, qint64 feed_ts, qint64 now) {
    // 1. Garbage: implausibly old, or so far ahead it cannot be a zone mistake.
    for (auto& it : items) {
        if (it.ts > 0 && (it.ts < kMinPlausibleTs || it.ts > now + kMaxFeedClockErrorSec))
            it.ts = 0;
    }
    // 2. A feed whose newest item is still in the future mislabels its zone.
    qint64 newest = 0;
    for (const auto& it : items)
        newest = std::max(newest, it.ts);
    qint64 shift = 0;
    if (newest > now + kMaxFutureSkewSec) {
        const qint64 ahead = newest - now;
        shift = ((ahead + 899) / 900) * 900;
        for (auto& it : items)
            if (it.ts > 0)
                it.ts -= shift;
    }
    // 3. Undated items in a dated feed: nearest dated neighbour in feed order
    //    (the previous one — feeds list newest first — else the next one).
    bool any_dated = false;
    for (const auto& it : items)
        any_dated = any_dated || it.ts > 0;
    if (any_dated) {
        qint64 prev = 0;
        for (auto& it : items) {
            if (it.ts > 0) {
                prev = it.ts;
            } else if (prev > 0) {
                it.ts = prev;
                it.ts_estimated = true;
            }
        }
        qint64 next = 0;
        for (auto i = items.size() - 1; i >= 0; --i) {
            auto& it = items[i];
            if (it.ts > 0 && !it.ts_estimated) {
                next = it.ts;
            } else if (it.ts == 0 && next > 0) {
                it.ts = next;
                it.ts_estimated = true;
            }
        }
    }
    // 4. A feed with no item dates at all stays 0: the caller uses first-seen
    //    time, capped by the feed date (resolve_first_seen()).
    (void)feed_ts;
    return shift;
}

/// Time for an item of a feed that dates none of its items: when we first saw
/// it, but never later than the feed's own (past) channel date.
inline qint64 resolve_first_seen(qint64 first_seen, qint64 feed_ts, qint64 now) {
    qint64 t = first_seen > 0 ? first_seen : now;
    if (feed_ts >= kMinPlausibleTs && feed_ts <= now + kMaxFutureSkewSec)
        t = std::min(t, feed_ts);
    return std::min(t, now);
}

/// Where one parsed item goes in time: its own (normalised) date, or — for an
/// item of a feed that dates nothing — the first-seen time, flagged undated.
struct ItemTime {
    qint64 ts = 0;
    bool undated = false;
};
inline ItemTime item_time(const Item& item, qint64 remembered_first_seen, qint64 feed_ts, qint64 now) {
    if (item.ts > 0)
        return {item.ts, false};
    return {resolve_first_seen(remembered_first_seen, feed_ts, now), true};
}

/// Keep the `max_items` newest items of one feed (undated ones last, in feed
/// order). Bounds what a firehose feed (100-200 items) contributes per fetch.
inline void keep_newest(QVector<Item>& items, int max_items) {
    if (items.size() <= max_items)
        return;
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if ((a.ts > 0) != (b.ts > 0))
            return a.ts > 0;
        return a.ts > b.ts;
    });
    items.resize(max_items);
}

// ── Ordering (shared by every news consumer) ────────────────────────────────

/// True for an article whose feed gave no publication time (a type without an
/// `undated` member is always dated).
template <typename A>
inline bool is_undated(const A& a) {
    if constexpr (requires(const A& x) { x.undated; }) {
        return a.undated;
    } else {
        (void)a;
        return false;
    }
}

/// Canonical order: dated articles before undated ones (an undated time is only
/// when we first saw it); then newest publication time first; equal times by
/// id, so the order is identical on every refresh and in every view.
template <typename A>
inline bool newer_first(const A& a, const A& b) {
    const bool ua = is_undated(a);
    if (ua != is_undated(b))
        return !ua;
    if (a.sort_ts != b.sort_ts)
        return a.sort_ts > b.sort_ts;
    return a.id < b.id;
}

template <typename A>
inline void sort_newest_first(QVector<A>& v) {
    std::sort(v.begin(), v.end(), newer_first<A>);
}

/// Merge two lists already in canonical order into one, dropping ids already
/// present (the first list wins). O(n + m).
template <typename A>
inline QVector<A> merge_sorted_unique(const QVector<A>& primary, const QVector<A>& secondary) {
    QSet<QString> seen;
    seen.reserve(primary.size() + secondary.size());
    QVector<A> out;
    out.reserve(primary.size() + secondary.size());
    auto i = primary.cbegin();
    auto j = secondary.cbegin();
    auto take = [&](const A& a) {
        if (a.id.isEmpty() || !seen.contains(a.id)) {
            seen.insert(a.id);
            out.append(a);
        }
    };
    // Every primary id first, so a secondary copy can never shadow it.
    for (const auto& a : primary)
        seen.insert(a.id);
    while (i != primary.cend() || j != secondary.cend()) {
        if (j == secondary.cend() || (i != primary.cend() && !newer_first(*j, *i))) {
            out.append(*i);
            ++i;
        } else {
            take(*j);
            ++j;
        }
    }
    return out;
}

/// Source-diversity cap for the default list: at most `per_hour` stories of
/// one source per clock hour of publication, keeping the first ones in the
/// list's own order (newest first, or most relevant first). Order of what is
/// kept is unchanged; `dropped` (optional) receives how many were held back.
template <typename A>
inline QVector<A> cap_per_source_hour(const QVector<A>& list, int per_hour, int* dropped = nullptr) {
    QHash<QString, int> used;
    QVector<A> out;
    out.reserve(list.size());
    int n_dropped = 0;
    for (const auto& a : list) {
        const QString key = a.source.toUpper() + QLatin1Char('|') + QString::number(a.sort_ts / 3600);
        int& count = used[key];
        if (count >= per_hour) {
            ++n_dropped;
            continue;
        }
        ++count;
        out.append(a);
    }
    if (dropped)
        *dropped = n_dropped;
    return out;
}

} // namespace fincept::services::news_feed
