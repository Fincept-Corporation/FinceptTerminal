#include "services/news/NewsClusterService.h"

#include "services/news/NewsAnalytics.h"

#include <QDateTime>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <numeric>

namespace fincept::services {

namespace {

constexpr double kNewsClusterSimilarity = 0.25;  // Jaccard over headline tokens
constexpr int64_t kNewsClusterWindowSec = 86400; // a story's members span at most 24 h
constexpr int kNewsClusterCompareMembers = 4;    // seed + the 3 most recent members

struct NcsCluster {
    QVector<int> members;          // article indexes, chronological
    QVector<QSet<QString>> sample; // token sets compared against (seed + recent)
    int64_t first_ts = 0;
    int64_t last_ts = 0;
    QString category;
};

} // namespace

QVector<NewsCluster> cluster_articles(const QVector<NewsArticle>& articles) {
    if (articles.isEmpty())
        return {};

    // Chronological pass: a story grows from its first report, so the cluster
    // id (derived from the earliest member) and membership stay stable from one
    // refresh to the next instead of depending on the relevance order.
    QVector<int> order(articles.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&articles](int a, int b) {
        if (articles[a].sort_ts != articles[b].sort_ts)
            return articles[a].sort_ts < articles[b].sort_ts;
        return articles[a].id < articles[b].id;
    });

    QVector<QSet<QString>> tokens(articles.size());
    for (int i = 0; i < articles.size(); ++i)
        tokens[i] = news_analytics::headline_tokens(articles[i].headline);

    QVector<NcsCluster> clusters;
    QHash<QString, QVector<int>> by_token; // token -> clusters containing it (inverted index)

    for (const int idx : order) {
        const auto& a = articles[idx];
        const auto& t = tokens[idx];
        // Candidates: clusters sharing at least one token, still inside the window.
        QSet<int> candidates;
        for (const auto& tok : t) {
            const auto it = by_token.constFind(tok);
            if (it == by_token.constEnd())
                continue;
            for (int c : *it)
                candidates.insert(c);
        }
        int best = -1;
        double best_sim = 0.0;
        for (int c : std::as_const(candidates)) {
            const auto& cl = clusters[c];
            if (a.sort_ts - cl.first_ts > kNewsClusterWindowSec)
                continue;
            double sim = 0.0;
            for (const auto& s : cl.sample)
                sim = std::max(sim, news_analytics::jaccard(t, s));
            const double threshold = cl.category == a.category ? kNewsClusterSimilarity * 0.8 : kNewsClusterSimilarity;
            if (sim >= threshold && sim > best_sim) {
                best = c;
                best_sim = sim;
            }
        }
        if (best < 0) {
            NcsCluster cl;
            cl.first_ts = a.sort_ts;
            cl.category = a.category;
            clusters.append(cl);
            best = static_cast<int>(clusters.size()) - 1;
        }
        auto& cl = clusters[best];
        cl.members.append(idx);
        cl.last_ts = std::max(cl.last_ts, a.sort_ts);
        if (cl.sample.size() < kNewsClusterCompareMembers) {
            cl.sample.append(t);
        } else {
            // Keep the seed, rotate the recent members.
            cl.sample.remove(1);
            cl.sample.append(t);
        }
        for (const auto& tok : t) {
            auto& v = by_token[tok];
            if (v.isEmpty() || v.last() != best)
                v.append(best);
        }
    }

    const int64_t now = QDateTime::currentSecsSinceEpoch();
    QVector<NewsCluster> result;
    result.reserve(clusters.size());
    for (const auto& cl : std::as_const(clusters)) {
        NewsCluster c;
        // Stable id: the earliest report's article id.
        c.id = QStringLiteral("c") + articles[cl.members.first()].id;
        c.tier = 4;
        c.latest_sort_ts = 0;

        QSet<QString> sources;
        int bullish = 0, bearish = 0;
        QVector<qint64> stamps;
        stamps.reserve(cl.members.size());
        for (int idx : cl.members) {
            const auto& a = articles[idx];
            c.articles.append(a);
            sources.insert(a.source);
            stamps.append(a.sort_ts);
            c.latest_sort_ts = std::max<int64_t>(c.latest_sort_ts, a.sort_ts);
            c.tier = std::min(c.tier, a.tier);
            if (a.priority == Priority::FLASH || a.priority == Priority::URGENT)
                c.is_breaking = true;
            if (a.sentiment == Sentiment::BULLISH)
                bullish++;
            if (a.sentiment == Sentiment::BEARISH)
                bearish++;
        }

        // Lead article: best tier then newest
        std::sort(c.articles.begin(), c.articles.end(), [](const NewsArticle& a, const NewsArticle& b) {
            if (a.tier != b.tier)
                return a.tier < b.tier;
            return a.sort_ts > b.sort_ts;
        });
        c.lead_article = c.articles.first();

        c.source_count = sources.size();
        c.category = c.lead_article.category;
        c.sentiment = bullish > bearish   ? Sentiment::BULLISH
                      : bearish > bullish ? Sentiment::BEARISH
                                          : Sentiment::NEUTRAL;
        // Momentum: last 2 h against the 6 h before that (articles per hour).
        c.velocity = news_analytics::velocity_label(news_analytics::cluster_velocity(stamps, now));

        result.append(std::move(c));
    }

    // Same order as every other news list: newest first (a story's time is its
    // latest report), ties by id so the order is stable across refreshes. The
    // breaking ones are picked out by get_breaking_clusters(), in this order.
    // (Breaking-first used to float a day-old "breaking" story above fresher news.)
    std::sort(result.begin(), result.end(), [](const NewsCluster& a, const NewsCluster& b) {
        if (a.latest_sort_ts != b.latest_sort_ts)
            return a.latest_sort_ts > b.latest_sort_ts;
        return a.id < b.id;
    });

    return result;
}

QVector<NewsCluster> get_breaking_clusters(const QVector<NewsCluster>& clusters) {
    QVector<NewsCluster> result;
    for (const auto& c : clusters) {
        if (c.is_breaking)
            result.append(c);
    }
    return result;
}

} // namespace fincept::services
