#pragma once
#include "core/symbol/IGroupLinked.h"
#include "screens/IStatefulScreen.h"
#include "services/news/NewsClusterService.h"
#include "services/news/NewsMonitorService.h"
#include "services/news/NewsService.h"

#include <QHBoxLayout>
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

/// Main news screen orchestrator — redesigned 2-panel layout.
///
/// Layout (top to bottom):
///   1. Command bar (32px) — search, category/time/sort/view pills, controls
///   2. Intel strip (26px) — live stats, sentiment, monitors, deviations
///   3. Content area — full-width feed | optional right detail overlay (420px)
///                      optional left intel drawer (280px, toggled)
///   4. Ticker strip (22px) — scrolling breaking headlines
class NewsScreen : public QWidget, public IStatefulScreen, public IGroupLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit NewsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "news"; }
    int state_version() const override { return 1; }

    // IGroupLinked — subscribe-only. When the group's active symbol
    // changes, the news feed's search query becomes that symbol so the
    // article list filters to mentions.
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override { return {}; }

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

    void on_drawer_toggle();
    void on_detail_closed();

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

    // Content area layout — holds feed + overlays
    QHBoxLayout* content_layout_ = nullptr;

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

    // Notification dedup
    QSet<QString> notified_breaking_;
    QSet<QString> notified_monitors_;
    QSet<QString> notified_deviations_;
    QSet<QString> notified_flash_;

    // Pulse animation timer
    QTimer* pulse_timer_ = nullptr;

    // Debounced DB seen-writes
    QTimer* seen_flush_timer_ = nullptr;
    QSet<QString> pending_seen_ids_;

    // Active variant for feed filtering
    QString active_variant_ = "FULL";

    // Symbol group link — SymbolGroup::None when unlinked.
    SymbolGroup link_group_ = SymbolGroup::None;
};

} // namespace fincept::screens
