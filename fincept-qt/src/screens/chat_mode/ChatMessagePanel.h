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

class ChatMessagePanel : public QWidget {
    Q_OBJECT
  public:
    explicit ChatMessagePanel(QWidget* parent = nullptr);

    void load_messages(const QVector<ChatMessage>& messages);
    void clear_messages();
    void set_session_title(const QString& title);
    void set_stream_mode(StreamMode mode);
    StreamMode current_mode() const { return current_mode_; }

    // Draft + scroll accessors used by ChatModeScreen for session persistence.
    QString draft_text() const;
    void set_draft_text(const QString& text);
    int scroll_position() const;
    void set_scroll_position(int pos);

  signals:
    void draft_changed();
    void scroll_changed();

  signals:
    void send_requested(const QString& message, StreamMode mode);
    void mode_toggled(StreamMode mode);

  public slots:
    void on_stream_session_meta(const QString& session_id, const QString& new_title);
    void on_stream_text_delta(const QString& text);
    void on_stream_tool_end(const QString& tool_name, int duration_ms);
    void on_stream_step_start(int step_number);
    void on_stream_step_finish(int tokens_used);
    void on_stream_thinking(const QString& content);
    void on_stream_finish(int total_tokens);
    void on_stream_error(const QString& message);
    void on_stream_heartbeat();
    void on_insufficient_credits();
    void on_tools_registered(int count);
    void set_credits(int credits);

  private slots:
    void on_send_clicked();
    void on_optimize_clicked();
    void on_typing_tick();

  protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

  private:
    QLabel* hdr_title_lbl_ = nullptr;
    QLabel* hdr_credits_lbl_ = nullptr;
    QLabel* hdr_tools_lbl_ = nullptr;
    QLabel* hdr_tokens_lbl_ = nullptr;
    QPushButton* mode_btn_ = nullptr;

    QScrollArea* scroll_area_ = nullptr;
    QWidget* messages_container_ = nullptr;
    QVBoxLayout* messages_layout_ = nullptr;
    QWidget* welcome_panel_ = nullptr;

    QWidget* typing_indicator_ = nullptr;
    QLabel* typing_dots_lbl_ = nullptr;
    QLabel* typing_status_lbl_ = nullptr;
    QTimer* typing_timer_ = nullptr;
    int typing_step_ = 0;

    QPlainTextEdit* input_box_ = nullptr;
    QPushButton* send_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QPushButton* optimize_btn_ = nullptr;
    QLabel* char_lbl_ = nullptr;

    StreamMode current_mode_ = StreamMode::Lite;
    bool streaming_ = false;
    QPointer<QTextEdit> streaming_bubble_;
    QString streaming_buffer_;
    int total_tokens_ = 0;
    bool scroll_locked_ = false;

    QTimer* render_timer_ = nullptr;
    bool render_dirty_ = false;

    // Thinking accumulator — collapsed into one card on finish
    QStringList pending_thinking_;
    QVector<QPair<QString, int>> pending_tools_;
    QWidget* thinking_card_ = nullptr;

    void build_ui();
    QWidget* build_header();
    QWidget* build_welcome();
    QWidget* build_messages_area();
    QWidget* build_typing_indicator();
    QWidget* build_input_area();

    void add_message_bubble(const QString& role, const QString& content, const QString& timestamp = {});
    QTextEdit* add_streaming_bubble();
    void insert_collapsed_thinking_card(int before_index = -1);
    void resize_bubble(QTextEdit* bubble);
    void scroll_to_bottom();
    void show_welcome(bool show);
    void show_typing(bool show);
    void set_input_enabled(bool enabled);
};

} // namespace fincept::chat_mode
