#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Lock screen — PIN setup (mandatory first time) and PIN unlock (subsequent).
/// Obsidian design: sharp corners, monospace, no shadows, terminal aesthetic.
///
/// Pages:
///   0 — PIN setup (create new 6-digit PIN, confirm)
///   1 — PIN unlock (enter PIN to unlock terminal)
///   2 — Lockout (too many failed attempts, must re-authenticate)
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

    // Lockout page (index 2)
    QLabel* lockout_title_ = nullptr;
    QLabel* lockout_badge_ = nullptr;
    QLabel* lockout_msg_ = nullptr;
    QPushButton* lockout_reauth_btn_ = nullptr;

    // Lockout countdown timer
    QTimer* lockout_timer_ = nullptr;

    void build_setup_page();
    void build_unlock_page();
    void build_lockout_page();

    void retranslateUi();

    void on_setup_submit();
    void on_unlock_submit();
    void update_lockout_display();
    /// Recompute the attempts-remaining label using the live PinManager state.
    /// Called from show_unlock(), unlock failure paths, and retranslateUi() so
    /// the text stays in sync across both state and language changes.
    void refresh_attempts_label();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
};

} // namespace fincept::screens
