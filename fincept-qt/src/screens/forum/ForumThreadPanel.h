// src/screens/forum/ForumThreadPanel.h
#pragma once
#include "services/forum/ForumModels.h"

#include <QEvent>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
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
    /// Replace the loading spinner with a "could not load this thread" state and
    /// a way back to the feed. set_loading(false) alone only stopped the spinner
    /// and left the user on a frozen "Loading thread..." page with no back button.
    void show_load_error();
    /// Disable the Reply button/composer while a reply is being posted so a
    /// double-click or double-Enter can't submit the same comment twice.
    void set_reply_busy(bool busy);
    /// Scroll the open thread to its last reply (after posting one).
    void scroll_to_end();
    /// Clear the reply composer. Called by ForumScreen only after a reply has
    /// actually posted, so a failed submit keeps the user's text.
    void clear_reply_input();
    void clear();
    QString reply_draft() const;
    void set_reply_draft(const QString& text);

  signals:
    void back_requested();
    void comment_submitted(const QString& post_uuid, const QString& content);
    void vote_post(const QString& post_uuid, const QString& vote_type);
    void vote_comment(const QString& comment_uuid, const QString& vote_type);
    void author_clicked(const QString& username);

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void build_ui();
    void rebuild_comments();
    void retranslateUi();

    // Static chrome (cached for retranslateUi)
    QLabel* loading_text_ = nullptr;
    QPushButton* back_btn_ = nullptr;
    QPushButton* up_btn_ = nullptr;
    QLabel* replies_hdr_ = nullptr;
    QPushButton* send_btn_ = nullptr;

    QStackedWidget* stack_ = nullptr; // 0=loading, 1=thread
    QWidget* thread_page_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QPushButton* load_back_btn_ = nullptr; // visible only in the load-error state
    bool reply_busy_ = false;
    bool spinning_ = false; // a load is in progress (spinner wanted while visible)

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
