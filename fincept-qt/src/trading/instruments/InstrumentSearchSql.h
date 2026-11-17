#pragma once
// InstrumentSearchSql.h — how the symbol search is phrased for SQLite when a
// broker's catalogue is not in memory (InstrumentRepository::search / search_all;
// InstrumentService serves every loaded broker from its in-memory index).
//
// Pure string helpers (Qt Core only, header-only) so the rules can be unit-tested
// without a database:
//  - User text is always bound, never spliced into SQL. '%', '_' and '\' typed by
//    the user are escaped so they match literally (paired with ESCAPE '\' in SQL);
//    before, "A_B" matched "AXB" and a lone "%" matched every row.
//  - A query shorter than kMinSubstringChars matches symbol PREFIXES only, as a
//    range on the symbol index (symbol >= lo AND symbol < hi). '%n%' over three
//    columns could not use any index and read every instrument row per keystroke.
// Every symbol lives in this namespace so unity builds cannot collide with it.

#include <QChar>
#include <QString>

namespace fincept::trading::instrument_search_sql {

/// Queries shorter than this (after trimming) use the indexed prefix match only.
inline constexpr int kMinSubstringChars = 3;

/// `s` with LIKE's wildcards ('%', '_') and the escape character itself ('\')
/// preceded by '\'. Use with `LIKE ? ESCAPE '\'`.
inline QString escape_like(const QString& s) {
    QString out;
    out.reserve(s.size() + 4);
    for (const QChar c : s) {
        if (c == QLatin1Char('\\') || c == QLatin1Char('%') || c == QLatin1Char('_'))
            out.append(QLatin1Char('\\'));
        out.append(c);
    }
    return out;
}

/// "contains `query`" — '%' + escaped upper-cased query + '%'.
inline QString contains_pattern(const QString& query) {
    return QLatin1Char('%') + escape_like(query.trimmed().toUpper()) + QLatin1Char('%');
}

/// "starts with `query`" — escaped upper-cased query + '%'.
inline QString starts_with_pattern(const QString& query) {
    return escape_like(query.trimmed().toUpper()) + QLatin1Char('%');
}

/// True when the query is too short for a substring scan (prefix match only).
inline bool prefix_only(const QString& query) {
    return query.trimmed().size() < kMinSubstringChars;
}

/// Bounds [lo, hi) such that, under SQLite's BINARY collation, `col >= lo AND
/// col < hi` holds exactly for the values that start with `prefix`. lo is the
/// prefix itself; hi is the prefix with its last character incremented. Pass the
/// upper-cased query: symbols are stored upper-case. Returns false when no such
/// bound can be written (empty prefix, a surrogate pair, or U+FFFF last) — the
/// caller then falls back to LIKE with starts_with_pattern().
///
/// BINARY compares UTF-8 bytes, which orders like code points; for BMP text
/// without surrogates that is also QString's UTF-16 order, so the increment is
/// safe. An increment that would land in the surrogate block (U+D800-U+DFFF)
/// skips to U+E000 — no character lies in between.
inline bool prefix_range(const QString& prefix, QString& lo, QString& hi) {
    if (prefix.isEmpty())
        return false;
    for (const QChar c : prefix) {
        if (c.isSurrogate())
            return false;
    }
    const char16_t last = prefix.at(prefix.size() - 1).unicode();
    if (last == 0xFFFF)
        return false;
    char16_t next = static_cast<char16_t>(last + 1);
    if (next >= 0xD800 && next <= 0xDFFF)
        next = 0xE000;
    lo = prefix;
    hi = prefix.left(prefix.size() - 1) + QChar(next);
    return true;
}

} // namespace fincept::trading::instrument_search_sql
