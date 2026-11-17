#pragma once
// AiChatBubble — Floating "Quick Chat" assistant bubble.
//
// EXPLICITLY independent of the AI Chat tab:
//   • Does NOT use ChatRepository — no sessions, no DB writes.
//   • In-memory history only, lost on app exit (and on Clear).
//   • Bubble conversations never appear in the AI Chat tab's session list.
//
// Same rendering engine and visual language as the tab, in compact form: the transcript is
// an AicTranscriptView (Density::Compact — COPY only, tools / reasoning collapsed to a line),
// styled by the shared ai_chat selector sheet (AiChatStyle.h).
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

#include "screens/ai_chat/AiChatTypes.h"
#include "services/llm/LlmService.h"

#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace fincept::ui {
class EmptyState;
class ModelBadge;
} // namespace fincept::ui

namespace fincept::screens::aichat {
class AicTranscriptView;
}

namespace fincept {

class AiChatBubble : public QWidget {
    Q_OBJECT
  public:
    explicit AiChatBubble(QWidget* parent = nullptr);

    /// Call from the parent's resizeEvent to reposition the bubble + panel.
    void reposition();

  protected:
    bool eventFilter(QObject* obj, QEvent* e) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_send();
    void on_clear_chat();
    void on_stream_chunk(const QString& chunk, bool done);
    void on_streaming_done(ai_chat::LlmResponse response);
    /// Our request's harness was unavailable and the next model answers.
    void on_route_notice(const QString& origin_session_id, const QString& feature, const QString& notice);
    // Following an AI Chat tab turn that operates the screens (see "Mirror" below).
    void on_request_started(const QString& chat_session_id, const QString& user_message);
    void on_request_chunk(const QString& chat_session_id, const QString& chunk);
    void on_ui_operating(const QString& chat_session_id);
    void on_toggle_voice_mode();
    void on_toggle_mic();
    void on_stop_speech();
    void on_transcription(const QString& text);
    void on_stt_listening_changed(bool active);
    void on_stt_error(const QString& message);

  private:
    enum class Status { Idle, Listening, Thinking, Speaking, Error };

    // ── UI panels ────────────────────────────────────────────────────────────
    QWidget* bubble_btn_ = nullptr;
    QWidget* chat_panel_ = nullptr;
    QLabel* bubble_glyph_ = nullptr;
    QLabel* unread_badge_ = nullptr;

    // Panel internals
    screens::aichat::AicTranscriptView* transcript_ = nullptr;
    QWidget* welcome_widget_ = nullptr; // shown instead of the transcript while the chat is empty
    QLabel* welcome_glyph_ = nullptr;
    QLabel* welcome_heading_ = nullptr; // cached for retranslateUi
    QLabel* welcome_sub_ = nullptr;
    // No AI model configured: the welcome becomes a needs-setup state (built on first need).
    ui::EmptyState* welcome_setup_ = nullptr;
    int welcome_llm_state_ = -1; // -1 not checked yet, 0 missing, 1 configured
    QLabel* hdr_title_lbl_ = nullptr;    // "QUICK CHAT"
    QLabel* hdr_subtitle_lbl_ = nullptr; // "Not saved · separate from AI Chat tab"
    ui::ModelBadge* model_badge_ = nullptr; // which model answers Quick Chat; click = picker
    QPlainTextEdit* input_box_ = nullptr;
    QPushButton* send_btn_ = nullptr;
    QPushButton* mic_btn_ = nullptr;
    QPushButton* voice_mode_btn_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QPushButton* close_btn_ = nullptr;
    QWidget* header_ = nullptr;
    QWidget* input_row_ = nullptr;

    // Unified status strip.
    QWidget* status_strip_ = nullptr;
    QLabel* status_dot_ = nullptr;
    QLabel* status_lbl_ = nullptr;
    QPushButton* stop_speech_btn_ = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    bool is_open_ = false;
    bool voice_mode_ = false;
    bool is_listening_ = false;
    bool is_speaking_ = false;
    bool streaming_ = false;
    QString error_msg_;
    int unread_count_ = 0;

    /// In-memory only — never written to ChatRepository. The whole point of the bubble is
    /// to stay separate from the AI Chat tab.
    std::vector<ai_chat::ConversationMessage> chat_history_;

    // Mirror: a turn sent from the AI Chat tab that starts operating the terminal (opening a
    // screen, working its controls) takes the user away from the chat. From that moment the
    // bubble opens and shows the same turn live - question, tool steps, answer - so the work
    // can be followed on whatever screen the AI is driving. Until then the turn's chunks are
    // only kept, so the mirrored turn starts complete.
    QString mirror_session_;    ///< the tab request being watched ("" = none)
    QString mirror_user_text_;
    QStringList mirror_chunks_; ///< chunks received before the mirror engaged
    bool mirror_engaged_ = false;

    // Minimised while the AI works: the round button breathes (slow fade in and out);
    // still when idle or while the panel is open.
    QGraphicsOpacityEffect* breath_fx_ = nullptr;
    QPropertyAnimation* breath_anim_ = nullptr;
    void update_breathing();

    quint64 streaming_turn_ = 0;  ///< the answer being written (0 = none)
    qint64 request_started_ms_ = 0;
    QVector<screens::aichat::ToolStep> tool_steps_;

    // ── Build helpers ────────────────────────────────────────────────────────
    void build_bubble_button();
    void build_chat_panel();
    QWidget* build_panel_header();
    QWidget* build_input_row();
    QWidget* build_status_strip();
    void build_welcome_widget();

    // ── Placement ────────────────────────────────────────────────────────────
    /// Set the bubble + panel geometry, lifted above any status strip (or horizontal
    /// scrollbar) that runs along the bottom edge under the bubble.
    void place();
    /// Height of that bottom strip under the bubble in the parent, 0 if none.
    int bottom_strip_height() const;
    /// `lift` raised until the resting bubble covers no button or input of the screen under
    /// it (the workbench's FOLLOW UP row); `lift` itself when that would take it more than a
    /// third of the way up.
    int lift_over_controls(int lift);
    bool replace_pending_ = false; // a deferred place() is queued (lazily built screens)

    // ── Behaviour helpers ────────────────────────────────────────────────────
    void refresh_theme();
    void retranslateUi();
    void open_panel();
    void close_panel();
    void show_welcome_if_empty();
    void hide_welcome();
    /// Swap the welcome between "How can I help?" and "No AI model configured"
    /// (OPEN SETTINGS > LLM CONFIG) to match LlmService; applied only on a change.
    void refresh_welcome_setup();
    void add_turn(screens::aichat::TurnRole role, const QString& text,
                  screens::aichat::NoticeKind notice = screens::aichat::NoticeKind::None);
    void add_fincept_notice(const QString& error); // out of credits / sign-in rejected
    void add_route_notice(const QString& text);    // a harness was unavailable (fallback / ask / fail)
    void append_tool_chunk(const QString& text);
    void set_input_enabled(bool enabled);
    void update_unread(int delta);
    void start_listening();
    void stop_listening();
    void speak_text(const QString& text);
    void stop_tts();

    // ── Status / mic visuals ─────────────────────────────────────────────────
    void render_status(); // derive Status from flags + paint
    void set_mic_listening_visual(bool on);
};

} // namespace fincept
