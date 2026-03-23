// src/screens/forum/ForumScreen.h
#pragma once
#include "services/forum/ForumModels.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens {

class ForumFeedPanel;
class ForumThreadPanel;

class ForumScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ForumScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_category_selected(int category_id, const QString& name, const QString& color);
    void on_post_selected(const services::ForumPost& post);
    void on_search(const QString& query);
    void on_trending();
    void on_new_post_requested();
    void navigate_back_to_feed();

  private:
    void build_ui();
    void build_toolbar();
    void load_initial_data();
    void rebuild_category_chips();
    void show_new_post_dialog(int category_id);
    void show_edit_profile_dialog(const services::ForumProfile& profile);

    // Toolbar
    QWidget*     toolbar_           = nullptr;
    QWidget*     chips_container_   = nullptr;
    QHBoxLayout* chips_layout_      = nullptr;
    QLineEdit*   search_input_      = nullptr;
    QLabel*      stat_posts_lbl_    = nullptr;
    QLabel*      stat_active_lbl_   = nullptr;
    QLabel*      profile_avatar_    = nullptr;

    // Main stacked view: 0=feed, 1=thread
    QStackedWidget* main_stack_     = nullptr;
    ForumFeedPanel*   feed_         = nullptr;
    ForumThreadPanel* thread_       = nullptr;

    // State
    QVector<services::ForumCategory> categories_;
    int     active_category_id_   = 0;
    QString active_category_name_;
    QString active_category_color_;
    QString current_detail_uuid_;
    bool    initial_load_done_    = false;
};

} // namespace fincept::screens
