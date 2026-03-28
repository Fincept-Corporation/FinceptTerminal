// src/screens/forum/ForumSidebarPanel.h
#pragma once
#include "services/forum/ForumModels.h"

#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

class ForumSidebarPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ForumSidebarPanel(QWidget* parent = nullptr);

    void set_stats(const services::ForumStats& stats);
    void set_categories(const QVector<services::ForumCategory>& cats);
    void set_active_category(int id);
    void set_loading(bool on);
    void set_my_profile(const services::ForumProfile& profile);

  signals:
    void category_selected(int id, const QString& name, const QString& color);
    void trending_clicked();
    void new_post_requested(int category_id);
    void search_requested(const QString& query);

  private:
    void build_ui();
    void rebuild_categories();
    void rebuild_contributors();

    // Brand
    QWidget* brand_accent_ = nullptr;

    // Profile card
    QLabel* avatar_lbl_ = nullptr;
    QLabel* profile_name_lbl_ = nullptr;
    QLabel* profile_sub_lbl_ = nullptr;
    QLabel* profile_rep_lbl_ = nullptr;

    // Search
    QLineEdit* search_input_ = nullptr;

    // Stats
    QLabel* stat_posts_val_ = nullptr;
    QLabel* stat_comments_val_ = nullptr;
    QLabel* stat_active_val_ = nullptr;

    // Categories
    QVBoxLayout* cat_layout_ = nullptr;
    QWidget* cat_container_ = nullptr;

    // Contributors
    QVBoxLayout* contrib_layout_ = nullptr;
    QWidget* contrib_container_ = nullptr;

    QVector<services::ForumCategory> categories_;
    QVector<services::ForumContributor> contributors_;
    services::ForumStats cached_stats_;
    int active_category_id_ = -1;
};

} // namespace fincept::screens
