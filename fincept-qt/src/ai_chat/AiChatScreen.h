#pragma once
// AiChatScreen.h — Fincept AI Terminal chat (Qt port of fincept-web chat-mode)

#include "ai_chat/LlmService.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace fincept::screens {

class AiChatScreen : public QWidget {
    Q_OBJECT
  public:
    explicit AiChatScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private slots:
    void on_send();
    void on_new_session();
    void on_session_selected(int row);
    void on_delete_session();
    void on_stream_chunk(const QString& chunk, bool done);
    void on_streaming_done(ai_chat::LlmResponse response);
    void on_provider_changed();
    void on_search_changed(const QString& text);
    void on_clock_tick();

  private:
    // ── Sidebar ──────────────────────────────────────────────────────────
    QWidget*        sidebar_            = nullptr;
    QLabel*         session_count_lbl_  = nullptr;
    QLineEdit*      search_edit_        = nullptr;
    QListWidget*    session_list_       = nullptr;
    QPushButton*    new_btn_            = nullptr;
    QPushButton*    delete_btn_         = nullptr;
    QLabel*         stats_msgs_lbl_     = nullptr;
    QLabel*         stats_tokens_lbl_   = nullptr;

    // ── Chat area ────────────────────────────────────────────────────────
    QWidget*        chat_widget_        = nullptr;
    QLabel*         hdr_status_online_  = nullptr;
    QLabel*         hdr_status_ai_      = nullptr;
    QLabel*         hdr_session_lbl_    = nullptr;
    QLabel*         hdr_msgs_lbl_       = nullptr;
    QLabel*         hdr_clock_lbl_      = nullptr;
    QTimer*         clock_timer_        = nullptr;
    QScrollArea*    scroll_area_        = nullptr;
    QWidget*        messages_container_ = nullptr;
    QVBoxLayout*    messages_layout_    = nullptr;
    QWidget*        suggestions_panel_  = nullptr;
    QPlainTextEdit* input_box_          = nullptr;
    QPushButton*    send_btn_           = nullptr;

    // ── State ────────────────────────────────────────────────────────────
    QString         active_session_id_;
    std::vector<ai_chat::ConversationMessage> history_;
    bool            streaming_          = false;
    QTextEdit*      streaming_bubble_   = nullptr;
    int             total_tokens_       = 0;
    int             total_messages_     = 0;

    // ── Build ────────────────────────────────────────────────────────────
    void build_ui();
    void build_sidebar();
    void build_chat_area();
    QWidget* build_header_bar();
    QWidget* build_suggestions();
    QWidget* build_input_area();

    // ── Data ─────────────────────────────────────────────────────────────
    void load_sessions();
    void load_messages(const QString& session_id);
    void create_new_session();
    void add_message_bubble(const QString& role, const QString& content,
                            const QString& timestamp = {});
    QTextEdit* add_streaming_bubble();
    void scroll_to_bottom();
    void set_input_enabled(bool enabled);
    void update_stats();
    void show_suggestions(bool show);
};

} // namespace fincept::screens
