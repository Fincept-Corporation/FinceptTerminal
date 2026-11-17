#pragma once
// AiChatComposer.h — the AI Chat composer (bottom of the centre zone).
//
//   [AAPL · linked ×] [notes.csv · 12 KB ×]                          (chips, wrap)
//   ┌──────────────────────────────────────────────────────────────────────────────┐
//   │ Ask about markets, a company, your portfolio…                                  │ grows 1 → ~8 lines
//   └──────────────────────────────────────────────────────────────────────────────┘
//   ENTER send · SHIFT+ENTER new line · / prompts · @ symbol or file      ≈ 312 tok · reply ≤ 8,192   [ATTACH] [SEND]
//
//   * Enter sends, Shift+Enter inserts a new line (said on the hint line). Enter never stops
//     a running answer; Esc does (or closes an open menu). Ctrl+R = regenerate / retry.
//   * "/" at the start of a word opens the prompt menu: saved prompts and built-in research
//     templates — choosing one FILLS the composer (never sends).
//   * "@" opens the mention menu: a ui::SymbolSearchBox (its resolver; the picked symbol
//     replaces the "@" and becomes a context chip), Portfolio ▸ / Watchlist ▸ (a reference
//     chip), Attach file…
//   * Drag-and-drop or paste of files attaches them (chips; contents read only when sent).
//     Images cannot be sent (the chat service sends text): refused with the reason.
//   * SEND / STOP: Send carries fincept_agent_risk "spend"; while an answer streams the
//     button reads STOP and carries no marker. The editor itself carries "spend" (Enter
//     sends) and fincept_agent_risk_keys "Ctrl+R=spend". With no model configured, SEND is
//     disabled and the hint line says why.

#include "screens/ai_chat/AiChatTypes.h"

#include <QFrame>
#include <QString>
#include <QVector>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QWidget;

namespace fincept::screens::aichat {

class AicComposerEdit; // AiChatComposer.cpp (QPlainTextEdit subclass)

class AicComposer : public QFrame {
    Q_OBJECT
  public:
    explicit AicComposer(QWidget* parent = nullptr);
    ~AicComposer() override;

    QPlainTextEdit* editor() const;
    QPushButton* send_button() const { return send_btn_; }
    QPushButton* attach_button() const { return attach_btn_; }
    /// Reasoning effort for the next message: "low" | "medium" | "high" | "xhigh" | "max"
    /// (default medium; remembered). High and above cost extra credits on the Fincept model.
    QString effort() const { return effort_; }

    QString text() const;
    void set_text(const QString& text); ///< replaces the draft, cursor at the end
    void insert_text(const QString& text); ///< at the cursor (QUOTE inserts "> …" lines)
    void clear_text();
    void focus_editor();

    /// While an answer streams: the button reads STOP (danger outline, no agent-risk
    /// marker) and Enter does not send. Back to SEND (marker "spend") when idle.
    void set_busy(bool busy);
    /// STOP pressed: "STOPPING…", disabled, until set_busy(false).
    void set_stopping();
    /// No AI model: SEND disabled, the hint line carries `reason` (empty when configured).
    void set_configured(bool configured, const QString& reason = {});

    /// Chips: context items and files the next message carries (same list as the rail).
    void set_chips(const QVector<Attachment>& context, const QVector<Attachment>& files);
    /// "reply ≤ 8,192 tok" on the hint line (0 = not known: hidden).
    void set_reply_cap(int tokens);
    /// Extra tokens the attachments add to the estimate (files: size / 4).
    void set_attachment_tokens(int tokens);
    /// Saved prompts shown first in the "/" menu (built-in templates follow).
    void set_saved_prompts(const QVector<PromptTemplate>& prompts);
    /// Symbol substituted for {SYMBOL} in templates ("" = left as a placeholder to fill).
    void set_symbol_hint(const QString& symbol);

  signals:
    void send_requested();
    void stop_requested();
    void regenerate_requested();
    void attach_file_requested();
    /// Files dropped or pasted (absolute paths).
    void files_dropped(const QStringList& paths);
    /// A dropped / pasted image or image file: refused (text-only chat) — the host says so.
    void image_rejected(const QString& name);
    /// "@" mention: a resolved symbol, or a portfolio / watchlist reference.
    void context_attach_requested(const fincept::screens::aichat::Attachment& item);
    /// × on a chip.
    void chip_removed(const fincept::screens::aichat::Attachment& item);
    void text_changed();

  protected:
    void changeEvent(QEvent* e) override;

  private:
    friend class AicComposerEdit;
    bool busy_ = false;
    bool configured_ = true;
    AicComposerEdit* edit_ = nullptr;
    QWidget* chips_ = nullptr;
    QLabel* hint_ = nullptr;
    QLabel* meter_ = nullptr;
    QPushButton* attach_btn_ = nullptr;
    QPushButton* effort_btn_ = nullptr;
    QString effort_ = QStringLiteral("medium");
    void open_effort_menu();
    void update_effort_button();
    QPushButton* send_btn_ = nullptr;
    QString setup_reason_;
    QString symbol_hint_;
    int reply_cap_ = 0;
    int attachment_tokens_ = 0;
    QVector<PromptTemplate> saved_prompts_;

    void retranslate();
    void fit_height();
    void update_meter();
    void update_send_button();
    void open_prompt_menu();
    void open_mention_menu();

    // ── private (AiChatComposer.cpp) ─────────────────────────────────────────
    bool stopping_ = false;
    int trigger_pos_ = -1; ///< position of the "/" or "@" that opened a menu
    QVector<Attachment> chip_context_;
    QVector<Attachment> chip_files_;

    void rebuild_chips();
    void fit_metrics();
    void update_hint();
    void set_focus_frame(bool focused);
    /// Replaces the token that starts with `trigger` at `pos` ("/…", "@…") by `with`;
    /// inserts at the cursor when that character is no longer there.
    void replace_trigger(int pos, QChar trigger, const QString& with);
    void apply_template(int slash_pos, const PromptTemplate& prompt);
};

} // namespace fincept::screens::aichat
