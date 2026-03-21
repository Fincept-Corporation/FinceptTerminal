#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QMap>
#include <QVector>
#include "services/news/NewsService.h"

namespace fincept::screens {

/// Left sidebar (260px) — sentiment gauge, top stories, categories, feed stats, monitors.
class NewsSidebarPanel : public QWidget {
    Q_OBJECT
public:
    explicit NewsSidebarPanel(QWidget* parent = nullptr);

    void update_data(const QVector<fincept::services::NewsArticle>& articles,
                     const QMap<QString, int>& category_counts,
                     int bullish, int bearish, int neutral,
                     int feed_count, int source_count, int cluster_count);

signals:
    void category_clicked(const QString& category);
    void article_clicked(const fincept::services::NewsArticle& article);

private:
    QWidget* build_sentiment_gauge();
    QWidget* build_top_stories();
    QWidget* build_categories();
    QWidget* build_feed_stats();

    // Sentiment gauge
    QWidget* bull_bar_ = nullptr;
    QWidget* bear_bar_ = nullptr;
    QWidget* neut_bar_ = nullptr;
    QLabel*  sent_score_label_ = nullptr;
    QLabel*  bull_label_ = nullptr;
    QLabel*  bear_label_ = nullptr;
    QLabel*  neut_label_ = nullptr;

    // Top stories
    QVBoxLayout* stories_layout_ = nullptr;

    // Categories
    QVBoxLayout* categories_layout_ = nullptr;

    // Feed stats
    QLabel* feeds_label_    = nullptr;
    QLabel* articles_label_ = nullptr;
    QLabel* clusters_label_ = nullptr;
    QLabel* sources_label_  = nullptr;

    QVector<fincept::services::NewsArticle> current_articles_;
};

} // namespace fincept::screens
