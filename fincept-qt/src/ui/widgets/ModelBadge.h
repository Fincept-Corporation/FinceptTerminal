#pragma once
// ModelBadge — which model answers this feature, and the place to change it.
//
//   ● Claude Code · sonnet · high ▾
//
// One shared widget for every AI surface (AI Chat header, the Quick Chat
// bubble, Dashboard Quick Ask, …). It shows ModelRouter's first choice for its
// feature with a state dot (SUCCESS: a connected harness answers; ACCENT: an
// API provider or Fincept LLM; WARNING: the harness the user meant to use is
// unavailable and the next model answers). A click opens the picker:
//   Auto (the rule — what it resolves to now)
//   each harness: its models and efforts when it may serve terminal AI; "Needs
//     your OK…" (the consent dialog) when only consent is missing; otherwise
//     disabled with the reason ("Workbench only — sign in with an API key")
//     and a link to the vendor's API-key page
//   the API profiles, the active provider, Fincept LLM
//   Model settings…
// The choice is stored as the feature's explicit assignment (ModelRouter::set_choice).
//
// Appearance: custom-painted from the live theme tokens and the user's font
// (no stylesheet, nothing cached); repainted on theme changes, re-fitted on
// ThemeManager::appearance_settled. Nothing here blocks the GUI thread.

#include <QString>
#include <QWidget>

#include <functional>

namespace fincept::ui {

class ModelBadge : public QWidget {
    Q_OBJECT
  public:
    explicit ModelBadge(const QString& feature, QWidget* parent = nullptr, const QString& context_id = {});

    QString feature() const { return feature_; }
    /// Compact: no effort, shorter text (the bubble header).
    void set_compact(bool compact);
    /// Re-resolve now (also automatic on ModelRouter::routes_changed).
    void refresh();
    /// The label currently shown ("Claude Code · sonnet · high").
    QString text() const { return text_; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  signals:
    /// The user picked another model for this feature.
    void choice_changed();

  protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    enum class Tone { Harness, Neutral, Warning };
    void open_picker();
    QString elided_text(int width) const;

    QString feature_;
    QString context_id_;
    QString text_;
    QString tooltip_;
    Tone tone_ = Tone::Neutral;
    bool compact_ = false;
    bool hover_ = false;
};

/// The one-time terminal-AI consent dialog for a harness: it names the vendor
/// that will receive the data and what is sent. Asynchronous (window-modal);
/// `done(true)` after the user agreed — the consent is then stored and, with
/// `enable_default`, "Use for terminal AI" is switched on. GUI thread.
void request_harness_terminal_consent(QWidget* parent, const QString& harness_id, bool enable_default,
                                      std::function<void(bool agreed)> done = {});

/// Show that dialog whenever HarnessRegistry::terminal_consent_requested fires
/// (e.g. the Agent Workbench's "Use for terminal AI" checkbox). Idempotent.
void install_harness_consent_handler();

/// Open Settings › LLM Configuration (the Terminal AI tab is there).
void open_llm_settings();

} // namespace fincept::ui
