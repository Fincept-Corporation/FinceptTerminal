#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens {

/// Login screen — email/password, MFA step, force-login for active sessions.
/// Obsidian design: sharp corners, monospace, no shadows, terminal aesthetic.
class LoginScreen : public QWidget {
    Q_OBJECT
  public:
    explicit LoginScreen(QWidget* parent = nullptr);

  signals:
    void navigate_register();
    void navigate_forgot_password();

  private:
    QLineEdit* email_input_ = nullptr;
    QLineEdit* password_input_ = nullptr;
    QPushButton* login_btn_ = nullptr;
    QLabel* error_label_ = nullptr;
    QPushButton* show_pw_btn_ = nullptr;

    QWidget* mfa_page_ = nullptr;
    QLineEdit* mfa_input_ = nullptr;
    QPushButton* mfa_verify_btn_ = nullptr;
    QLabel* mfa_error_ = nullptr;

    QWidget* conflict_page_ = nullptr;
    QLabel* conflict_msg_ = nullptr;

    QStackedWidget* pages_ = nullptr;

    void build_login_page();
    void build_mfa_page();
    void build_conflict_page();

    void show_error(const QString& msg);
    void clear_error();
    void set_loading(bool loading);

  protected:
    void paintEvent(QPaintEvent* event) override;

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
