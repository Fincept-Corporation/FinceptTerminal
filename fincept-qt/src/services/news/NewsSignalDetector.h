#pragma once
// NewsSignalDetector — feed-level correlation signals computed in-process.
//
// A C++ port of the heuristics that used to live in scripts/news_correlation.py
// (`detect_signals`). That script was spawned on every News filter change; the
// work is a few thousand substring checks over at most a few hundred headlines,
// so it now runs synchronously in well under a millisecond with no Python
// process at all. Depends on QtCore only (testable without the app).

#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services {

/// One feed-level signal (velocity spike, keyword spike, triangulation, ...).
struct CorrelationSignal {
    QString type; // velocity_spike, keyword_spike, triangulation, hotspot_escalation, military_surge
    QString category;
    QString detail;
    QString severity; // critical, high, medium, low
    double value = 0;
    QStringList sources;
};

/// The article fields the detector reads.
struct NewsSignalInput {
    QString category;
    QString headline;
    QString source;
    qint64 sort_ts = 0; // unix seconds
};

/// Detect signals over `items` as of `now_secs` (unix seconds). Pure function.
///
/// Signals (thresholds as in the original script):
///   velocity_spike     — a category has >= 5 articles in the last hour and >= 3x the
///                        previous hour's count (high at >= 5x)
///   triangulation      — the same headline (first 50 chars, case-folded) from >= 3 sources
///   keyword_spike      — a threat keyword in >= 5 headlines (top 10 keywords; high at >= 10)
///   hotspot_escalation — >= 10 conflict-keyword mentions (critical at >= 20)
///   military_surge     — >= 8 military-keyword mentions
/// Sorted by severity (critical, high, medium, low), stable within a severity.
///
/// The script's `geo_convergence` signal was dropped on purpose: it grouped by the
/// FEED's region ("GLOBAL", "US"), i.e. where the publisher sits, so it fired a
/// "high" signal on every refresh without saying anything about the news.
QVector<CorrelationSignal> detect_news_signals(const QVector<NewsSignalInput>& items, qint64 now_secs);

} // namespace fincept::services
