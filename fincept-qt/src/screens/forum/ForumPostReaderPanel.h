// src/screens/forum/ForumPostReaderPanel.h
#pragma once
#include "services/forum/ForumModels.h"

#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class ForumPostReaderPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ForumPostReaderPanel(QWidget* parent = nullptr);

    void show_post(const services::ForumPostDetail& detail);
    void set_loading(bool on);
    void clear();

  signals:
    void comment_submitted(const QString& post_uuid, const QString& content);
    void vote_post(const QString& post_uuid, const QString& vote_type);
    void vote_comment(const QString& comment_uuid, const QString& vote_type);
    void author_clicked(const QString& username);

  private:
    void build_ui();
    void rebuild_comments();

    QStackedWidget* stack_ = nullptr; // 0=empty, 1=loading, 2=content
    QWidget* content_page_ = nullptr;

    // Post header
    QLabel* category_chip_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* author_label_ = nullptr;
    QLabel* meta_label_ = nullptr;

    // Post body
    QLabel* body_label_ = nullptr;

    // Engagement bar
    QWidget* vote_up_btn_ = nullptr;
    QLabel* likes_label_ = nullptr;
    QLabel* replies_label_ = nullptr;
    QLabel* views_label_ = nullptr;

    // Comments
    QWidget* comments_area_ = nullptr;
    QVBoxLayout* comments_layout_ = nullptr;

    // Reply input
    QLineEdit* reply_input_ = nullptr;

    // Loading spinner
    QLabel* spin_label_ = nullptr;
    QTimer* spin_timer_ = nullptr;
    int spin_frame_ = 0;

    services::ForumPostDetail current_detail_;
};

} // namespace fincept::screens
