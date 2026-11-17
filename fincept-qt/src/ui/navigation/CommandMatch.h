#pragma once
// CommandMatch.h — how a command-bar query matches a screen (title, id,
// aliases, keywords, description). Qt Core only and header-only, so
// tests/tst_command_match.cpp exercises it without the widget.
//
// Matching is word based and tolerant of spacing and punctuation: the query
// and every field are split into lower-case words at anything that is not a
// letter or digit, so "alt investments", "Alt. Investments", "alt inv" and
// "investments" all find "Alt. Investments", and "equity res" finds "Equity
// Research". A query matches a field when each of its words is the start of a
// word there (token-prefix matching). The older substring rules stay, so every
// alias / name / keyword query that matched before still matches with the same
// rank.

#include <QString>
#include <QStringList>

#include <algorithm>

namespace fincept::ui::command_match {

/// Lower-case words of `text`, split at every character that is not a letter
/// or digit ("M&A Analytics" -> m, a, analytics; "alt_investments" -> alt,
/// investments).
inline QStringList words(const QString& text) {
    QStringList out;
    QString cur;
    for (const QChar c : text) {
        if (c.isLetterOrNumber()) {
            cur += c.toLower();
        } else if (!cur.isEmpty()) {
            out.append(cur);
            cur.clear();
        }
    }
    if (!cur.isEmpty())
        out.append(cur);
    return out;
}

/// True when every query word is the start of a DIFFERENT word of `field`, in
/// the same order ("equity res" ~ "Equity Research", not "research equity").
inline bool prefixes_in_order(const QStringList& query, const QStringList& field) {
    int at = 0;
    for (const QString& q : query) {
        bool found = false;
        while (at < field.size()) {
            if (field[at++].startsWith(q)) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

/// True when every query word is the start of some word of `field` (any order).
inline bool prefixes_anywhere(const QStringList& query, const QStringList& field) {
    return std::all_of(query.begin(), query.end(), [&field](const QString& q) {
        return std::any_of(field.begin(), field.end(), [&q](const QString& w) { return w.startsWith(q); });
    });
}

/// Rank of a screen for `query` (0 = no match; higher = better):
///   100 an alias typed exactly            95 the full title ("alt investments")
///    90 the start of the title            85 title words, in order ("equity res")
///    80 the start of an alias             75 title / id / alias words, any order
///    70 title contains the query          60 an alias contains it
///    45 words found anywhere (keywords, description too)
///    40 description contains it          30 a keyword contains it
inline int score(const QString& query, const QString& name, const QString& id, const QStringList& aliases,
                 const QStringList& keywords, const QString& description) {
    const QString q = query.trimmed().toLower();
    const QStringList q_words = words(q);
    if (q.isEmpty() || q_words.isEmpty())
        return 0;
    const QString q_norm = q_words.join(QLatin1Char(' '));

    for (const QString& a : aliases)
        if (a.toLower() == q)
            return 100;

    const QStringList name_words = words(name);
    const QString name_norm = name_words.join(QLatin1Char(' '));
    int best = 0;
    auto take = [&best](int s) { best = std::max(best, s); };

    if (!name_norm.isEmpty()) {
        // Also the title typed without its spaces ("altinvestments").
        if (name_norm == q_norm || name_words.join(QString()) == q_words.join(QString()))
            take(95);
        else if (name_norm.startsWith(q_norm))
            take(90);
        else if (prefixes_in_order(q_words, name_words))
            take(85);
    }
    for (const QString& a : aliases)
        if (a.toLower().startsWith(q))
            take(80);

    QStringList title_words = name_words;
    title_words += words(id);
    for (const QString& a : aliases)
        title_words += words(a);
    if (prefixes_anywhere(q_words, title_words))
        take(75);

    if (name.toLower().contains(q))
        take(70);
    for (const QString& a : aliases)
        if (a.toLower().contains(q))
            take(60);

    if (best < 45) {
        QStringList all_words = title_words;
        for (const QString& k : keywords)
            all_words += words(k);
        all_words += words(description);
        if (prefixes_anywhere(q_words, all_words))
            take(45);
    }
    if (description.toLower().contains(q))
        take(40);
    for (const QString& k : keywords)
        if (k.toLower().contains(q))
            take(30);
    return best;
}

/// Score at or above which the WHOLE typed text is a screen title being typed
/// (its words, in order) — used to tell "equity res" (Equity Research) from
/// "equity rep…" (the dock verb "replace"). Any-order matches (75) do not count:
/// "markets a" stays the start of "markets add", not "Asia Markets".
inline constexpr int kTitleMatch = 85;

} // namespace fincept::ui::command_match
