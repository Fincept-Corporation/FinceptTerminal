#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>

class QBoxLayout;

namespace fincept::ui {
class EnterpriseLockPromo;
}

namespace fincept::screens {

/// Lock screen — PIN setup (mandatory first time) and PIN unlock (subsequent).
/// Obsidian design: sharp corners, monospace, no shadows, terminal aesthetic.
///
/// Pages:
///   0 — PIN setup (create new 6-digit PIN, confirm)
///   1 — PIN unlock (enter PIN to unlock terminal)
///   2 — Lockout (too many failed attempts, must re-authenticate)
///
/// Beside the PIN card (wide window) or as a slim strip under it (narrow
/// window / large font, unlock page only) the screen advertises Fincept
/// Terminal Enterprise (ui::EnterpriseLockPromo): static generic copy, no
/// account data, links that open in the browser, nothing focusable — the PIN
/// field keeps the keyboard. Hidden on the lockout page, when there is no room
/// for it, and for an account whose in-memory plan is already Enterprise.
///
/// Internationalised via tr() + retranslateUi(); reacts to runtime language
/// switches through changeEvent(QEvent::LanguageChange).
class LockScreen : public QWidget {
    Q_OBJECT
  public:
    explicit LockScreen(QWidget* parent = nullptr);

    /// Show the appropriate page based on PIN state.
    void activate();

    /// Show PIN setup page (mandatory enrollment).
    void show_setup();

    /// Show PIN unlock page (returning user / inactivity lock).
    void show_unlock();

    /// Show lockout page (max attempts exceeded).
    void show_lockout();

  signals:
    /// PIN setup or unlock completed — terminal should proceed.
    void unlocked();

    /// User chose to re-authenticate (from lockout page).
    void reauth_requested();

  private:
    QStackedWidget* pages_ = nullptr;

    // Layout: top row (version, language), then the body — a column holding the
    // PIN card and the Enterprise strip, and the Enterprise panel beside it.
    QBoxLayout* top_row_ = nullptr;
    QBoxLayout* body_ = nullptr;
    ui::EnterpriseLockPromo* promo_panel_ = nullptr; ///< wide: second column
    ui::EnterpriseLockPromo* promo_strip_ = nullptr; ///< narrow: one line under the card

    // PIN Setup page (index 0) — text-bearing widgets
    QLabel* setup_title_ = nullptr;
    QLabel* setup_badge_ = nullptr;
    QLabel* setup_subtitle_ = nullptr;
    QLabel* setup_info_ = nullptr;
    QLabel* setup_pin_lbl_ = nullptr;
    QLabel* setup_confirm_lbl_ = nullptr;
    QLabel* setup_note_ = nullptr;
    QLineEdit* setup_pin_input_ = nullptr;
    QLineEdit* setup_confirm_input_ = nullptr;
    QPushButton* setup_btn_ = nullptr;
    QLabel* setup_error_ = nullptr;

    // PIN Unlock page (index 1)
    QLabel* unlock_title_ = nullptr;
    QLabel* unlock_badge_ = nullptr;
    QLabel* unlock_subtitle_ = nullptr;
    QLabel* unlock_pin_lbl_ = nullptr;
    QLineEdit* unlock_pin_input_ = nullptr;
    QPushButton* unlock_btn_ = nullptr;
    QLabel* unlock_error_ = nullptr;
    QLabel* unlock_attempts_ = nullptr;
    QLabel* unlock_lockout_label_ = nullptr;
    /// "Forgot PIN? Sign in again" escape link. Cached so retranslateUi() can
    /// re-apply its label — it was previously built inline and left in the
    /// source language after a live language switch.
    QPushButton* unlock_forgot_btn_ = nullptr;

    // Lockout page (index 2)
    QLabel* lockout_title_ = nullptr;
    QLabel* lockout_badge_ = nullptr;
    QLabel* lockout_msg_ = nullptr;
    QPushButton* lockout_reauth_btn_ = nullptr;

    // Lockout countdown timer
    QTimer* lockout_timer_ = nullptr;

    // PIN key derivation runs off the UI thread (PinManager); while a check or
    // a PIN setup is in flight its page shows a busy state, its inputs are
    // disabled and Enter / clicks are ignored, so one submission is exactly
    // one attempt.
    bool unlock_busy_ = false;
    bool setup_busy_ = false;
    /// Bumped when the app takes this screen away (a sibling window unlocked,
    /// sign-out, window closing — not a minimise). A result that arrives for an
    /// older serial is ignored: PinManager has accounted it, but it must not
    /// unlock or touch a screen the user has left.
    quint64 attempt_serial_ = 0;

    void build_setup_page();
    void build_unlock_page();
    void build_lockout_page();

    void retranslateUi();

    void on_setup_submit();
    void on_unlock_submit();
    void on_unlock_result(bool ok);
    void set_unlock_busy(bool busy);
    void set_setup_busy(bool busy);
    void update_lockout_display();
    /// Recompute the attempts-remaining label using the live PinManager state.
    /// Called from show_unlock(), unlock failure paths, and retranslateUi() so
    /// the text stays in sync across both state and language changes.
    void refresh_attempts_label();
    /// Card width from the font size (wider at larger fonts), never wider
    /// than the screen leaves room for; and which Enterprise promotion fits —
    /// the panel beside the card when the width (font-metric breakpoint) and
    /// height allow, else the strip under the unlock card when the height
    /// allows, else none. Re-run on resize, show, page change, language change
    /// and ThemeManager::appearance_settled.
    void fit_layout();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
};

} // namespace fincept::screens
