// src/screens/forum/ForumThreadPanel.h
#pragma once
#include "services/forum/ForumModels.h"

#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class ForumThreadPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ForumThreadPanel(QWidget* parent = nullptr);

    void show_post(const services::ForumPostDetail& detail);
    void set_loading(bool on);
    void clear();

  signals:
    void back_requested();
    void comment_submitted(const QString& post_uuid, const QString& content);
    void vote_post(const QString& post_uuid, const QString& vote_type);
    void vote_comment(const QString& comment_uuid, const QString& vote_type);
    void author_clicked(const QString& username);

  private:
    void build_ui();
    void rebuild_comments();

    QStackedWidget* stack_ = nullptr; // 0=loading, 1=thread
    QWidget* thread_page_ = nullptr;

    // Thread header
    QLabel* t_cat_chip_ = nullptr;
    QLabel* t_title_lbl_ = nullptr;
    QLabel* t_author_lbl_ = nullptr;
    QLabel* t_meta_lbl_ = nullptr;
    QLabel* t_body_lbl_ = nullptr;
    QLabel* t_likes_lbl_ = nullptr;
    QLabel* t_replies_lbl_ = nullptr;
    QLabel* t_views_lbl_ = nullptr;
    QWidget* t_comments_w_ = nullptr;
    QVBoxLayout* t_comments_vl_ = nullptr;
    QLineEdit* t_reply_input_ = nullptr;

    // Spinner
    QLabel* spin_lbl_ = nullptr;
    QTimer* spin_timer_ = nullptr;
    int spin_frame_ = 0;

    services::ForumPostDetail current_;
};

} // namespace fincept::screens
