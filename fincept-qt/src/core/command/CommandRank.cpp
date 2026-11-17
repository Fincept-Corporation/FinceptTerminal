// CommandRank.cpp — see CommandRank.h.
#include "core/command/CommandRank.h"

#include <QVarLengthArray>

#include <algorithm>
#include <cmath>

namespace fincept::command::rank {

namespace {

bool crk_is_word_char(QChar c) {
    return c.isLetterOrNumber();
}

/// Every query word is the start of a DIFFERENT field word, in order; `hits` gets the index of each.
bool crk_prefixes_in_order(const QStringList& query, const QStringList& field, QList<int>* hits) {
    int at = 0;
    for (const QString& q : query) {
        bool found = false;
        while (at < field.size()) {
            const int i = at++;
            if (field[i].startsWith(q)) {
                if (hits)
                    hits->append(i);
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

/// Every query word starts some word of `field` (any order).
bool crk_prefixes_anywhere(const QStringList& query, const QStringList& field) {
    for (const QString& q : query) {
        bool found = false;
        for (const QString& w : field) {
            if (w.startsWith(q)) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

/// Best (lowest) typo distance of `q` against `w`: the whole word, or the start of a longer word
/// (typing in progress: "portfo" vs "portfolio", "wacth" vs "watchlist").
int crk_word_distance(const QString& q, const QString& w, int budget) {
    if (w.startsWith(q))
        return 0;
    int best = budget + 1;
    best = std::min(best, osa_distance(q, w, budget));
    if (w.size() > q.size()) {
        // Compare against the word's start at the typed length and one either side (a missed or doubled letter).
        for (int len = std::max(1, static_cast<int>(q.size()) - 1);
             len <= std::min(static_cast<int>(w.size()), static_cast<int>(q.size()) + 1); ++len)
            best = std::min(best, osa_distance(q, QStringView(w).left(len), budget));
    }
    return best;
}

int crk_coverage(int typed, int total) {
    if (total <= 0)
        return 0;
    return std::clamp(typed * 400 / total, 0, 400);
}

void crk_add_word_highlights(const QStringList& q_words, const QList<int>& hit_index, const Prepared& row,
                             QList<Span>* out) {
    for (int i = 0; i < hit_index.size() && i < q_words.size(); ++i) {
        const int wi = hit_index[i];
        if (wi < 0 || wi >= row.title_spans.size())
            continue;
        const Span s = row.title_spans[wi];
        out->append({s.start, std::min(s.length, static_cast<int>(q_words[i].size()))});
    }
}

/// Spans covering the first `chars` word characters of the title, word by word.
QList<Span> crk_prefix_highlight(const Prepared& row, int chars) {
    QList<Span> out;
    for (int i = 0; i < row.title_spans.size() && chars > 0; ++i) {
        const Span s = row.title_spans[i];
        const int take = std::min(chars, s.length);
        out.append({s.start, take});
        chars -= take;
    }
    return out;
}

} // namespace

QStringList words(QStringView text) {
    QStringList out;
    QString cur;
    for (const QChar c : text) {
        if (crk_is_word_char(c)) {
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

QList<Span> word_spans(QStringView text) {
    QList<Span> out;
    int start = -1;
    for (int i = 0; i < text.size(); ++i) {
        if (crk_is_word_char(text[i])) {
            if (start < 0)
                start = i;
        } else if (start >= 0) {
            out.append({start, i - start});
            start = -1;
        }
    }
    if (start >= 0)
        out.append({start, static_cast<int>(text.size()) - start});
    return out;
}

Prepared prepare(const Fields& f) {
    Prepared p;
    p.title = f.title;
    p.title_words = words(f.title);
    p.title_spans = word_spans(f.title);
    p.title_norm = p.title_words.join(QLatin1Char(' '));
    p.title_compact = p.title_words.join(QString());
    p.title_lower = f.title.toLower();
    for (const QString& c : f.codes) {
        const QStringList cw = words(c);
        if (cw.isEmpty())
            continue;
        p.codes.append(cw.join(QString()));
        p.code_words += cw;
    }
    for (const QString& k : f.keywords)
        p.keyword_words += words(k);
    p.keywords_lower = f.keywords.join(QLatin1Char(' ')).toLower();
    p.desc_words = words(f.description);
    return p;
}

Query prepare_query(const QString& text) {
    Query q;
    q.raw = text;
    q.words = words(text);
    q.norm = q.words.join(QLatin1Char(' '));
    q.compact = q.words.join(QString());
    int letters = 0;
    bool lower = false;
    for (const QChar c : text) {
        if (c.isLetter()) {
            ++letters;
            if (c.isLower())
                lower = true;
        }
    }
    q.all_caps = letters >= 2 && !lower;
    return q;
}

int typo_budget(int length) {
    if (length <= 3)
        return 0;
    if (length <= 6)
        return 1;
    return 2;
}

int osa_distance(QStringView a, QStringView b, int max_distance) {
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    if (max_distance < 0)
        return 0;
    if (std::abs(n - m) > max_distance)
        return max_distance + 1;
    if (n == 0 || m == 0)
        return std::max(n, m);
    // Three rolling rows (OSA needs i-2 for the transposition).
    QVarLengthArray<int, 64> prev2(m + 1), prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j)
        prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        int row_min = cur[0];
        for (int j = 1; j <= m; ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int v = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
                v = std::min(v, prev2[j - 2] + 1);
            cur[j] = v;
            row_min = std::min(row_min, v);
        }
        if (row_min > max_distance)
            return max_distance + 1;
        std::swap(prev2, prev);
        std::swap(prev, cur);
    }
    return std::min(prev[m], max_distance + 1);
}

Match match(const Query& q, const Prepared& row) {
    Match m;
    if (q.words.isEmpty())
        return m;
    const int typed = static_cast<int>(q.compact.size());

    // ── Exact ──
    if (!row.title_norm.isEmpty() && (q.norm == row.title_norm || q.compact == row.title_compact)) {
        m.tier = Tier::Exact;
        m.quality = 900;
        m.highlight = crk_prefix_highlight(row, static_cast<int>(row.title_compact.size()));
        return m;
    }
    if (row.codes.contains(q.compact)) {
        m.tier = Tier::Exact;
        m.quality = 880;
        m.on_code = true;
        return m;
    }

    // ── Prefix ──
    if (!row.title_norm.isEmpty() && (row.title_norm.startsWith(q.norm) || row.title_compact.startsWith(q.compact))) {
        m.tier = Tier::Prefix;
        m.quality = 500 + crk_coverage(typed, static_cast<int>(row.title_compact.size()));
        m.highlight = crk_prefix_highlight(row, typed);
        return m;
    }
    for (const QString& c : row.codes) {
        if (typed >= 1 && c.startsWith(q.compact)) {
            m.tier = Tier::Prefix;
            m.quality = 300 + crk_coverage(typed, static_cast<int>(c.size()));
            m.on_code = true;
            return m;
        }
    }

    // ── Words ──
    QList<int> hits;
    if (crk_prefixes_in_order(q.words, row.title_words, &hits)) {
        m.tier = Tier::WordsInOrder;
        m.quality = 400 + crk_coverage(typed, static_cast<int>(row.title_compact.size()));
        crk_add_word_highlights(q.words, hits, row, &m.highlight);
        return m;
    }
    QStringList title_and_codes = row.title_words;
    title_and_codes += row.code_words;
    if (crk_prefixes_anywhere(q.words, title_and_codes)) {
        m.tier = Tier::WordsAnyOrder;
        m.quality = 300 + crk_coverage(typed, static_cast<int>(row.title_compact.size()));
        for (const QString& qw : q.words) {
            for (int i = 0; i < row.title_words.size(); ++i) {
                if (row.title_words[i].startsWith(qw) && i < row.title_spans.size()) {
                    m.highlight.append({row.title_spans[i].start,
                                        std::min(row.title_spans[i].length, static_cast<int>(qw.size()))});
                    break;
                }
            }
        }
        return m;
    }

    // ── Substring (inside the title) ──
    if (q.norm.size() >= 2) {
        const int at = static_cast<int>(row.title_lower.indexOf(q.norm));
        if (at >= 0) {
            m.tier = Tier::Substring;
            m.quality = 300 + crk_coverage(typed, static_cast<int>(row.title_compact.size()));
            m.highlight.append({at, static_cast<int>(q.norm.size())});
            return m;
        }
    }

    // ── Keyword ──
    QStringList all = title_and_codes;
    all += row.keyword_words;
    if ((!row.keyword_words.isEmpty() && crk_prefixes_anywhere(q.words, all)) ||
        (q.norm.size() >= 3 && row.keywords_lower.contains(q.norm))) {
        m.tier = Tier::Keyword;
        m.quality = 300;
        return m;
    }

    // ── Fuzzy (typos) — every typed word must land somewhere, at least one word long enough to allow a typo ──
    bool any_budget = false;
    int total_distance = 0;
    bool all_found = true;
    for (const QString& qw : q.words) {
        const int budget = typo_budget(static_cast<int>(qw.size()));
        any_budget = any_budget || budget > 0;
        int best = budget + 1;
        int best_word = -1;
        for (int i = 0; i < all.size() && best > 0; ++i) {
            const int d = crk_word_distance(qw, all[i], budget);
            if (d < best) {
                best = d;
                best_word = i;
            }
        }
        if (best > budget) {
            all_found = false;
            break;
        }
        total_distance += best;
        if (best_word >= 0 && best_word < row.title_spans.size() && best_word < row.title_words.size())
            m.highlight.append(row.title_spans[best_word]);
    }
    if (all_found && any_budget && total_distance > 0) {
        m.tier = Tier::Fuzzy;
        m.quality = std::max(0, 600 - 200 * total_distance);
        return m;
    }
    m.highlight.clear();

    // ── Description ──
    QStringList with_desc = all;
    with_desc += row.desc_words;
    if (!row.desc_words.isEmpty() && crk_prefixes_anywhere(q.words, with_desc)) {
        m.tier = Tier::Description;
        m.quality = 200;
        return m;
    }
    return Match{};
}

int usage_boost(const Usage& u, qint64 now_ms) {
    int boost = 0;
    if (u.count > 0) {
        // Frequency with diminishing returns: 1 use 60, 3 uses ~120, 10 uses ~200, capped at 250.
        boost += std::min(250, static_cast<int>(std::lround(60.0 * std::log2(1.0 + u.count) / std::log2(2.0))));
    }
    if (u.last_used_ms > 0 && now_ms >= u.last_used_ms) {
        const double days = static_cast<double>(now_ms - u.last_used_ms) / 86400000.0;
        boost += static_cast<int>(std::lround(250.0 * std::pow(0.5, days / 7.0)));
    }
    if (u.pinned)
        boost += 100;
    return std::clamp(boost, 0, kMaxUsageBoost);
}

qint64 score(const Match& m, int usage, int priority) {
    if (!m.matched())
        return 0;
    const int prio = std::clamp(priority, -100, 100);
    return static_cast<qint64>(m.tier) * kTierWidth + std::clamp(m.quality, 0, 999) +
           std::clamp(usage, 0, kMaxUsageBoost) + 1000 + prio * 10;
}

} // namespace fincept::command::rank
