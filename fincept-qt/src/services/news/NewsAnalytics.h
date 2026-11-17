#pragma once
// NewsAnalytics — pure computations behind the News screen's analytics:
// cluster velocity, script-aware headline tokens for de-duplication, volume
// baselines (z-scores), time bucketing of volume / tone and trending keys with
// their change against the previous window.
//
// Plain value types only (no NewsArticle), so the logic is unit-testable with
// Qt Core alone. Thread-safe: no shared mutable state.

#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>

namespace fincept::services::news_analytics {

// ── Cluster velocity ─────────────────────────────────────────────────────────

enum class Velocity { Rising, Stable, Falling };

/// "rising" / "stable" / "falling" (the strings NewsCluster::velocity uses).
QString velocity_label(Velocity v);

/// Story momentum: the article rate of the last 2 h against the rate of the 6 h
/// before that (8 h .. 2 h ago). Rising needs at least two recent articles;
/// falling needs at least two in the earlier window.
Velocity cluster_velocity(const QVector<qint64>& timestamps, qint64 now);

// ── De-duplication tokens ────────────────────────────────────────────────────

/// Headline tokens for similarity: lower-case words longer than two letters
/// (stop words removed) for space-delimited scripts, character bigrams for
/// runs of CJK / kana / Hangul / Thai text (which has no word spaces).
QSet<QString> headline_tokens(const QString& headline);

/// |a ∩ b| / |a ∪ b|; 0 for two empty sets.
double jaccard(const QSet<QString>& a, const QSet<QString>& b);

/// Normalised headline for exact-duplicate detection (lower case, letters and
/// digits only, single spaces).
QString duplicate_key(const QString& headline);

// ── Volume baselines ─────────────────────────────────────────────────────────

struct Baseline {
    int samples = 0;
    double mean = 0.0;
    double stddev = 0.0;
};

/// Mean and (population) standard deviation of hourly counts.
Baseline baseline_of(const QVector<int>& counts);

/// z-score of `current` against `b`; NaN when the baseline has fewer than
/// `min_samples` hours or a standard deviation below `min_stddev` (a quiet
/// category with a flat history cannot be judged).
double zscore(double current, const Baseline& b, int min_samples = 24, double min_stddev = 0.5);

// ── Time series ──────────────────────────────────────────────────────────────

/// A sensible bucket width for a visible span: 15 min up to 12 h, 1 h up to
/// 3 days, 6 h up to 14 days, 1 day beyond.
qint64 bucket_seconds_for_span(qint64 span_secs);

struct Sample {
    qint64 ts = 0;        ///< unix seconds
    double score = 0.0;   ///< tone -1..1 (only meaningful when scored)
    bool scored = false;  ///< tone was measured
    int tone = 0;         ///< +1 bullish, -1 bearish, 0 neutral / unknown
};

struct SeriesPoint {
    qint64 start = 0; ///< bucket start, unix seconds
    int count = 0;    ///< articles
    int scored = 0;   ///< of which tone-scored
    double mean_score = std::numeric_limits<double>::quiet_NaN(); ///< NaN when none scored
    int bullish = 0;
    int bearish = 0;
};

/// Fixed-width buckets covering [from, to) — every bucket is present, empty
/// ones with count 0 and a NaN mean.
QVector<SeriesPoint> bucketize(const QVector<Sample>& samples, qint64 from, qint64 to, qint64 width);

// ── Trending ─────────────────────────────────────────────────────────────────

struct TrendInput {
    qint64 ts = 0;
    QStringList keys; ///< tickers / entities / categories the article counts for
    double score = 0.0;
    bool scored = false;
};

struct TrendRow {
    QString key;
    int current = 0;  ///< mentions in [now - window, now]
    int previous = 0; ///< mentions in [now - 2·window, now - window)
    int scored = 0;   ///< current mentions with a measured tone
    double mean_score = std::numeric_limits<double>::quiet_NaN(); ///< mean tone of those
    qint64 latest = 0;
    QVector<double> spark; ///< counts over the two windows, oldest first
};

/// Keys mentioned at least `min_count` times in the current window, most
/// mentioned first (ties: larger increase, then most recent), at most `max_rows`.
QVector<TrendRow> trending(const QVector<TrendInput>& inputs, qint64 now, qint64 window, int max_rows,
                           int min_count = 2, int spark_buckets = 8);

/// Relative change of `current` against `previous` in percent; NaN when there
/// was nothing before (a new topic).
double change_pct(int current, int previous);

} // namespace fincept::services::news_analytics
