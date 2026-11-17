// src/screens/notes/NotesLogic.h
//
// The pure half of the Notes screen — Qt Core only, no widgets, no database —
// so it can be unit tested (tests/tst_notes_logic.cpp) and run on worker threads:
//
//   * tag / ticker lists as stored in financial_notes (comma-separated text),
//   * $CASHTAG extraction from a Markdown body,
//   * the list preview line, word count and timestamps,
//   * search (words, $TICKER, #tag), view filters, sorting, navigation counts,
//   * the built-in note templates,
//   * Markdown export / import with a front-matter block (round-trips every field
//     the screen edits),
//   * each entry's board row (status, manual order, review date, origin) with the
//     defaults a note gets before it has one (storage/repositories/NoteBoardTypes.h).
//
// Entries are templated on the note type so the tests can use a plain struct
// with the same field names as fincept::FinancialNote.
// The board itself (lanes, grouping, ordering, commands) is NotesBoardLogic.h.
#pragma once

#include "storage/repositories/NoteBoardTypes.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QPair>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimeZone>
#include <QVariantMap>
#include <QVector>

#include <algorithm>
#include <optional>

namespace fincept::screens::notes {

// ── Tag / ticker lists ───────────────────────────────────────────────────────

/// Tags as typed, split on commas / semicolons / new lines; trimmed, de-duplicated
/// case-insensitively (first spelling wins), order kept. Tags may contain spaces.
inline QStringList split_tags(const QString& csv) {
    static const QRegularExpression sep(QStringLiteral("[,;\\n]"));
    QStringList out;
    QSet<QString> seen;
    for (const QString& raw : csv.split(sep, Qt::SkipEmptyParts)) {
        QString t = raw.trimmed();
        if (t.startsWith(QLatin1Char('#')))
            t = t.mid(1).trimmed();
        if (t.isEmpty())
            continue;
        const QString key = t.toLower();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        out << t;
    }
    return out;
}

/// A ticker as the terminal writes it ("$aapl " -> "AAPL", "brk.b" -> "BRK.B",
/// "^gspc" -> "^GSPC", "7203.t" -> "7203.T"); empty when it is not a symbol.
inline QString normalize_ticker(const QString& raw) {
    QString t = raw.trimmed();
    if (t.startsWith(QLatin1Char('$')))
        t = t.mid(1);
    t = t.toUpper();
    static const QRegularExpression ok(QStringLiteral("^[A-Z0-9^][A-Z0-9.\\-^=]{0,19}$"));
    static const QRegularExpression letter(QStringLiteral("[A-Z]"));
    if (!ok.match(t).hasMatch() || !letter.match(t).hasMatch())
        return {};
    return t;
}

/// Tickers split on commas / semicolons / whitespace, normalised, de-duplicated.
inline QStringList split_tickers(const QString& csv) {
    static const QRegularExpression sep(QStringLiteral("[,;\\s]+"));
    QStringList out;
    for (const QString& raw : csv.split(sep, Qt::SkipEmptyParts)) {
        const QString t = normalize_ticker(raw);
        if (!t.isEmpty() && !out.contains(t))
            out << t;
    }
    return out;
}

/// Storage form of a tag or ticker list ("a, b, c") — what financial_notes holds
/// and what the dashboard Notes tile shows.
inline QString join_list(const QStringList& items) {
    return items.join(QStringLiteral(", "));
}

/// $CASHTAG symbols written in a Markdown body ("$AAPL", "$BRK.B", "$RELIANCE.NS"),
/// upper-cased and de-duplicated in order of appearance. Skipped: fenced code
/// blocks, `inline code`, prices ("$100") and anything glued to a word ("a$B").
inline QStringList extract_cashtags(const QString& markdown) {
    static const QRegularExpression cashtag(QStringLiteral("(?<![\\w$])\\$([A-Za-z][A-Za-z0-9.\\-^=]{0,14})\\b"));
    static const QRegularExpression inline_code(QStringLiteral("`[^`]*`"));
    QStringList out;
    if (!markdown.contains(QLatin1Char('$')))
        return out;
    bool in_fence = false;
    for (const QString& raw : markdown.split(QLatin1Char('\n'))) {
        const QString trimmed = raw.trimmed();
        if (trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"))) {
            in_fence = !in_fence;
            continue;
        }
        if (in_fence || !raw.contains(QLatin1Char('$')))
            continue;
        QString line = raw;
        if (line.contains(QLatin1Char('`')))
            line.replace(inline_code, QStringLiteral(" "));
        auto it = cashtag.globalMatch(line);
        while (it.hasNext()) {
            const QString t = normalize_ticker(it.next().captured(1));
            if (!t.isEmpty() && !out.contains(t))
                out << t;
        }
    }
    return out;
}

/// `a` followed by every entry of `b` not already in it.
inline QStringList merge_unique(QStringList a, const QStringList& b) {
    for (const QString& s : b)
        if (!a.contains(s))
            a << s;
    return a;
}

/// Markdown body with each $CASHTAG turned into a fincept-symbol: link (the
/// viewer offers "Open in…" on click). Code blocks and code spans are left alone.
inline QString markdown_with_symbol_links(const QString& content) {
    static const QRegularExpression cashtag(QStringLiteral("(?<![\\w$\\[])\\$([A-Za-z][A-Za-z0-9.\\-^=]{0,14})\\b"));
    if (!content.contains(QLatin1Char('$')))
        return content;
    QStringList out;
    bool in_fence = false;
    for (const QString& line : content.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"))) {
            in_fence = !in_fence;
            out << line;
            continue;
        }
        if (in_fence || !line.contains(QLatin1Char('$')) || line.contains(QLatin1Char('`'))) {
            out << line;
            continue;
        }
        QString l = line;
        l.replace(cashtag, QStringLiteral("[$\\1](fincept-symbol:\\1)"));
        out << l;
    }
    return out.join(QLatin1Char('\n'));
}

// ── Preview, title, counts ───────────────────────────────────────────────────

/// One line of Markdown as plain text: emphasis, code ticks and link syntax removed.
inline QString strip_inline_markdown(QString s) {
    static const QRegularExpression link(QStringLiteral("!?\\[([^\\]]*)\\]\\([^)]*\\)"));
    static const QRegularExpression marks(QStringLiteral("\\*\\*|__|~~|`"));
    static const QRegularExpression single(QStringLiteral("(?<![\\w*])\\*(?=\\S)|(?<=\\S)\\*(?![\\w*])"));
    s.replace(link, QStringLiteral("\\1"));
    s.remove(marks);
    s.remove(single);
    return s;
}

/// Plain-text lines of a Markdown body: block markers (#, >, -, 1., [ ]),
/// table pipes, rules and fences removed; blank lines dropped.
inline QStringList plain_lines(const QString& content, int max_lines) {
    static const QRegularExpression lead(
        QStringLiteral("^(#{1,6}\\s+|>\\s*|[-*+]\\s+\\[[ xX]\\]\\s+|[-*+]\\s+|\\d+[.)]\\s+)+"));
    static const QRegularExpression rule(QStringLiteral("^([-*_=]\\s*){3,}$"));
    static const QRegularExpression table_rule(QStringLiteral("^[\\s|:\\-]+$"));
    QStringList out;
    bool in_fence = false;
    qsizetype pos = 0;
    while (pos <= content.size() && out.size() < max_lines) {
        qsizetype nl = content.indexOf(QLatin1Char('\n'), pos);
        if (nl < 0)
            nl = content.size();
        QString line = content.mid(pos, nl - pos).trimmed();
        pos = nl + 1;
        if (line.startsWith(QLatin1String("```")) || line.startsWith(QLatin1String("~~~"))) {
            in_fence = !in_fence;
            continue;
        }
        if (line.isEmpty())
            continue;
        if (!in_fence) {
            if (rule.match(line).hasMatch())
                continue;
            if (line.startsWith(QLatin1Char('|'))) {
                if (table_rule.match(line).hasMatch())
                    continue;
                line.replace(QLatin1Char('|'), QLatin1Char(' '));
            }
            line.remove(lead);
            line = strip_inline_markdown(line).simplified();
        }
        if (!line.isEmpty())
            out << line;
    }
    return out;
}

/// The list's second line: the start of the body as plain text, skipping a first
/// line that only repeats the title.
inline QString preview_text(const QString& title, const QString& content, int max_chars = 220) {
    QStringList lines = plain_lines(content, 6);
    if (!lines.isEmpty() && lines.first().compare(title.trimmed(), Qt::CaseInsensitive) == 0)
        lines.removeFirst();
    QString out;
    for (const QString& l : lines) {
        if (!out.isEmpty())
            out += QStringLiteral("  ");
        out += l;
        if (out.size() >= max_chars)
            break;
    }
    return out.left(max_chars);
}

/// A title for a note saved without one: its first line of text (≤ 80 chars).
inline QString title_from_body(const QString& content) {
    const QStringList lines = plain_lines(content, 1);
    return lines.isEmpty() ? QString() : lines.first().left(80).trimmed();
}

/// Runs of non-space characters.
inline int word_count(const QString& text) {
    int n = 0;
    bool in_word = false;
    for (const QChar c : text) {
        const bool space = c.isSpace();
        if (!space && !in_word)
            ++n;
        in_word = !space;
    }
    return n;
}

// ── Timestamps ───────────────────────────────────────────────────────────────

/// financial_notes times: SQLite datetime('now') ("YYYY-MM-DD HH:MM:SS", UTC) or ISO 8601.
inline QDateTime parse_db_time(const QString& s) {
    if (s.isEmpty())
        return {};
    if (s.size() == 19 && s.at(10) == QLatin1Char(' ')) {
        // Date and time parsed apart: a local-time parse would reject times in a DST gap.
        const QDate d = QDate::fromString(s.left(10), Qt::ISODate);
        const QTime t = QTime::fromString(s.mid(11), QStringLiteral("HH:mm:ss"));
        if (d.isValid() && t.isValid())
            return QDateTime(d, t, QTimeZone(QTimeZone::UTC));
    }
    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::ISODate);
    return dt;
}

/// The current time in the form SQLite's datetime('now') writes.
inline QString db_time(const QDateTime& utc) {
    return utc.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

/// Compact age for the list: "now", "12m", "5h", "3d", "Nov 4", "Nov 4, 2024".
inline QString relative_time(qint64 then_ms, qint64 now_ms) {
    if (then_ms <= 0)
        return {};
    const qint64 secs = std::max<qint64>(0, (now_ms - then_ms) / 1000);
    if (secs < 60)
        return QStringLiteral("now");
    if (secs < 3600)
        return QString::number(secs / 60) + QLatin1Char('m');
    if (secs < 86400)
        return QString::number(secs / 3600) + QLatin1Char('h');
    if (secs < 7 * 86400)
        return QString::number(secs / 86400) + QLatin1Char('d');
    const QDateTime then = QDateTime::fromMSecsSinceEpoch(then_ms).toLocalTime();
    const QDateTime now = QDateTime::fromMSecsSinceEpoch(now_ms).toLocalTime();
    return then.date().year() == now.date().year() ? then.toString(QStringLiteral("MMM d"))
                                                   : then.toString(QStringLiteral("MMM d, yyyy"));
}

// ── Priority / category ──────────────────────────────────────────────────────

/// CRITICAL 4, HIGH 3, MEDIUM 2, LOW 1, anything else 0.
inline int priority_rank(const QString& p) {
    const QString u = p.trimmed().toUpper();
    if (u == QLatin1String("CRITICAL"))
        return 4;
    if (u == QLatin1String("HIGH"))
        return 3;
    if (u == QLatin1String("MEDIUM"))
        return 2;
    if (u == QLatin1String("LOW"))
        return 1;
    return 0;
}

/// The category a note is filed under, as a grouping key ("" -> "GENERAL").
inline QString category_key(const QString& category) {
    const QString u = category.trimmed().toUpper();
    return u.isEmpty() ? QStringLiteral("GENERAL") : u;
}

/// The categories the editor offers, in menu (and board lane) order.
inline QStringList default_category_codes() {
    return {QStringLiteral("TRADE_IDEA"), QStringLiteral("RESEARCH"), QStringLiteral("MARKET_ANALYSIS"),
            QStringLiteral("EARNINGS"),   QStringLiteral("ECONOMIC"), QStringLiteral("PORTFOLIO"),
            QStringLiteral("GENERAL")};
}

/// "CRITICAL" / "HIGH" / "MEDIUM" / "LOW" for priority_rank() 4..1, "" for 0.
inline QString priority_code(int rank) {
    switch (rank) {
    case 4:
        return QStringLiteral("CRITICAL");
    case 3:
        return QStringLiteral("HIGH");
    case 2:
        return QStringLiteral("MEDIUM");
    case 1:
        return QStringLiteral("LOW");
    default:
        return {};
    }
}

inline QStringList priority_codes() {
    return {QStringLiteral("CRITICAL"), QStringLiteral("HIGH"), QStringLiteral("MEDIUM"), QStringLiteral("LOW")};
}

// ── Search ───────────────────────────────────────────────────────────────────

/// A parsed search box: plain words (all must appear), $TICKER (a linked ticker
/// starting with it) and #tag (a tag starting with it). "quoted phrases" stay whole.
struct SearchQuery {
    QStringList words;   ///< lower-case
    QStringList tickers; ///< upper-case
    QStringList tags;    ///< lower-case
    bool is_empty() const { return words.isEmpty() && tickers.isEmpty() && tags.isEmpty(); }
    /// What to highlight in titles, previews and the body.
    QStringList highlight_terms() const { return words + tickers + tags; }
    bool operator==(const SearchQuery& o) const {
        return words == o.words && tickers == o.tickers && tags == o.tags;
    }
};

inline SearchQuery parse_search(const QString& text) {
    SearchQuery q;
    QStringList tokens;
    QString cur;
    bool quoted = false;
    for (const QChar c : text) {
        if (c == QLatin1Char('"')) {
            if (quoted || !cur.trimmed().isEmpty())
                tokens << cur;
            cur.clear();
            quoted = !quoted;
        } else if (c.isSpace() && !quoted) {
            if (!cur.isEmpty())
                tokens << cur;
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.isEmpty())
        tokens << cur;
    for (const QString& raw : tokens) {
        const QString tok = raw.trimmed();
        if (tok.isEmpty())
            continue;
        if (tok.size() > 1 && tok.startsWith(QLatin1Char('$'))) {
            const QString t = normalize_ticker(tok);
            if (!t.isEmpty() && !q.tickers.contains(t)) {
                q.tickers << t;
                continue;
            }
        }
        if (tok.size() > 1 && tok.startsWith(QLatin1Char('#'))) {
            const QString t = tok.mid(1).toLower();
            if (!q.tags.contains(t))
                q.tags << t;
            continue;
        }
        const QString w = tok.toLower();
        if (!q.words.contains(w))
            q.words << w;
    }
    return q;
}

/// Case-insensitive [start, length) ranges of every term in `text`, sorted and
/// merged. Used to highlight matches in the list and the editor.
inline QVector<QPair<int, int>> match_ranges(const QString& text, const QStringList& terms, int max_ranges = 200) {
    QVector<QPair<int, int>> r;
    for (const QString& term : terms) {
        if (term.isEmpty())
            continue;
        qsizetype from = 0;
        while (r.size() < max_ranges) {
            const qsizetype at = text.indexOf(term, from, Qt::CaseInsensitive);
            if (at < 0)
                break;
            r.append({static_cast<int>(at), static_cast<int>(term.size())});
            from = at + term.size();
        }
    }
    std::sort(r.begin(), r.end());
    QVector<QPair<int, int>> merged;
    for (const auto& p : r) {
        if (!merged.isEmpty() && p.first <= merged.last().first + merged.last().second) {
            const int end = std::max(merged.last().first + merged.last().second, p.first + p.second);
            merged.last().second = end - merged.last().first;
        } else {
            merged.append(p);
        }
    }
    return merged;
}

// ── Entries: a note plus what the list needs, computed once per load ────────

template <class Note>
struct BasicEntry {
    Note note;
    QString haystack;        ///< lower-case title, body, tags and tickers (search)
    QStringList tag_list;    ///< as written
    QStringList ticker_list; ///< upper-case: the note's tickers, then $CASHTAGS in the body
    QString category;        ///< category_key()
    QString preview;         ///< preview_text()
    qint64 updated_ms = 0;
    qint64 created_ms = 0;
    int priority = 0; ///< priority_rank()
    /// Board row: the stored one (meta.stored) or the defaults for this note.
    fincept::notes_board::NoteBoardMeta meta;
    /// Created from a Chat Mode run (origin "agent_runs" or Chat Mode's tag).
    bool linked = false;
    QDate review; ///< meta.review_date()
};

/// True when `tags` carries the tag Chat Mode puts on the notes it saves.
inline bool has_agent_run_tag(const QStringList& tags) {
    for (const QString& t : tags)
        if (t.compare(fincept::notes_board::kAgentRunTag, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

/// The run id prefix Chat Mode writes in the footer of a note it saved
/// ("---\nSaved from Chat Mode: agent run “Title” (1a2b3c4d) · …"); empty when none.
/// Only the 8-hex-digit group after the last "---" rule is read, so it works in
/// every UI language.
inline QString chat_run_ref_from_body(const QString& content) {
    static const QRegularExpression re(QStringLiteral("\\(([0-9A-Fa-f]{8})\\)"));
    const QString tail = content.right(800);
    const qsizetype rule = tail.lastIndexOf(QLatin1String("\n---\n"));
    if (rule < 0)
        return {};
    const auto m = re.match(tail, rule);
    return m.hasMatch() ? m.captured(1).toLower() : QString();
}

/// The Chat Mode run a "notes.created" / "notes.updated" event names
/// (agent_tasks::artefact_logic::note_created_event): nullopt when the event is not
/// from Chat Mode (its "source" is not "agent_runs"); an empty string when it is but
/// carries no run id (an older build) — the note is still linked.
inline std::optional<QString> chat_run_from_event(const QVariantMap& payload) {
    if (payload.value(QStringLiteral("source")).toString() != fincept::notes_board::kOriginAgentRuns)
        return std::nullopt;
    for (const char* key : {"run_id", "run", "agent_run_id"}) {
        const QString run = payload.value(QString::fromLatin1(key)).toString().trimmed();
        if (!run.isEmpty())
            return run;
    }
    return QString();
}

/// The board row a note has before one is stored: status from its category (CLOSED
/// when archived), order by last edit (newest first), origin from Chat Mode's tag.
template <class Entry>
fincept::notes_board::NoteBoardMeta default_meta(const Entry& e) {
    fincept::notes_board::NoteBoardMeta m;
    m.note_id = e.note.id;
    m.status = fincept::notes_board::default_status(e.category, e.note.is_archived);
    m.rank = fincept::notes_board::default_rank(e.updated_ms / 1000);
    if (has_agent_run_tag(e.tag_list))
        m.origin = fincept::notes_board::kOriginAgentRuns;
    m.stored = false;
    return m;
}

/// Put a stored board row on an entry (status normalised; the derived fields follow).
template <class Entry>
void apply_meta(Entry& e, fincept::notes_board::NoteBoardMeta m) {
    m.note_id = e.note.id;
    m.status = fincept::notes_board::normalize_status(m.status);
    e.meta = std::move(m);
    e.review = e.meta.review_date();
    e.linked = e.meta.origin == fincept::notes_board::kOriginAgentRuns || has_agent_run_tag(e.tag_list);
}

template <class Note>
BasicEntry<Note> make_entry(const Note& n) {
    BasicEntry<Note> e;
    e.note = n;
    e.tag_list = split_tags(n.tags);
    e.ticker_list = merge_unique(split_tickers(n.tickers), extract_cashtags(n.content));
    e.category = category_key(n.category);
    e.preview = preview_text(n.title, n.content);
    const QDateTime up = parse_db_time(n.updated_at);
    const QDateTime cr = parse_db_time(n.created_at);
    e.updated_ms = up.isValid() ? up.toMSecsSinceEpoch() : 0;
    e.created_ms = cr.isValid() ? cr.toMSecsSinceEpoch() : 0;
    if (e.updated_ms == 0)
        e.updated_ms = e.created_ms;
    e.priority = priority_rank(n.priority);
    apply_meta(e, default_meta(e));
    QString hay;
    hay.reserve(n.title.size() + n.content.size() + n.tags.size() + 16 * e.ticker_list.size() + 4);
    hay += n.title;
    hay += QLatin1Char('\n');
    hay += n.content;
    hay += QLatin1Char('\n');
    hay += n.tags;
    hay += QLatin1Char('\n');
    hay += e.ticker_list.join(QLatin1Char(' '));
    e.haystack = hay.toLower();
    return e;
}

template <class Entry>
bool matches(const Entry& e, const SearchQuery& q) {
    for (const QString& w : q.words)
        if (!e.haystack.contains(w))
            return false;
    for (const QString& t : q.tickers) {
        bool hit = false;
        for (const QString& mine : e.ticker_list)
            if (mine.startsWith(t)) {
                hit = true;
                break;
            }
        if (!hit)
            return false;
    }
    for (const QString& g : q.tags) {
        bool hit = false;
        for (const QString& mine : e.tag_list)
            if (mine.startsWith(g, Qt::CaseInsensitive)) {
                hit = true;
                break;
            }
        if (!hit)
            return false;
    }
    return true;
}

// ── Views (the navigation pane) ──────────────────────────────────────────────

enum class ViewKind { All, Pinned, Category, Tag, Ticker, Archived };

struct ViewFilter {
    ViewKind kind = ViewKind::All;
    QString key; ///< category key, tag (any case) or ticker

    /// Stable string form, persisted in the screen state ("all", "cat:RESEARCH", "tag:ai", "ticker:NVDA").
    QString to_string() const {
        switch (kind) {
        case ViewKind::All:
            return QStringLiteral("all");
        case ViewKind::Pinned:
            return QStringLiteral("pinned");
        case ViewKind::Archived:
            return QStringLiteral("archived");
        case ViewKind::Category:
            return QStringLiteral("cat:") + key;
        case ViewKind::Tag:
            return QStringLiteral("tag:") + key;
        case ViewKind::Ticker:
            return QStringLiteral("ticker:") + key;
        }
        return QStringLiteral("all");
    }
    static ViewFilter from_string(const QString& s) {
        ViewFilter v;
        if (s == QLatin1String("pinned"))
            v.kind = ViewKind::Pinned;
        else if (s == QLatin1String("archived"))
            v.kind = ViewKind::Archived;
        else if (s.startsWith(QLatin1String("cat:")))
            v = {ViewKind::Category, category_key(s.mid(4))};
        else if (s.startsWith(QLatin1String("tag:")) && s.size() > 4)
            v = {ViewKind::Tag, s.mid(4)};
        else if (s.startsWith(QLatin1String("ticker:")) && s.size() > 7)
            v = {ViewKind::Ticker, s.mid(7).toUpper()};
        return v;
    }
    bool operator==(const ViewFilter& o) const { return kind == o.kind && key == o.key; }
};

template <class Entry>
bool in_view(const Entry& e, const ViewFilter& v) {
    if (v.kind == ViewKind::Archived)
        return e.note.is_archived;
    if (e.note.is_archived)
        return false;
    switch (v.kind) {
    case ViewKind::All:
        return true;
    case ViewKind::Pinned:
        return e.note.is_favorite;
    case ViewKind::Category:
        return e.category == v.key;
    case ViewKind::Tag:
        for (const QString& t : e.tag_list)
            if (t.compare(v.key, Qt::CaseInsensitive) == 0)
                return true;
        return false;
    case ViewKind::Ticker:
        return e.ticker_list.contains(v.key);
    case ViewKind::Archived:
        break;
    }
    return false;
}

enum class SortKey { Updated = 0, Created = 1, Title = 2, Priority = 3 };

/// Pinned notes first (unless `pinned_first` is false), then by `key`; ties by
/// last edit, newest first, then id.
template <class Entry>
void sort_entries(QVector<Entry>& rows, SortKey key, bool pinned_first = true) {
    std::stable_sort(rows.begin(), rows.end(), [key, pinned_first](const Entry& a, const Entry& b) {
        if (pinned_first && a.note.is_favorite != b.note.is_favorite)
            return a.note.is_favorite;
        switch (key) {
        case SortKey::Created:
            if (a.created_ms != b.created_ms)
                return a.created_ms > b.created_ms;
            break;
        case SortKey::Title: {
            const int c = a.note.title.compare(b.note.title, Qt::CaseInsensitive);
            if (c != 0)
                return c < 0;
            break;
        }
        case SortKey::Priority:
            if (a.priority != b.priority)
                return a.priority > b.priority;
            break;
        case SortKey::Updated:
            break;
        }
        if (a.updated_ms != b.updated_ms)
            return a.updated_ms > b.updated_ms;
        return a.note.id > b.note.id;
    });
}

/// The list for one view + search, sorted.
template <class Entry>
QVector<Entry> filter_and_sort(const QVector<Entry>& all, const ViewFilter& v, const SearchQuery& q, SortKey key) {
    QVector<Entry> out;
    out.reserve(all.size());
    const bool search = !q.is_empty();
    for (const Entry& e : all)
        if (in_view(e, v) && (!search || matches(e, q)))
            out.append(e);
    sort_entries(out, key, v.kind != ViewKind::Pinned && v.kind != ViewKind::Archived);
    return out;
}

/// Counts for the navigation pane (archived notes only count as archived).
struct NavCounts {
    int all = 0;
    int pinned = 0;
    int archived = 0;
    QHash<QString, int> categories;   ///< category key -> count
    QHash<QString, int> tags;         ///< lower-case tag -> count
    QHash<QString, QString> tag_text; ///< lower-case tag -> first spelling seen
    QHash<QString, int> tickers;      ///< ticker -> count
};

template <class Entry>
NavCounts count_nav(const QVector<Entry>& all) {
    NavCounts c;
    for (const Entry& e : all) {
        if (e.note.is_archived) {
            ++c.archived;
            continue;
        }
        ++c.all;
        if (e.note.is_favorite)
            ++c.pinned;
        ++c.categories[e.category];
        for (const QString& t : e.tag_list) {
            const QString k = t.toLower();
            ++c.tags[k];
            if (!c.tag_text.contains(k))
                c.tag_text.insert(k, t);
        }
        for (const QString& t : e.ticker_list)
            ++c.tickers[t];
    }
    return c;
}

/// Keys of `counts` by count (descending), then name.
inline QStringList keys_by_count(const QHash<QString, int>& counts) {
    QStringList keys = counts.keys();
    std::sort(keys.begin(), keys.end(), [&counts](const QString& a, const QString& b) {
        const int ca = counts.value(a), cb = counts.value(b);
        if (ca != cb)
            return ca > cb;
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return keys;
}

// ── Templates ────────────────────────────────────────────────────────────────

/// A starting point for a new note. {{date}} and {{ticker}} are filled in by
/// fill_template(); {{cashtag}} becomes "$AAPL" (or a "Ticker:" prompt).
struct TemplateDef {
    QString id;
    QString name;
    QString category;
    QString priority;
    QString sentiment;
    QString tags;
    QString title;
    QString body;
    QString status = fincept::notes_board::kStatusIdea; ///< board lane a new note from it starts in
};

inline QVector<TemplateDef> builtin_templates() {
    QVector<TemplateDef> t = {
        {QStringLiteral("trade_idea"), QStringLiteral("Trade idea"), QStringLiteral("TRADE_IDEA"),
         QStringLiteral("HIGH"), QStringLiteral("NEUTRAL"), QStringLiteral("trade idea"),
         QStringLiteral("Trade idea: {{ticker}}"),
         QStringLiteral("## Thesis\n{{cashtag}} — why this trade, and why now?\n\n"
                        "## Setup\n- Timeframe: \n- Catalyst: \n\n"
                        "## Entry\n- Trigger / price: \n- Size: \n\n"
                        "## Stop\n- Invalidation level: \n- Why there: \n\n"
                        "## Target\n- Target 1: \n- Target 2: \n\n"
                        "## Risk\n- Risk per trade (R): \n- Reward / risk: \n- What would prove me wrong: \n")},
        {QStringLiteral("earnings_prep"), QStringLiteral("Earnings prep"), QStringLiteral("EARNINGS"),
         QStringLiteral("MEDIUM"), QStringLiteral("NEUTRAL"), QStringLiteral("earnings"),
         QStringLiteral("Earnings prep: {{ticker}}"),
         QStringLiteral("## Company\n{{cashtag}} — report date: \n\n"
                        "## Consensus\n| Metric | Consensus | My estimate |\n|---|---|---|\n| EPS |  |  |\n"
                        "| Revenue |  |  |\n| Guidance |  |  |\n\n"
                        "## What matters this quarter\n1. \n2. \n3. \n\n"
                        "## Implied move & positioning\n- Options implied move: \n- Positioning / sentiment: \n\n"
                        "## Plan\n- If it beats: \n- If it misses: \n")},
        {QStringLiteral("daily_journal"), QStringLiteral("Daily journal"), QStringLiteral("GENERAL"),
         QStringLiteral("LOW"), QStringLiteral("NEUTRAL"), QStringLiteral("journal"),
         QStringLiteral("Journal {{date}}"),
         QStringLiteral("## Pre-market\n- Market tone: \n- Key levels: \n- Events today: \n\n"
                        "## Plan\n- [ ] Watchlist reviewed\n- [ ] Risk per trade set\n- [ ] Max daily loss: \n\n"
                        "## Trades\n| Ticker | Side | Entry | Exit | R | Notes |\n|---|---|---|---|---|---|\n"
                        "|  |  |  |  |  |  |\n\n"
                        "## Review\n- What went well: \n- What to improve: \n- Rule breaks: \n")},
        {QStringLiteral("post_trade_review"), QStringLiteral("Post-trade review"), QStringLiteral("TRADE_IDEA"),
         QStringLiteral("MEDIUM"), QStringLiteral("NEUTRAL"), QStringLiteral("review"),
         QStringLiteral("Review: {{ticker}}"),
         QStringLiteral("## Trade\n{{cashtag}} — side:  · entry:  · exit:  · size: \n\n"
                        "## Result\n- P&L: \n- R multiple: \n\n"
                        "## Plan vs. execution\n- Did I follow the entry plan? \n- Did I respect the stop? \n"
                        "- Exit reason: \n\n"
                        "## What I learned\n- \n\n"
                        "## Grade (A–F)\n- Setup:  · Execution:  · Management: \n")},
    };
    t[0].status = fincept::notes_board::kStatusIdea;
    t[1].status = fincept::notes_board::kStatusResearching;
    t[2].status = fincept::notes_board::kStatusActive;
    t[3].status = fincept::notes_board::kStatusReview;
    return t;
}

/// `text` with {{date}} (yyyy-MM-dd), {{ticker}} and {{cashtag}} filled in. With no
/// ticker, {{ticker}} falls back to the date and {{cashtag}} to a "Ticker:" prompt.
inline QString fill_template(QString text, const QDate& date, const QString& ticker) {
    const QString d = date.toString(QStringLiteral("yyyy-MM-dd"));
    const QString t = normalize_ticker(ticker);
    text.replace(QStringLiteral("{{date}}"), d);
    text.replace(QStringLiteral("{{ticker}}"), t.isEmpty() ? d : t);
    text.replace(QStringLiteral("{{cashtag}}"), t.isEmpty() ? QStringLiteral("**Ticker:** ") : QLatin1Char('$') + t);
    return text;
}

// ── Markdown export / import ─────────────────────────────────────────────────

/// One note as a Markdown file: a front-matter block with the note's fields,
/// the title as a heading, then the body.
struct MarkdownDoc {
    QString title;
    QString body;
    QString category;
    QString priority;
    QString sentiment;
    QStringList tags;
    QStringList tickers;
    QString created;
    QString updated;
    bool pinned = false;
    bool archived = false;
};

inline QString yaml_quote(const QString& s) {
    QString e = s;
    e.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    e.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    e.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    return QLatin1Char('"') + e + QLatin1Char('"');
}

inline QString yaml_unquote(QString s) {
    s = s.trimmed();
    if (s.size() >= 2 && ((s.startsWith(QLatin1Char('"')) && s.endsWith(QLatin1Char('"'))) ||
                          (s.startsWith(QLatin1Char('\'')) && s.endsWith(QLatin1Char('\''))))) {
        const bool dq = s.startsWith(QLatin1Char('"'));
        s = s.mid(1, s.size() - 2);
        if (!dq)
            return s.replace(QStringLiteral("''"), QStringLiteral("'"));
        QString out;
        out.reserve(s.size());
        for (qsizetype i = 0; i < s.size(); ++i) {
            if (s.at(i) == QLatin1Char('\\') && i + 1 < s.size()) {
                const QChar n = s.at(++i);
                out += (n == QLatin1Char('n')) ? QChar(QLatin1Char('\n')) : n;
            } else {
                out += s.at(i);
            }
        }
        return out;
    }
    return s;
}

/// "[a, "b, c", d]" -> {a, "b, c", d}; a bare "a, b" works too.
inline QStringList yaml_list(QString s) {
    s = s.trimmed();
    if (s.startsWith(QLatin1Char('[')) && s.endsWith(QLatin1Char(']')))
        s = s.mid(1, s.size() - 2);
    QStringList out;
    QString cur;
    bool quoted = false;
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c == QLatin1Char('\\') && quoted && i + 1 < s.size()) {
            cur += c;
            cur += s.at(++i);
            continue;
        }
        if (c == QLatin1Char('"'))
            quoted = !quoted;
        if (c == QLatin1Char(',') && !quoted) {
            const QString v = yaml_unquote(cur);
            if (!v.isEmpty())
                out << v;
            cur.clear();
            continue;
        }
        cur += c;
    }
    const QString v = yaml_unquote(cur);
    if (!v.isEmpty())
        out << v;
    return out;
}

inline QString to_markdown(const MarkdownDoc& d) {
    const auto list = [](const QStringList& items) {
        QStringList q;
        for (const QString& s : items)
            q << yaml_quote(s);
        return QLatin1Char('[') + q.join(QStringLiteral(", ")) + QLatin1Char(']');
    };
    QString out = QStringLiteral("---\n");
    out += QStringLiteral("title: ") + yaml_quote(d.title) + QLatin1Char('\n');
    if (!d.category.isEmpty())
        out += QStringLiteral("category: ") + yaml_quote(d.category) + QLatin1Char('\n');
    if (!d.priority.isEmpty())
        out += QStringLiteral("priority: ") + yaml_quote(d.priority) + QLatin1Char('\n');
    if (!d.sentiment.isEmpty())
        out += QStringLiteral("sentiment: ") + yaml_quote(d.sentiment) + QLatin1Char('\n');
    if (!d.tags.isEmpty())
        out += QStringLiteral("tags: ") + list(d.tags) + QLatin1Char('\n');
    if (!d.tickers.isEmpty())
        out += QStringLiteral("tickers: ") + list(d.tickers) + QLatin1Char('\n');
    if (d.pinned)
        out += QStringLiteral("pinned: true\n");
    if (d.archived)
        out += QStringLiteral("archived: true\n");
    if (!d.created.isEmpty())
        out += QStringLiteral("created: ") + yaml_quote(d.created) + QLatin1Char('\n');
    if (!d.updated.isEmpty())
        out += QStringLiteral("updated: ") + yaml_quote(d.updated) + QLatin1Char('\n');
    out += QStringLiteral("---\n\n# ") + QString(d.title).replace(QLatin1Char('\n'), QLatin1Char(' ')) +
           QStringLiteral("\n\n") + d.body + QLatin1Char('\n');
    return out;
}

/// Reads a note back from a Markdown / text file. Front matter is optional: without
/// it a leading "# heading" is the title, else `fallback_title` (the file name).
inline MarkdownDoc from_markdown(const QString& text, const QString& fallback_title) {
    MarkdownDoc d;
    QString t = text;
    t.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (t.startsWith(QChar(0xFEFF)))
        t.remove(0, 1);
    QString body = t;
    if (t.startsWith(QLatin1String("---\n"))) {
        qsizetype end = t.indexOf(QLatin1String("\n---\n"), 3);
        qsizetype after = end + 5;
        if (end < 0 && t.endsWith(QLatin1String("\n---"))) {
            end = t.size() - 4;
            after = t.size();
        }
        if (end >= 0) {
            const QString block = t.mid(4, end - 4);
            for (const QString& line : block.split(QLatin1Char('\n'))) {
                const qsizetype colon = line.indexOf(QLatin1Char(':'));
                if (colon <= 0)
                    continue;
                const QString key = line.left(colon).trimmed().toLower();
                const QString value = line.mid(colon + 1).trimmed();
                if (key == QLatin1String("title"))
                    d.title = yaml_unquote(value);
                else if (key == QLatin1String("category"))
                    d.category = yaml_unquote(value);
                else if (key == QLatin1String("priority"))
                    d.priority = yaml_unquote(value);
                else if (key == QLatin1String("sentiment"))
                    d.sentiment = yaml_unquote(value);
                else if (key == QLatin1String("tags"))
                    d.tags = yaml_list(value);
                else if (key == QLatin1String("tickers"))
                    d.tickers = yaml_list(value);
                else if (key == QLatin1String("pinned") || key == QLatin1String("favorite"))
                    d.pinned = yaml_unquote(value).toLower() == QLatin1String("true");
                else if (key == QLatin1String("archived"))
                    d.archived = yaml_unquote(value).toLower() == QLatin1String("true");
                else if (key == QLatin1String("created"))
                    d.created = yaml_unquote(value);
                else if (key == QLatin1String("updated"))
                    d.updated = yaml_unquote(value);
            }
            body = t.mid(after);
        }
    }
    // Leading blank lines, then a "# heading" that is (or supplies) the title.
    qsizetype start = 0;
    while (start < body.size() && body.at(start) == QLatin1Char('\n'))
        ++start;
    if (body.mid(start, 2) == QLatin1String("# ")) {
        qsizetype eol = body.indexOf(QLatin1Char('\n'), start);
        if (eol < 0)
            eol = body.size();
        const QString heading = body.mid(start + 2, eol - start - 2).trimmed();
        if (d.title.isEmpty() || heading == d.title.trimmed()) {
            if (d.title.isEmpty())
                d.title = heading;
            body = body.mid(std::min(eol + 1, body.size()));
            if (body.startsWith(QLatin1Char('\n')))
                body.remove(0, 1);
            start = -1;
        }
    }
    if (start > 0)
        body = body.mid(start);
    if (body.endsWith(QLatin1Char('\n')))
        body.chop(1);
    d.body = body;
    if (d.title.trimmed().isEmpty())
        d.title = fallback_title.trimmed();
    return d;
}

/// A file name stem for a note title: path separators and other characters file
/// systems reject replaced, whitespace collapsed, at most 80 characters.
inline QString safe_file_stem(const QString& title) {
    static const QRegularExpression bad(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]"));
    QString s = title;
    s.replace(bad, QStringLiteral("_"));
    s = s.simplified().left(80).trimmed();
    while (s.startsWith(QLatin1Char('.')))
        s.remove(0, 1);
    return s.isEmpty() ? QStringLiteral("note") : s;
}

} // namespace fincept::screens::notes
