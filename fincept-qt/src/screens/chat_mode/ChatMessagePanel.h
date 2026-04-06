#pragma once
#include "screens/chat_mode/ChatModeTypes.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::chat_mode {

/// Center panel — message history, SSE streaming display, input area.
class ChatMessagePanel : public QWidget {
    Q_OBJECT
  public:
    explicit ChatMessagePanel(QWidget* parent = nullptr);

    /// Load a session's messages into the view.
    void load_messages(const QVector<ChatMessage>& messages);

    /// Clear all messages and show welcome state.
    void clear_messages();

    /// Called when a new session is active (updates header title).
    void set_session_title(const QString& title);

    /// Update the mode selector label.
    void set_stream_mode(StreamMode mode);

    StreamMode current_mode() const { return current_mode_; }

  signals:
    void send_requested(const QString& message, StreamMode mode);
    void mode_toggled(StreamMode mode);

  public slots:
    // SSE event slots — connected to ChatModeService signals
    void on_stream_session_meta(const QString& session_id, const QString& new_title);
    void on_stream_text_delta(const QString& text);
    void on_stream_tool_end(const QString& tool_name, int duration_ms);
    void on_stream_finish(int total_tokens);
    void on_stream_error(const QString& message);
    void on_stream_heartbeat();

    void on_insufficient_credits();

  private slots:
    void on_send_clicked();
    void on_typing_tick();
    bool eventFilter(QObject* obj, QEvent* ev) override;

  private:
    // Header
    QLabel* hdr_title_lbl_   = nullptr;
    QLabel* hdr_mode_lbl_    = nullptr;
    QLabel* hdr_tokens_lbl_  = nullptr;
    QPushButton* mode_btn_   = nullptr;

    // Messages area
    QScrollArea* scroll_area_       = nullptr;
    QWidget*     messages_container_ = nullptr;
    QVBoxLayout* messages_layout_   = nullptr;
    QWidget*     welcome_panel_     = nullptr;

    // Typing indicator
    QWidget* typing_indicator_ = nullptr;
    QLabel*  typing_dots_lbl_  = nullptr;
    QTimer*  typing_timer_     = nullptr;
    int      typing_step_      = 0;

    // Input area
    QPlainTextEdit* input_box_  = nullptr;
    QPushButton*    send_btn_   = nullptr;
    QPushButton*    stop_btn_   = nullptr;
    QLabel*         char_lbl_   = nullptr;

    // State
    StreamMode            current_mode_      = StreamMode::Lite;
    bool                  streaming_         = false;
    QPointer<QTextEdit>   streaming_bubble_;
    QString               streaming_buffer_;   // accumulates raw markdown during stream
    int                   total_tokens_      = 0;

    void build_ui();
    QWidget* build_header();
    QWidget* build_welcome();
    QWidget* build_messages_area();
    QWidget* build_typing_indicator();
    QWidget* build_input_area();

    void add_message_bubble(const QString& role, const QString& content,
                            const QString& timestamp = {});
    QTextEdit* add_streaming_bubble();
    void append_tool_badge(const QString& tool_name, int duration_ms);

    void scroll_to_bottom();
    void show_welcome(bool show);
    void show_typing(bool show);
    void set_input_enabled(bool enabled);
    void update_tokens_label();
};

} // namespace fincept::chat_mode
