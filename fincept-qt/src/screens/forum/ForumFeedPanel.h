// src/screens/forum/ForumFeedPanel.h
#pragma once
#include "services/forum/ForumModels.h"

#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class ForumFeedPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ForumFeedPanel(QWidget* parent = nullptr);

    void set_posts(const services::ForumPostsPage& page, const QString& cat_color = {});
    void set_loading(bool on);
    void set_active_post(const QString& uuid);
    void clear_active();
    void set_header(const QString& text);

    // Dashboard data
    void set_stats(const services::ForumStats& stats);
    void set_profile(const services::ForumProfile& profile);
    void set_categories(const QVector<services::ForumCategory>& cats, int active_id);

  signals:
    void post_selected(const services::ForumPost& post);
    void load_more_requested(int page);
    void category_clicked(int id, const QString& name, const QString& color);
    void profile_edit_requested();

  private:
    void build_ui();
    void rebuild_posts();
    void rebuild_stats();
    void rebuild_profile();
    void rebuild_categories();
    void rebuild_contributors();
    void show_skeleton();
    void pulse_skeleton();

    QScrollArea* scroll_ = nullptr;
    QWidget* content_w_ = nullptr;
    QVBoxLayout* content_vl_ = nullptr;

    // Dashboard sections
    QWidget* stats_section_ = nullptr;
    QWidget* profile_section_ = nullptr;
    QWidget* cats_section_ = nullptr;
    QWidget* contrib_section_ = nullptr;

    // Posts section
    QLabel* posts_header_lbl_ = nullptr;
    QLabel* posts_count_lbl_ = nullptr;
    QWidget* posts_container_ = nullptr;
    QVBoxLayout* posts_vl_ = nullptr;

    // Skeleton
    QWidget* skeleton_w_ = nullptr;
    QTimer* skeleton_timer_ = nullptr;

    // Data
    services::ForumPostsPage page_;
    services::ForumStats stats_;
    services::ForumProfile profile_;
    QVector<services::ForumCategory> categories_;
    int active_cat_id_ = 0;
    QString active_uuid_;
    QString cat_color_;
    bool stats_loaded_ = false;
    bool profile_loaded_ = false;
    bool cats_loaded_ = false;
};

} // namespace fincept::screens
