#include "services/news/NewsClusterService.h"
#include <QSet>
#include <QDateTime>
#include <QUuid>
#include <algorithm>

namespace fincept::services {

// Tokenize headline into lowercase words for Jaccard similarity
static QSet<QString> tokenize(const QString& text) {
    QSet<QString> tokens;
    for (const auto& w : text.toLower().split(QRegularExpression("\\W+"), Qt::SkipEmptyParts)) {
        if (w.size() > 2) tokens.insert(w);
    }
    return tokens;
}

static double jaccard(const QSet<QString>& a, const QSet<QString>& b) {
    if (a.isEmpty() && b.isEmpty()) return 0;
    int intersection = 0;
    for (const auto& t : a) {
        if (b.contains(t)) intersection++;
    }
    int union_size = a.size() + b.size() - intersection;
    return union_size > 0 ? static_cast<double>(intersection) / union_size : 0;
}

QVector<NewsCluster> cluster_articles(const QVector<NewsArticle>& articles) {
    if (articles.isEmpty()) return {};

    constexpr double SIMILARITY_THRESHOLD = 0.25;
    constexpr int64_t TIME_WINDOW_SEC = 86400;  // 24h

    struct Entry {
        int article_idx;
        QSet<QString> tokens;
        int cluster_idx = -1;
    };

    QVector<Entry> entries;
    entries.reserve(articles.size());
    for (int i = 0; i < articles.size(); i++) {
        Entry e{i, tokenize(articles[i].headline), -1};
        entries.append(e);
    }

    QVector<QVector<int>> cluster_members;

    for (int i = 0; i < entries.size(); i++) {
        if (entries[i].cluster_idx >= 0) continue;

        int ci = cluster_members.size();
        cluster_members.append(QVector<int>{i});
        entries[i].cluster_idx = ci;

        for (int j = i + 1; j < entries.size(); j++) {
            if (entries[j].cluster_idx >= 0) continue;

            auto dt = std::abs(articles[i].sort_ts - articles[j].sort_ts);
            if (dt > TIME_WINDOW_SEC) continue;

            bool same_cat = articles[i].category == articles[j].category;
            double sim = jaccard(entries[i].tokens, entries[j].tokens);
            double threshold = same_cat ? SIMILARITY_THRESHOLD * 0.8 : SIMILARITY_THRESHOLD;

            if (sim >= threshold) {
                entries[j].cluster_idx = ci;
                cluster_members[ci].append(j);
            }
        }
    }

    auto now = QDateTime::currentSecsSinceEpoch();
    QVector<NewsCluster> result;
    result.reserve(cluster_members.size());

    for (const auto& members : cluster_members) {
        NewsCluster c;
        c.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        c.tier = 4;
        c.latest_sort_ts = 0;

        QSet<QString> sources;
        int bullish = 0, bearish = 0;

        for (int idx : members) {
            const auto& a = articles[idx];
            c.articles.append(a);
            sources.insert(a.source);
            if (a.sort_ts > c.latest_sort_ts) c.latest_sort_ts = a.sort_ts;
            if (a.tier < c.tier) c.tier = a.tier;
            if (a.priority == Priority::FLASH || a.priority == Priority::URGENT) c.is_breaking = true;
            if (a.sentiment == Sentiment::BULLISH) bullish++;
            if (a.sentiment == Sentiment::BEARISH) bearish++;
        }

        // Lead article: best tier then newest
        std::sort(c.articles.begin(), c.articles.end(), [](const NewsArticle& a, const NewsArticle& b) {
            if (a.tier != b.tier) return a.tier < b.tier;
            return a.sort_ts > b.sort_ts;
        });
        c.lead_article = c.articles.first();

        c.source_count = sources.size();
        c.category = c.lead_article.category;
        c.sentiment = bullish > bearish ? Sentiment::BULLISH : bearish > bullish ? Sentiment::BEARISH : Sentiment::NEUTRAL;

        // Velocity: articles in last 2h vs previous 6h
        int recent = 0, older = 0;
        for (const auto& a : c.articles) {
            if ((now - a.sort_ts) < 7200) recent++;
            else older++;
        }
        c.velocity = recent > older ? "rising" : recent == older ? "stable" : "falling";

        result.append(std::move(c));
    }

    // Sort clusters: breaking first, then by latest_sort_ts
    std::sort(result.begin(), result.end(), [](const NewsCluster& a, const NewsCluster& b) {
        if (a.is_breaking != b.is_breaking) return a.is_breaking;
        return a.latest_sort_ts > b.latest_sort_ts;
    });

    return result;
}

QVector<NewsCluster> get_breaking_clusters(const QVector<NewsCluster>& clusters) {
    QVector<NewsCluster> result;
    for (const auto& c : clusters) {
        if (c.is_breaking) result.append(c);
    }
    return result;
}

} // namespace fincept::services
