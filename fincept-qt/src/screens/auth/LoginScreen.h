#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens {

/// Login screen — email/password, MFA step, force-login for active sessions.
/// Obsidian design: sharp corners, monospace, no shadows, terminal aesthetic.
///
/// Internationalised: all user-facing strings flow through tr(). On
/// QEvent::LanguageChange (emitted by Qt when LanguageManager installs a new
/// translator) retranslateUi() reapplies translated text to every widget held
/// by member pointer.
class LoginScreen : public QWidget {
    Q_OBJECT
  public:
    explicit LoginScreen(QWidget* parent = nullptr);

  signals:
    void navigate_register();
    void navigate_forgot_password();

  private:
    // ── Login page widgets that need re-translating ─────────────────────────
    QLabel* login_title_ = nullptr;
    QLabel* login_subtitle_ = nullptr;
    QLabel* email_lbl_ = nullptr;
    QLabel* pw_lbl_ = nullptr;
    QLineEdit* email_input_ = nullptr;
    QLineEdit* password_input_ = nullptr;
    QPushButton* login_btn_ = nullptr;
    QPushButton* forgot_btn_ = nullptr;
    QPushButton* show_pw_btn_ = nullptr;
    QLabel* no_account_lbl_ = nullptr;
    QPushButton* signup_btn_ = nullptr;
    QLabel* error_label_ = nullptr;
    QPushButton* google_btn_ = nullptr;
    QLabel* or_lbl_ = nullptr;

    // ── MFA page widgets ────────────────────────────────────────────────────
    QWidget* mfa_page_ = nullptr;
    QLabel* mfa_title_ = nullptr;
    QLabel* mfa_indicator_ = nullptr;
    QLabel* mfa_sub_ = nullptr;
    QLabel* mfa_code_lbl_ = nullptr;
    QLineEdit* mfa_input_ = nullptr;
    QPushButton* mfa_verify_btn_ = nullptr;
    QPushButton* mfa_back_btn_ = nullptr;
    QLabel* mfa_error_ = nullptr;

    // ── Conflict page widgets ───────────────────────────────────────────────
    QWidget* conflict_page_ = nullptr;
    QLabel* conflict_title_ = nullptr;
    QLabel* conflict_warning_lbl_ = nullptr;
    QLabel* conflict_msg_ = nullptr;
    QPushButton* force_login_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;

    QStackedWidget* pages_ = nullptr;

    void build_login_page();
    void build_mfa_page();
    void build_conflict_page();

    /// Reapply translated text to every widget cached as a member pointer.
    /// Called from the constructor and from changeEvent(LanguageChange).
    void retranslateUi();

    void show_error(const QString& msg);
    void clear_error();
    void set_loading(bool loading);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    /// Wipe email/password/MFA fields whenever the screen leaves the stack so
    /// credentials do not linger across logout → login cycles.
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_login();
    void on_mfa_verify();
    void on_force_login();
    void on_login_succeeded();
    void on_login_failed(const QString& error);
    void on_mfa_required();
    void on_active_session(const QString& msg);
    void on_mfa_verified();
    void on_mfa_failed(const QString& error);
};

} // namespace fincept::screens
