#pragma once
#include "screens/chat_mode/ChatModeTypes.h"

#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QWidget>

namespace fincept::chat_mode {

/// Left sidebar panel — session list, search, new/delete/rename/export.
class ChatSessionPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ChatSessionPanel(QWidget* parent = nullptr);

    void refresh_sessions();
    /// Keyboard focus into the conversation list.
    void focus_list();
    /// The EXPORT action (every conversation as JSON, non-modal file dialog, written
    /// on a worker) — also reachable from Chat Mode's command bar.
    void export_conversations();

  protected:
    void changeEvent(QEvent* event) override;

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
    void on_export_clicked();
    void on_search_changed(const QString& text);
    void on_search_server();
    void on_load_more();

  private:
    QLineEdit* search_edit_ = nullptr;
    QListWidget* session_list_ = nullptr;
    QPushButton* more_btn_ = nullptr; // "load older conversations"
    QPushButton* new_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* rename_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QPushButton* exit_btn_ = nullptr;
    QLabel* stats_lbl_ = nullptr;
    QLabel* title_lbl_ = nullptr;

    QTimer* search_timer_ = nullptr; // debounce server search

    QString active_uuid_;
    QVector<ChatSession> sessions_;
    // Sessions that contain a message matching the current server-side search; they are
    // listed (and highlighted) even when their TITLE doesn't match the typed text.
    QSet<QString> search_hits_;
    ChatStats last_stats_{};

    // How many of the newest conversations the sidebar shows (grows by a page per
    // "load older"). A refresh re-reads that window from the top. Servers that page
    // (`has_more` in the reply) get "load older" as one `offset` request for the next
    // page; older servers take only `limit`, so there it re-reads the larger window.
    int session_limit_ = 0;
    bool server_pages_ = false; // the last list reply carried `has_more`
    bool has_more_ = false;     // …and it said older conversations exist
    bool loading_more_ = false;
    int list_seq_ = 0; // only the newest list request may repopulate the sidebar

    /// Server-side cap on `limit` (apiback chat.py CHAT_SESSIONS_MAX_LIMIT).
    static constexpr int kMaxWindow = 1000;

    void build_ui();
    void retranslateUi();
    void apply_filter(const QString& text);
    void populate_list(const QVector<ChatSession>& sessions);
    void update_more_button();
};

} // namespace fincept::chat_mode
