#pragma once
#include "screens/chat_mode/ChatModeTypes.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

namespace fincept::chat_mode {

/// Left sidebar panel — session list, search, new/delete/rename.
class ChatSessionPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ChatSessionPanel(QWidget* parent = nullptr);

    void refresh_sessions();

  signals:
    void session_selected(const QString& uuid);
    void new_session_requested();
    void delete_session_requested(const QString& uuid);
    void rename_session_requested(const QString& uuid, const QString& current_title);
    void exit_chat_mode_requested();

  public slots:
    void set_active_session(const QString& uuid);
    void update_stats(const ChatStats& stats);

  private slots:
    void on_item_clicked(QListWidgetItem* item);
    void on_new_clicked();
    void on_delete_clicked();
    void on_rename_clicked();
    void on_search_changed(const QString& text);

  private:
    QLineEdit*   search_edit_   = nullptr;
    QListWidget* session_list_  = nullptr;
    QPushButton* new_btn_       = nullptr;
    QPushButton* delete_btn_    = nullptr;
    QPushButton* rename_btn_    = nullptr;
    QPushButton* exit_btn_      = nullptr;
    QLabel*      stats_lbl_     = nullptr;

    QString active_uuid_;
    QVector<ChatSession> sessions_;

    void build_ui();
    void apply_filter(const QString& text);
    void populate_list(const QVector<ChatSession>& sessions);
};

} // namespace fincept::chat_mode
