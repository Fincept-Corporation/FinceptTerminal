#pragma once
// ChatComposer.h — Chat Mode's composer (2026-11-17 revamp).
//
//   [THE AGENT IS WAITING FOR YOUR ANSWER … (answer banner, while armed)]
//   [+ CONTEXT] [AAPL ×] [Portfolio: Growth ×] … [ENGINE ▾] [RUN AS TASK] [BUDGET …]   (ComposerContextBar)
//   ┌ multi-line input (grows to ~8 lines; Enter / Ctrl+Enter send, Shift+Enter new line;
//   │ "/" at the start of a line opens the prompt-template menu, "@" the symbol / file mention)
//   └ activity line while a run goes ("thinking…  ·  1:05")
//   LITE|DEEP  PROFILE ▾  MAY: read · tools · propose (you approve)   meter 1,234 / 20,000 · ~5 cr   OPTIMIZE  [SEND ⏎ | STOP]
//
// Files dropped or pasted onto it become the attachment chip (text files, read on a
// worker). Nothing here sends by itself: submit_requested() carries the text to
// ChatMessagePanel, which validates (length, the costly-task confirmation) and sends.
// SEND and STOP are ONE button: in SEND state it carries fincept_agent_risk "spend",
// in STOP state the marker is removed. Theme tokens only; sizes from font metrics.

#include "screens/chat_mode/ChatModeTypes.h"

#include <QString>
#include <QVector>
#include <QWidget>

#include <memory>

class QPlainTextEdit;

namespace fincept::chat_mode {

class ComposerContextBar;

class ChatComposer : public QWidget {
    Q_OBJECT
  public:
    explicit ChatComposer(QWidget* parent = nullptr);
    ~ChatComposer() override;

    ComposerContextBar* context_bar() const;
    QPlainTextEdit* input() const;

    // ── Draft ───────────────────────────────────────────────────────────────
    QString text() const;
    /// Replace the draft (cursor to the end). Emits draft_changed() when it differs.
    void set_text(const QString& text);
    void clear_text();
    void focus_input();
    /// Append `text` as a quote ("> " lines) followed by an empty line, focus the input.
    void insert_quote(const QString& text);

    // ── Run state (driven by ChatMessagePanel) ──────────────────────────────
    /// `busy`: a run of this conversation is going — the button shows STOP (enabled
    /// when `can_stop`, with `stop_text` / `stop_tooltip`: "STOP" / "STOP WAITING").
    /// `pending`: a send is on its way — SEND disabled ("SENDING…").
    void set_run_state(bool busy, bool pending, bool can_stop, const QString& stop_text, const QString& stop_tooltip);
    /// The next message answers the question the agent paused on (banner, placeholder,
    /// SEND reads ANSWER). "Ask something else" in the banner disarms it (emits
    /// answer_cancelled()).
    void set_awaiting_answer(bool on, const QString& question = {});
    bool is_awaiting_answer() const;
    /// Live activity line under the input ("thinking…"); the elapsed time since
    /// `since_ms` (epoch ms, 0 = none) ticks once a second while shown. Empty text
    /// and 0 hide it.
    void set_activity(const QString& text, qint64 since_ms);
    /// A transient line (5 s) under the input — errors in the danger tone.
    void show_note(const QString& text, bool error = false);

    // ── Options ─────────────────────────────────────────────────────────────
    void set_stream_mode(StreamMode mode);
    StreamMode stream_mode() const;
    /// Investment profiles (GET /profile/profiles); the picker hides without any.
    void set_profiles(const QVector<AgentProfile>& profiles);
    /// Profile to attach to the next message; 0 = none.
    int selected_profile_id() const;
    /// -1 = never picked (follow the account's default), 0 = none, > 0 = that profile.
    int preferred_profile_id() const;
    void set_preferred_profile_id(int id);
    /// Account balance for the meter / estimate (< 0 = unknown).
    void set_credits(double balance);
    double credits() const;

  signals:
    /// Enter / Ctrl+Enter / SEND with a non-empty draft (the panel validates and sends).
    void submit_requested(const QString& text);
    /// STOP (the button in STOP state, or Esc in the input while a run goes).
    void stop_requested();
    void draft_changed();
    void mode_toggled(fincept::chat_mode::StreamMode mode);
    void profile_changed();
    /// "Ask something else" in the answer banner.
    void answer_cancelled();
    /// "/" menu: a template that needs NEW RUN's parameter form (it only opens the form).
    void template_requested(const QString& template_id);
    /// "/" menu: "All templates…".
    void templates_requested();
    /// Esc in an idle composer with an empty draft (the screen may go back).
    void escape_pressed();

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
