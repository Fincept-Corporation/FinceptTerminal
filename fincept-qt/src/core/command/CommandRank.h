#pragma once
// CommandRank — how the header search box ranks a row for what was typed. Pure (Qt Core,
// no QObject, no I/O); tests/tst_command_rank.cpp pins every rule.
//
// One sortable number per row: the MATCH TIER dominates, then quality inside the tier,
// the user's own use of the row (recency + frequency, pins) and the row's priority.
// Usage never lifts a weaker match over a better one: "exact ticker / exact title"
// always beats a prefix, a prefix beats words, words beat a typo-tolerant guess.
//
//   Exact          the whole title / an alias / a code (ticker, id) typed exactly,
//                  spacing and punctuation ignored ("alt investments", "AAPL", "bt")
//   Prefix         the start of the title or of a code ("equity re", "aap")
//   WordsInOrder   each typed word starts a title word, in order ("eq res")
//   WordsAnyOrder  each typed word starts a title / alias word ("research equity")
//   Substring      the title contains the typed text ("ernat")
//   Keyword        found through keywords ("hedge", "favorites")
//   Fuzzy          one or two typos per word (bounded edit distance with adjacent
//                  transposition; prefixes too): "wacthlist", "portfolo", "backtets"
//   Description    found only in the description / subtitle words

#include <QList>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace fincept::command::rank {

enum class Tier : int {
    None = 0,
    Description = 1,
    Fuzzy = 2,
    Keyword = 3,
    Substring = 4,
    WordsAnyOrder = 5,
    WordsInOrder = 6,
    Prefix = 7,
    Exact = 8,
};

/// A run of characters in the ORIGINAL title (for emphasis).
struct Span {
    int start = 0;
    int length = 0;
    bool operator==(const Span& o) const { return start == o.start && length == o.length; }
};

/// What a row can be found by.
struct Fields {
    QString title;         ///< shown text ("Equity Research", "Apple Inc.")
    QStringList codes;     ///< typed-exactly tokens: ticker, screen id, aliases ("AAPL", "equity_research", "er")
    QStringList keywords;  ///< synonyms
    QString description;   ///< subtitle / description words (weakest)
};

/// Fields normalised once (index build), reused for every keystroke.
struct Prepared {
    QString title;
    QStringList title_words;   ///< lower-case words of the title
    QList<Span> title_spans;   ///< where each title word sits in `title`
    QString title_norm;        ///< title_words joined by ' '
    QString title_compact;     ///< title_words joined without separator
    QString title_lower;       ///< title.toLower() (substring rule)
    QStringList codes;         ///< compact lower-case codes
    QStringList code_words;    ///< words of every code
    QStringList keyword_words; ///< words of every keyword
    QString keywords_lower;    ///< keywords joined by ' ' (substring rule)
    QStringList desc_words;
};

/// Lower-case words split at anything that is not a letter or digit (same rule as CommandMatch.h).
QStringList words(QStringView text);
/// The words of `text` with their positions in it.
QList<Span> word_spans(QStringView text);

Prepared prepare(const Fields& fields);

struct Query {
    QString raw;
    QString norm;          ///< lower-case words joined by ' '
    QStringList words;
    QString compact;       ///< words joined without separator
    bool all_caps = false; ///< typed in capitals (>= 2 letters, none lower case): reads as a ticker
    bool empty() const { return words.isEmpty(); }
};
Query prepare_query(const QString& text);

struct Match {
    Tier tier = Tier::None;
    int quality = 0;        ///< 0..999 inside the tier (coverage, closeness)
    bool on_code = false;   ///< matched a code / alias rather than the title
    QList<Span> highlight;  ///< title spans to emphasise
    bool matched() const { return tier != Tier::None; }
};
Match match(const Query& query, const Prepared& row);

/// Bounded optimal-string-alignment distance (insert, delete, substitute, swap of two adjacent
/// characters). Returns max_distance + 1 as soon as the distance must exceed max_distance.
int osa_distance(QStringView a, QStringView b, int max_distance);
/// Typos allowed in a typed word: 0 up to 3 characters, 1 up to 6, 2 from 7.
int typo_budget(int length);

/// The user's own history of one row.
struct Usage {
    int count = 0;
    qint64 last_used_ms = 0; ///< epoch ms; 0 = never
    bool pinned = false;
};
inline constexpr int kMaxUsageBoost = 600;
/// 0..kMaxUsageBoost: frequency (diminishing), recency (half-life 7 days), pinned.
int usage_boost(const Usage& usage, qint64 now_ms);

/// Width of one tier in the final score: quality (<= 999) + usage (<= 600) + priority (-100..100) * 10 fit.
inline constexpr qint64 kTierWidth = 10000;
/// The sortable score (higher first). 0 when not matched.
qint64 score(const Match& m, int usage_boost, int priority);
inline Tier tier_of(qint64 score) {
    return static_cast<Tier>(score / kTierWidth);
}

} // namespace fincept::command::rank
