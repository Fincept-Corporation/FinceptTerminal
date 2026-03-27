#pragma once

#include "ai_chat/LlmService.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
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
    void hideEvent(QHideEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* e) override;

  private slots:
    void on_send();
    void on_attach_file();
    void on_new_session();
    void on_session_selected(int row);
    void on_delete_session();
    void on_rename_session();
    void on_stream_chunk(const QString& chunk, bool done);
    void on_streaming_done(ai_chat::LlmResponse response);
    void on_provider_changed();
    void on_search_changed(const QString& text);
    void on_typing_indicator_tick();

  private:
    // ── Sidebar ──────────────────────────────────────────────────────────
    QWidget*      sidebar_       = nullptr;
    QLineEdit*    search_edit_   = nullptr;
    QListWidget*  session_list_  = nullptr;
    QPushButton*  new_btn_       = nullptr;
    QPushButton*  delete_btn_    = nullptr;
    QPushButton*  rename_btn_    = nullptr;
    QLabel*       provider_lbl_  = nullptr;
    QLabel*       model_lbl_     = nullptr;

    // ── Chat header ──────────────────────────────────────────────────────
    QWidget*      chat_widget_        = nullptr;
    QLabel*       hdr_status_dot_     = nullptr;
    QLabel*       hdr_status_lbl_     = nullptr;
    QLabel*       hdr_session_lbl_    = nullptr;
    QLabel*       hdr_model_lbl_      = nullptr;
    QLabel*       hdr_tokens_lbl_     = nullptr;

    // ── Chat area ────────────────────────────────────────────────────────
    QScrollArea*  scroll_area_        = nullptr;
    QWidget*      messages_container_ = nullptr;
    QVBoxLayout*  messages_layout_    = nullptr;
    QWidget*      welcome_panel_      = nullptr;
    QPlainTextEdit* input_box_        = nullptr;
    QPushButton*  send_btn_           = nullptr;
    QPushButton*  attach_btn_         = nullptr;
    QLabel*       attach_badge_       = nullptr;   // shows attached file name
    QString       attached_file_path_;

    // ── Typing indicator ─────────────────────────────────────────────────
    QWidget*      typing_indicator_   = nullptr;
    QLabel*       typing_dots_lbl_    = nullptr;
    QTimer*       typing_timer_       = nullptr;
    int           typing_step_        = 0;

    // ── State ────────────────────────────────────────────────────────────
    QString active_session_id_;
    QString active_session_title_;
    std::vector<ai_chat::ConversationMessage> history_;
    bool streaming_ = false;
    QPointer<QTextEdit> streaming_bubble_;
    int total_tokens_   = 0;
    int total_messages_ = 0;

    // ── Build ────────────────────────────────────────────────────────────
    void build_ui();
    void build_sidebar();
    void build_chat_area();
    QWidget* build_header_bar();
    QWidget* build_welcome();
    QWidget* build_input_area();
    QWidget* build_typing_indicator();

    // ── Data ─────────────────────────────────────────────────────────────
    void load_sessions();
    void load_messages(const QString& session_id);
    void create_new_session();
    void add_message_bubble(const QString& role, const QString& content, const QString& timestamp = {});
    QTextEdit* add_streaming_bubble();
    void clear_messages();
    void scroll_to_bottom();
    void set_input_enabled(bool enabled);
    void update_stats();
    void show_welcome(bool show);
    void show_typing(bool show);
};

} // namespace fincept::screens
