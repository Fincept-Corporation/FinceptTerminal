#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens {

/// 4-step password reset — Obsidian design.
class ForgotPasswordScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ForgotPasswordScreen(QWidget* parent = nullptr);

  signals:
    void navigate_login();

  protected:
    void paintEvent(QPaintEvent* event) override;
    /// Wipe email/OTP/new-password fields whenever the screen leaves the stack.
    void hideEvent(QHideEvent* event) override;

  private:
    QStackedWidget* pages_ = nullptr;

    QLineEdit* email_input_ = nullptr;
    QLineEdit* otp_input_ = nullptr;
    QLineEdit* new_password_ = nullptr;
    QLineEdit* confirm_password_ = nullptr;
    QLabel* error_label_ = nullptr;

    void build_email_page();
    void build_otp_sent_page();
    void build_reset_page();
    void build_success_page();

  private slots:
    void on_send_code();
    void on_reset_password();
    void on_resend();
};

} // namespace fincept::screens
