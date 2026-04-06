#pragma once
#include "services/news/NewsClusterService.h"
#include "services/news/NewsMonitorService.h"
#include "services/news/NewsService.h"

#include <QHideEvent>
#include <QSet>
#include <QShowEvent>
#include <QWidget>

#include <atomic>

namespace fincept::screens {

class NewsCommandBar;
class NewsSidePanel;
class NewsFeedPanel;
class NewsDetailPanel;
class NewsTickerStrip;

/// Main news screen orchestrator. Owns all state, connects all panels,
/// manages the async lifecycle with generation IDs for stale result rejection.
class NewsScreen : public QWidget {
    Q_OBJECT
  public:
    explicit NewsScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_category_changed(const QString& category);
    void on_time_range_changed(const QString& range);
    void on_sort_changed(const QString& sort);
    void on_view_mode_changed(const QString& mode);
    void on_search_changed(const QString& query);
    void on_refresh();

    void on_article_clicked(const services::NewsArticle& article);
    void on_cluster_clicked(const services::NewsCluster& cluster);
    void on_near_bottom();

    void on_sidebar_category_clicked(const QString& category);
    void on_sidebar_article_clicked(const services::NewsArticle& article);
    void on_monitor_added(const QString& label, const QStringList& keywords);
    void on_monitor_toggled(const QString& id);
    void on_monitor_deleted(const QString& id);

    void on_analyze_requested(const QString& url);
    void on_related_clicked(const services::NewsArticle& article);

  private:
    void build_ui();
    void connect_signals();

    void refresh_data(bool force);
    void apply_filters_async();
    void update_ui_from_filtered(int generation, const QVector<services::NewsArticle>& filtered,
                                 const QVector<services::NewsCluster>& clusters,
                                 const QMap<QString, int>& category_counts, int bullish, int bearish, int neutral);
    void update_monitors();
    void compute_deviations();
    void sort_articles(QVector<services::NewsArticle>& articles) const;

    int64_t time_window_seconds() const;

    // Panels
    NewsCommandBar* command_bar_ = nullptr;
    NewsSidePanel* side_panel_ = nullptr;
    NewsFeedPanel* feed_panel_ = nullptr;
    NewsDetailPanel* detail_panel_ = nullptr;
    NewsTickerStrip* ticker_strip_ = nullptr;

    // State
    QVector<services::NewsArticle> all_articles_;
    QVector<services::NewsArticle> filtered_articles_;
    QVector<services::NewsCluster> clusters_;
    QString active_category_ = "ALL";
    QString time_range_ = "24H";
    QString sort_mode_ = "RELEVANCE";
    QString view_mode_ = "WIRE";
    QString search_query_;
    bool loading_ = false;
    bool visible_ = false;
    bool first_show_ = true;

    // Generation ID for stale async result rejection
    std::atomic<int> filter_generation_{0};

    // Lazy loading
    int visible_article_count_ = 50;
    static constexpr int PAGE_SIZE = 50;

    // Deviation baseline
    struct CategoryBaseline {
        double mean_count = 0;
        double stddev = 0;
        QVector<int> hourly_counts;
    };
    QMap<QString, CategoryBaseline> baselines_;

    // Notification dedup — track IDs already fired this session to avoid re-firing on every refresh
    QSet<QString> notified_breaking_;   // cluster lead article IDs
    QSet<QString> notified_monitors_;   // monitor_id + ":" + article_id pairs
    QSet<QString> notified_deviations_; // category keys that triggered deviation alert
    QSet<QString> notified_flash_;      // article IDs fired as FLASH/high-impact

    // Pulse animation timer
    QTimer* pulse_timer_ = nullptr;

    // Debounced DB seen-writes: collect IDs, flush every 1s
    QTimer* seen_flush_timer_ = nullptr;
    QSet<QString> pending_seen_ids_;

    // Active variant for feed filtering
    QString active_variant_ = "FULL";
};

} // namespace fincept::screens
