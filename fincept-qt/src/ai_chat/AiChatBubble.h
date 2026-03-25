#pragma once
#include "ai_chat/LlmService.h"

#include <QKeyEvent>
#include <QLabel>
#include <QMediaPlayer>
#include <QMutex>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <atomic>
#include <vector>

namespace fincept {

// ── AiChatBubble ─────────────────────────────────────────────────────────────
// Floating AI assistant bubble — fixed bottom-right corner of the main window.
// Features: streaming chat, voice input (Windows SAPI), TTS output (ElevenLabs
// / OpenAI / browser via QMediaPlayer), session persistence via ChatRepository.

class AiChatBubble : public QWidget {
    Q_OBJECT
  public:
    explicit AiChatBubble(QWidget* parent = nullptr);

    // Call from parent's resizeEvent to reposition
    void reposition();

  protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

  private slots:
    void on_toggle_open();
    void on_send();
    void on_new_session();
    void on_stream_chunk(const QString& chunk, bool done);
    void on_streaming_done(ai_chat::LlmResponse response);
    void on_toggle_voice_mode();
    void on_toggle_mic();
    void on_stop_speech();
    void on_stt_finished(int exit_code, QProcess::ExitStatus status);
    void on_tts_media_status(QMediaPlayer::MediaStatus status);

  private:
    // ── UI panels ────────────────────────────────────────────────────────────
    QWidget* bubble_btn_  = nullptr;  // floating round button
    QWidget* chat_panel_  = nullptr;  // expanded chat panel
    QLabel*  unread_badge_= nullptr;

    // Panel internals
    QScrollArea*   scroll_area_    = nullptr;
    QWidget*       msg_container_  = nullptr;
    QVBoxLayout*   msg_layout_     = nullptr;
    QPlainTextEdit* input_box_     = nullptr;
    QPushButton*   send_btn_       = nullptr;
    QPushButton*   mic_btn_        = nullptr;
    QPushButton*   voice_mode_btn_ = nullptr;
    QPushButton*   stop_speech_btn_= nullptr;
    QPushButton*   new_btn_        = nullptr;
    QPushButton*   close_btn_      = nullptr;
    QWidget*       voice_status_bar_ = nullptr;
    QLabel*        voice_status_lbl_ = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    bool is_open_       = false;
    bool voice_mode_    = false;
    bool is_listening_  = false;
    bool is_speaking_   = false;
    bool streaming_     = false;
    int  unread_count_  = 0;

    QString active_session_id_;
    std::vector<ai_chat::ConversationMessage> history_;
    QPointer<QTextEdit> streaming_bubble_;

    // ── TTS ──────────────────────────────────────────────────────────────────
    QMediaPlayer* tts_player_  = nullptr;
    QProcess*     stt_process_ = nullptr;  // Windows SAPI STT
    QTimer*       pulse_timer_ = nullptr;  // button animation
    int           pulse_step_  = 0;

    // ── Build ────────────────────────────────────────────────────────────────
    void build_bubble_button();
    void build_chat_panel();
    QWidget* build_panel_header();
    QWidget* build_input_row();
    QWidget* build_voice_status_bar();

    // ── Helpers ──────────────────────────────────────────────────────────────
    void open_panel();
    void close_panel();
    void ensure_session();
    void add_bubble(const QString& role, const QString& text);
    QTextEdit* add_streaming_bubble();
    void scroll_to_bottom();
    void set_ui_enabled(bool enabled);
    void update_unread(int delta);
    void start_listening();
    void stop_listening();
    void speak_text(const QString& text);
    void stop_tts();
    void update_voice_status();
    void animate_pulse();
};

} // namespace fincept
