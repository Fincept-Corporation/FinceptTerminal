#pragma once
#include "screens/news/NewsFeedDelegate.h"
#include "screens/news/NewsFeedModel.h"
#include "services/news/NewsClusterService.h"
#include "services/news/NewsService.h"

#include <QLabel>
#include <QListView>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class NewsFeedPanel : public QWidget {
    Q_OBJECT
  public:
    explicit NewsFeedPanel(QWidget* parent = nullptr);

    NewsFeedModel* model() { return model_; }
    QListView* list_view() { return list_view_; }

    void show_breaking(const QVector<services::NewsCluster>& breaking_clusters);
    void clear_breaking();
    void set_loading(bool loading);
    void scroll_to(const QString& article_id);
    void set_selected(const QString& article_id);
    void select_next();
    void select_previous();

  signals:
    void article_clicked(const services::NewsArticle& article);
    void cluster_clicked(const services::NewsCluster& cluster);
    void near_bottom();

  private slots:
    void on_item_clicked(const QModelIndex& index);
    void check_scroll_position();

  private:
    void build_breaking_banner();
    void build_skeleton();
    void remove_skeleton();
    bool is_banner_duplicate(const QString& headline) const;

    QStackedWidget* stack_ = nullptr;
    QListView* list_view_ = nullptr;
    NewsFeedModel* model_ = nullptr;
    NewsFeedDelegate* delegate_ = nullptr;

    // Breaking banner
    QWidget* banner_widget_ = nullptr;
    QLabel* banner_tag_ = nullptr;
    QLabel* banner_headline_ = nullptr;
    QLabel* banner_source_ = nullptr;
    QTimer* banner_dismiss_timer_ = nullptr;

    struct BreakingEntry {
        QString headline_key;
        int64_t shown_at = 0;
    };
    QVector<BreakingEntry> recent_banners_;
    int64_t global_cooldown_until_ = 0;
    static constexpr int BANNER_DEDUP_WINDOW_SEC = 1800;    // 30 min
    static constexpr int BANNER_GLOBAL_COOLDOWN_SEC = 120;   // 2 min
    static constexpr int SOUND_COOLDOWN_SEC = 300;           // 5 min
    int64_t last_sound_at_ = 0;

    // Skeleton loading
    QWidget* skeleton_overlay_ = nullptr;
    QTimer* skeleton_anim_timer_ = nullptr;
    int skeleton_phase_ = 0;
    bool is_loading_ = false;
};

} // namespace fincept::screens
