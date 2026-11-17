// NewsAnalytics.cpp — see NewsAnalytics.h.

#include "services/news/NewsAnalytics.h"

#include <QHash>

#include <algorithm>
#include <cmath>

namespace fincept::services::news_analytics {

namespace {

bool na_is_cjk_like(char32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x3040 && cp <= 0x30FF) ||
           (cp >= 0xAC00 && cp <= 0xD7AF) || (cp >= 0x0E00 && cp <= 0x0E7F) || (cp >= 0xF900 && cp <= 0xFAFF);
}

const QSet<QString>& na_stop_words() {
    static const QSet<QString> words = {
        "the",  "and",  "for",  "with", "from", "that", "this", "after", "over", "into", "amid", "says", "said",
        "will", "have", "has",  "are",  "was",  "were", "its",  "their", "they", "but",  "not",  "new",  "how",
        "why",  "what", "who",  "when", "than", "more", "about", "could", "would", "may", "can",  "out",  "all",
        "des",  "les",  "une",  "pour", "dans", "sur",  "avec", "der",  "die",  "das",  "und",  "mit",  "für",
        "los",  "las",  "del",  "por",  "para", "con",  "una",  "dos",  "das",  "uma",  "per",  "della", "het",
        "een",  "van",  "voor", "live", "video", "update", "watch", "breaking", "exclusive",
    };
    return words;
}

} // namespace

QString velocity_label(Velocity v) {
    switch (v) {
        case Velocity::Rising:
            return QStringLiteral("rising");
        case Velocity::Falling:
            return QStringLiteral("falling");
        case Velocity::Stable:
            break;
    }
    return QStringLiteral("stable");
}

Velocity cluster_velocity(const QVector<qint64>& timestamps, qint64 now) {
    int recent = 0;   // (now - 2h, now]
    int previous = 0; // (now - 8h, now - 2h]
    for (const qint64 ts : timestamps) {
        const qint64 age = now - ts;
        if (age < 0 || age <= 2 * 3600)
            ++recent;
        else if (age <= 8 * 3600)
            ++previous;
    }
    const double recent_rate = recent / 2.0;     // per hour
    const double previous_rate = previous / 6.0; // per hour
    if (recent >= 2 && (previous == 0 || recent_rate > previous_rate * 1.5))
        return Velocity::Rising;
    if (previous >= 2 && recent_rate < previous_rate * 0.5)
        return Velocity::Falling;
    return Velocity::Stable;
}

QSet<QString> headline_tokens(const QString& headline) {
    QSet<QString> tokens;
    const QString text = headline.toLower();
    QString word;
    QString run; // current CJK-like run
    auto flush_word = [&]() {
        if (word.size() > 2 && !na_stop_words().contains(word))
            tokens.insert(word);
        word.clear();
    };
    auto flush_run = [&]() {
        if (run.size() == 1) {
            tokens.insert(run);
        } else {
            for (qsizetype i = 0; i + 1 < run.size(); ++i)
                tokens.insert(run.mid(i, 2));
        }
        run.clear();
    };
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        const char32_t cp = ch.unicode();
        if (na_is_cjk_like(cp)) {
            flush_word();
            run.append(ch);
            continue;
        }
        if (!run.isEmpty())
            flush_run();
        if (ch.isLetterOrNumber())
            word.append(ch);
        else
            flush_word();
    }
    flush_word();
    if (!run.isEmpty())
        flush_run();
    return tokens;
}

double jaccard(const QSet<QString>& a, const QSet<QString>& b) {
    if (a.isEmpty() && b.isEmpty())
        return 0.0;
    const QSet<QString>& small = a.size() <= b.size() ? a : b;
    const QSet<QString>& large = a.size() <= b.size() ? b : a;
    int inter = 0;
    for (const auto& t : small)
        if (large.contains(t))
            ++inter;
    const int uni = static_cast<int>(a.size() + b.size()) - inter;
    return uni > 0 ? static_cast<double>(inter) / uni : 0.0;
}

QString duplicate_key(const QString& headline) {
    QString out;
    out.reserve(headline.size());
    bool space = false;
    for (const QChar ch : headline.toLower()) {
        if (ch.isLetterOrNumber()) {
            if (space && !out.isEmpty())
                out.append(QLatin1Char(' '));
            out.append(ch);
            space = false;
        } else {
            space = true;
        }
    }
    return out;
}

Baseline baseline_of(const QVector<int>& counts) {
    Baseline b;
    b.samples = static_cast<int>(counts.size());
    if (counts.isEmpty())
        return b;
    double sum = 0;
    for (int c : counts)
        sum += c;
    b.mean = sum / counts.size();
    double var = 0;
    for (int c : counts)
        var += (c - b.mean) * (c - b.mean);
    b.stddev = std::sqrt(var / counts.size());
    return b;
}

double zscore(double current, const Baseline& b, int min_samples, double min_stddev) {
    if (b.samples < min_samples || b.stddev < min_stddev)
        return std::numeric_limits<double>::quiet_NaN();
    return (current - b.mean) / b.stddev;
}

qint64 bucket_seconds_for_span(qint64 span) {
    if (span <= 12 * 3600)
        return 15 * 60;
    if (span <= 3 * 86400)
        return 3600;
    if (span <= 14 * 86400)
        return 6 * 3600;
    return 86400;
}

QVector<SeriesPoint> bucketize(const QVector<Sample>& samples, qint64 from, qint64 to, qint64 width) {
    QVector<SeriesPoint> out;
    if (width <= 0 || to <= from)
        return out;
    // Align to whole buckets so a series does not shift every minute.
    const qint64 start = from - (from % width);
    const int n = static_cast<int>((to - start + width - 1) / width);
    if (n <= 0 || n > 20000)
        return out;
    out.resize(n);
    QVector<double> sums(n, 0.0);
    for (int i = 0; i < n; ++i)
        out[i].start = start + static_cast<qint64>(i) * width;
    for (const auto& s : samples) {
        if (s.ts < start || s.ts >= to)
            continue;
        const int i = static_cast<int>((s.ts - start) / width);
        if (i < 0 || i >= n)
            continue;
        auto& p = out[i];
        ++p.count;
        if (s.tone > 0)
            ++p.bullish;
        else if (s.tone < 0)
            ++p.bearish;
        if (s.scored) {
            ++p.scored;
            sums[i] += s.score;
        }
    }
    for (int i = 0; i < n; ++i)
        if (out[i].scored > 0)
            out[i].mean_score = sums[i] / out[i].scored;
    return out;
}

QVector<TrendRow> trending(const QVector<TrendInput>& inputs, qint64 now, qint64 window, int max_rows, int min_count,
                           int spark_buckets) {
    QHash<QString, TrendRow> rows;
    QHash<QString, double> sums;
    const qint64 cur_from = now - window;
    const qint64 prev_from = now - 2 * window;
    const int buckets = std::max(1, spark_buckets);
    const double bucket_w = static_cast<double>(2 * window) / buckets;
    for (const auto& in : inputs) {
        if (in.ts < prev_from || in.ts > now + 300)
            continue;
        const bool current = in.ts >= cur_from;
        QSet<QString> seen;
        for (const auto& key : in.keys) {
            if (key.isEmpty() || seen.contains(key))
                continue;
            seen.insert(key);
            auto& r = rows[key];
            if (r.key.isEmpty()) {
                r.key = key;
                r.spark = QVector<double>(buckets, 0.0);
            }
            const int bi = std::clamp(static_cast<int>((in.ts - prev_from) / bucket_w), 0, buckets - 1);
            r.spark[bi] += 1.0;
            if (current) {
                ++r.current;
                r.latest = std::max(r.latest, in.ts);
                if (in.scored) {
                    ++r.scored;
                    sums[key] += in.score;
                }
            } else {
                ++r.previous;
            }
        }
    }
    QVector<TrendRow> out;
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        if (it->current < min_count)
            continue;
        if (it->scored > 0)
            it->mean_score = sums.value(it.key()) / it->scored;
        out.append(*it);
    }
    std::sort(out.begin(), out.end(), [](const TrendRow& a, const TrendRow& b) {
        if (a.current != b.current)
            return a.current > b.current;
        const int da = a.current - a.previous;
        const int db = b.current - b.previous;
        if (da != db)
            return da > db;
        if (a.latest != b.latest)
            return a.latest > b.latest;
        return a.key < b.key;
    });
    if (max_rows > 0 && out.size() > max_rows)
        out.resize(max_rows);
    return out;
}

double change_pct(int current, int previous) {
    if (previous <= 0)
        return std::numeric_limits<double>::quiet_NaN();
    return (static_cast<double>(current) - previous) * 100.0 / previous;
}

} // namespace fincept::services::news_analytics
