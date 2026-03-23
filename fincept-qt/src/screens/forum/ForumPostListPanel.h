// src/screens/forum/ForumPostListPanel.h
#pragma once
#include "services/forum/ForumModels.h"

#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class ForumPostListPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ForumPostListPanel(QWidget* parent = nullptr);

    void set_posts(const services::ForumPostsPage& page, const QString& category_color = {});
    void set_loading(bool on);
    void set_header(const QString& title, const QString& color);
    void set_active_post(const QString& uuid);
    void clear();

  signals:
    void post_selected(const services::ForumPost& post);
    void load_more_requested(int page);

  private:
    void build_ui();
    void rebuild_feed();
    void show_skeleton();
    void pulse_skeleton();

    // Header bar
    QLabel*      channel_label_   = nullptr;  // "# general"
    QLabel*      count_label_     = nullptr;
    QWidget*     filter_bar_      = nullptr;

    // Feed
    QWidget*     feed_container_  = nullptr;
    QVBoxLayout* feed_layout_     = nullptr;
    QScrollArea* scroll_          = nullptr;

    // Skeleton
    QWidget*     skeleton_widget_ = nullptr;
    QTimer*      skeleton_timer_  = nullptr;

    services::ForumPostsPage current_page_;
    QString active_uuid_;
    QString category_color_;
};

} // namespace fincept::screens
