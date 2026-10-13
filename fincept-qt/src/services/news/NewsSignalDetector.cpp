// NewsSignalDetector.cpp — see the header for what is detected and why it is C++.
#include "services/news/NewsSignalDetector.h"

#include <QHash>
#include <QSet>
#include <QStringView>

#include <algorithm>
#include <cmath>

namespace fincept::services {

namespace news_signal_detector_detail {

struct KeywordCategory {
    const char* category;
    QVector<const char*> keywords;
};

// Same lists, same order as the original script (the order decides which category a
// keyword reports and how ties in the keyword ranking break).
const QVector<KeywordCategory>& keyword_categories() {
    static const QVector<KeywordCategory> kCategories = {
        {"conflict",
         {"war", "attack", "missile", "strike", "troops", "invasion", "airstrike", "bombing", "military", "combat",
          "weapon", "artillery", "drone", "casualties", "killed", "wounded"}},
        {"cyber",
         {"cyberattack", "hack", "breach", "ransomware", "malware", "vulnerability", "exploit", "data leak",
          "phishing"}},
        {"natural",
         {"earthquake", "tsunami", "hurricane", "typhoon", "flood", "wildfire", "drought", "tornado", "volcanic",
          "pandemic"}},
        {"market",
         {"crash", "selloff", "sell-off", "circuit breaker", "trading halt", "flash crash", "margin call", "bankruptcy",
          "default", "downgrade"}},
        {"regulatory",
         {"sanction", "tariff", "ban", "regulation", "antitrust", "investigation", "fine", "penalty", "embargo",
          "restriction"}},
    };
    return kCategories;
}

bool is_word_char(QChar c) {
    const char16_t u = c.unicode();
    return (u >= u'a' && u <= u'z') || (u >= u'0' && u <= u'9');
}

/// Whole-word match with simple inflections on lower-cased text — the equivalent of
/// the script's `(?<![a-z0-9])kw(?:s|es|ed|ing)?(?![a-z0-9])`. A bare substring test
/// counted "ban" in "bank" and "war" in "software".
bool contains_keyword(const QString& text, QLatin1String kw) {
    const QStringView view(text);
    auto boundary_at = [&view](qsizetype i) { return i >= view.size() || !is_word_char(view.at(i)); };
    qsizetype from = 0;
    while (from < view.size()) {
        const qsizetype pos = view.indexOf(kw, from);
        if (pos < 0)
            return false;
        from = pos + 1;
        if (pos > 0 && is_word_char(view.at(pos - 1)))
            continue;
        const qsizetype end = pos + kw.size();
        if (boundary_at(end))
            return true;
        for (const char* suffix : {"s", "es", "ed", "ing"}) {
            const QLatin1String sfx(suffix);
            if (view.mid(end).startsWith(sfx) && boundary_at(end + sfx.size()))
                return true;
        }
    }
    return false;
}

/// Insertion-ordered counter (Python's Counter keeps first-seen order for ties).
struct OrderedCounter {
    QVector<QString> order;
    QHash<QString, int> counts;
    void add(const QString& key) {
        auto it = counts.find(key);
        if (it == counts.end()) {
            order.append(key);
            counts.insert(key, 1);
        } else {
            ++it.value();
        }
    }
    int value(const QString& key) const { return counts.value(key, 0); }
};

int severity_rank(const QString& severity) {
    if (severity == QLatin1String("critical"))
        return 0;
    if (severity == QLatin1String("high"))
        return 1;
    if (severity == QLatin1String("medium"))
        return 2;
    return 3;
}

} // namespace news_signal_detector_detail

QVector<CorrelationSignal> detect_news_signals(const QVector<NewsSignalInput>& items, qint64 now_secs) {
    using namespace news_signal_detector_detail;

    const qint64 hour_ago = now_secs - 3600;
    const qint64 two_hours_ago = now_secs - 7200;

    OrderedCounter recent_counts;
    OrderedCounter older_counts;
    OrderedCounter keyword_counts;
    QVector<QString> headline_order;
    QHash<QString, QSet<QString>> sources_per_headline;

    for (const auto& item : items) {
        const QString category = item.category.isEmpty() ? QStringLiteral("GENERAL") : item.category;
        if (item.sort_ts >= hour_ago)
            recent_counts.add(category);
        else if (item.sort_ts >= two_hours_ago)
            older_counts.add(category);

        const QString headline = item.headline.toLower();
        const QString key = headline.left(50);
        auto it = sources_per_headline.find(key);
        if (it == sources_per_headline.end()) {
            headline_order.append(key);
            it = sources_per_headline.insert(key, {});
        }
        it->insert(item.source);

        for (const auto& cat : keyword_categories())
            for (const char* kw : cat.keywords)
                if (contains_keyword(headline, QLatin1String(kw)))
                    keyword_counts.add(QString::fromLatin1(kw));
    }

    QVector<CorrelationSignal> out;

    // 1. velocity_spike — a category running at 3x+ the previous hour's volume.
    for (const QString& cat : std::as_const(recent_counts.order)) {
        const int recent = recent_counts.value(cat);
        const int older_actual = older_counts.value(cat);
        const double ratio = static_cast<double>(recent) / std::max(older_actual, 1);
        if (ratio >= 3.0 && recent >= 5) {
            CorrelationSignal s;
            s.type = QStringLiteral("velocity_spike");
            s.category = cat;
            s.detail = QStringLiteral("%1: %2 articles in 1h vs %3 in prior hour (%4x)")
                           .arg(cat)
                           .arg(recent)
                           .arg(older_actual)
                           .arg(ratio, 0, 'f', 1);
            s.severity = ratio >= 5.0 ? QStringLiteral("high") : QStringLiteral("medium");
            s.value = std::round(ratio * 10.0) / 10.0;
            out.append(s);
        }
    }

    // 2. triangulation — the same story carried by 3+ sources.
    for (const QString& key : std::as_const(headline_order)) {
        const QSet<QString>& sources = sources_per_headline[key];
        if (sources.size() < 3)
            continue;
        QStringList sorted(sources.cbegin(), sources.cend());
        sorted.sort(Qt::CaseInsensitive);
        CorrelationSignal s;
        s.type = QStringLiteral("triangulation");
        s.detail = QStringLiteral("'%1...' reported by %2 sources").arg(key.left(40)).arg(sources.size());
        s.severity = QStringLiteral("medium");
        s.sources = sorted.mid(0, 5);
        s.value = sources.size();
        out.append(s);
    }

    // 3. keyword_spike — the ten most frequent threat keywords, if mentioned 5+ times.
    QVector<QString> ranked = keyword_counts.order;
    std::stable_sort(ranked.begin(), ranked.end(), [&keyword_counts](const QString& a, const QString& b) {
        return keyword_counts.value(a) > keyword_counts.value(b);
    });
    for (int i = 0; i < std::min<qsizetype>(10, ranked.size()); ++i) {
        const QString& kw = ranked[i];
        const int count = keyword_counts.value(kw);
        if (count < 5)
            continue;
        QString kw_category = QStringLiteral("general");
        for (const auto& cat : keyword_categories()) {
            if (std::any_of(cat.keywords.cbegin(), cat.keywords.cend(),
                            [&kw](const char* k) { return kw == QLatin1String(k); })) {
                kw_category = QString::fromLatin1(cat.category);
                break;
            }
        }
        CorrelationSignal s;
        s.type = QStringLiteral("keyword_spike");
        s.category = kw_category;
        s.detail = QStringLiteral("'%1' mentioned %2 times in current feed").arg(kw).arg(count);
        s.severity = count >= 10 ? QStringLiteral("high") : QStringLiteral("medium");
        s.value = count;
        out.append(s);
    }

    // 4. hotspot_escalation — conflict vocabulary surging across the feed.
    int conflict_count = 0;
    for (const char* kw : keyword_categories().first().keywords)
        conflict_count += keyword_counts.value(QString::fromLatin1(kw));
    if (conflict_count >= 10) {
        CorrelationSignal s;
        s.type = QStringLiteral("hotspot_escalation");
        s.detail = QStringLiteral("%1 conflict-related mentions in feed").arg(conflict_count);
        s.severity = conflict_count >= 20 ? QStringLiteral("critical") : QStringLiteral("high");
        s.value = conflict_count;
        out.append(s);
    }

    // 5. military_surge.
    int mil_count = 0;
    for (const char* kw : {"military", "troops", "missile", "weapon", "artillery", "drone"})
        mil_count += keyword_counts.value(QString::fromLatin1(kw));
    if (mil_count >= 8) {
        CorrelationSignal s;
        s.type = QStringLiteral("military_surge");
        s.detail = QStringLiteral("%1 military-related mentions").arg(mil_count);
        s.severity = QStringLiteral("high");
        s.value = mil_count;
        out.append(s);
    }

    std::stable_sort(out.begin(), out.end(), [](const CorrelationSignal& a, const CorrelationSignal& b) {
        return severity_rank(a.severity) < severity_rank(b.severity);
    });
    return out;
}

} // namespace fincept::services
