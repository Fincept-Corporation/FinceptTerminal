#pragma once
// AiChatBubble — Floating "Quick Chat" assistant bubble.
//
// EXPLICITLY independent of the AI Chat tab:
//   • Does NOT use ChatRepository — no sessions, no DB writes.
//   • In-memory history only, lost on app exit (and on Clear).
//   • Bubble conversations never appear in the AI Chat tab's session list.
//
// Voice flow:
//   mic_btn or voice_mode_btn
//     → SpeechService::start_listening()
//     → SpeechService::transcription_ready(text)   [signal, UI thread]
//     → on_transcription(text)
//     → input_box_ populated → on_send()
//   AI reply (in voice mode)
//     → TtsService::speak(text)
//     → TtsService::speaking_finished() → resume listening

#include "ai_chat/LlmService.h"

#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace fincept {

class AiChatBubble : public QWidget {
    Q_OBJECT
  public:
    explicit AiChatBubble(QWidget* parent = nullptr);

    /// Call from the parent's resizeEvent to reposition the bubble + panel.
    void reposition();

  protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

  private slots:
    void on_send();
    void on_clear_chat();
    void on_stream_chunk(const QString& chunk, bool done);
    void on_streaming_done(ai_chat::LlmResponse response);
    void on_toggle_voice_mode();
    void on_toggle_mic();
    void on_stop_speech();
    void on_transcription(const QString& text);
    void on_stt_listening_changed(bool active);
    void on_stt_error(const QString& message);

  private:
    enum class Status { Idle, Listening, Thinking, Speaking, Error };

    // ── UI panels ────────────────────────────────────────────────────────────
    QWidget*     bubble_btn_  = nullptr;
    QWidget*     chat_panel_  = nullptr;
    QLabel*      unread_badge_ = nullptr;

    // Panel internals
    QScrollArea*    scroll_area_      = nullptr;
    QWidget*        msg_container_    = nullptr;
    QVBoxLayout*    msg_layout_       = nullptr;
    QWidget*        welcome_widget_   = nullptr;   // shown when chat is empty
    QPlainTextEdit* input_box_        = nullptr;
    QPushButton*    send_btn_         = nullptr;
    QPushButton*    mic_btn_          = nullptr;
    QPushButton*    voice_mode_btn_   = nullptr;
    QPushButton*    new_btn_          = nullptr;
    QPushButton*    close_btn_        = nullptr;

    // Unified status strip — replaces the old typing_row + voice_status_bar.
    QWidget*        status_strip_     = nullptr;
    QLabel*         status_dot_       = nullptr;
    QLabel*         status_lbl_       = nullptr;
    QPushButton*    stop_speech_btn_  = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    bool is_open_      = false;
    bool voice_mode_   = false;
    bool is_listening_ = false;
    bool is_speaking_  = false;
    bool streaming_    = false;
    QString error_msg_;
    int unread_count_  = 0;

    /// In-memory only — never written to ChatRepository. The whole point of
    /// the bubble is to stay separate from the AI Chat tab.
    std::vector<ai_chat::ConversationMessage> chat_history_;

    QPointer<QLabel> streaming_bubble_;

    // ── Build helpers ────────────────────────────────────────────────────────
    void build_bubble_button();
    void build_chat_panel();
    QWidget* build_panel_header();
    QWidget* build_input_row();
    QWidget* build_status_strip();
    void build_welcome_widget();

    // ── Behaviour helpers ────────────────────────────────────────────────────
    void refresh_theme();
    void open_panel();
    void close_panel();
    void show_welcome_if_empty();
    void hide_welcome();
    void add_bubble(const QString& role, const QString& text);
    QLabel* add_streaming_bubble();
    void scroll_to_bottom();
    void set_input_enabled(bool enabled);
    void update_unread(int delta);
    void start_listening();
    void stop_listening();
    void speak_text(const QString& text);
    void stop_tts();

    // ── Status / mic visuals ─────────────────────────────────────────────────
    void render_status();                  // derive Status from flags + paint
    void set_mic_listening_visual(bool on);
};

} // namespace fincept
