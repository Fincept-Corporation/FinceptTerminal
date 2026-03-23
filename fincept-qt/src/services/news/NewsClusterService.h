#pragma once
#include "services/news/NewsService.h"

#include <QMap>
#include <QString>
#include <QVector>

namespace fincept::services {

struct NewsCluster {
    QString id;
    NewsArticle lead_article;
    QVector<NewsArticle> articles;
    int source_count = 0;
    QString velocity; // "rising" / "stable" / "falling"
    Sentiment sentiment = Sentiment::NEUTRAL;
    QString category;
    int tier = 4;
    int64_t latest_sort_ts = 0;
    bool is_breaking = false;
};

QVector<NewsCluster> cluster_articles(const QVector<NewsArticle>& articles);
QVector<NewsCluster> get_breaking_clusters(const QVector<NewsCluster>& clusters);

} // namespace fincept::services
