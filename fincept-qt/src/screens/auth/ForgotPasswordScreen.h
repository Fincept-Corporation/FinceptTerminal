#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens {

/// 4-step password reset — Obsidian design.
/// Internationalised via tr() + retranslateUi(); reacts to runtime language
/// switches through changeEvent(QEvent::LanguageChange).
class ForgotPasswordScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ForgotPasswordScreen(QWidget* parent = nullptr);

  signals:
    void navigate_login();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    /// Wipe email/OTP/new-password fields whenever the screen leaves the stack.
    void hideEvent(QHideEvent* event) override;

  private:
    QStackedWidget* pages_ = nullptr;

    // Email page
    QLabel* email_title_ = nullptr;
    QLabel* email_sub_ = nullptr;
    QLabel* email_lbl_ = nullptr;
    QLineEdit* email_input_ = nullptr;
    QLabel* error_label_ = nullptr;
    QPushButton* send_btn_ = nullptr;
    QPushButton* back_login_btn_ = nullptr;

    // OTP-sent page
    QLabel* sent_title_ = nullptr;
    QLabel* sent_sub_ = nullptr;
    QPushButton* sent_continue_btn_ = nullptr;
    QPushButton* sent_resend_btn_ = nullptr;

    // Reset page
    QLabel* reset_title_ = nullptr;
    QLabel* reset_code_lbl_ = nullptr;
    QLabel* reset_new_lbl_ = nullptr;
    QLabel* reset_confirm_lbl_ = nullptr;
    QLineEdit* otp_input_ = nullptr;
    QLineEdit* new_password_ = nullptr;
    QLineEdit* confirm_password_ = nullptr;
    QPushButton* reset_btn_ = nullptr;

    // Success page
    QLabel* success_title_ = nullptr;
    QLabel* success_status_ = nullptr;
    QLabel* success_sub_ = nullptr;
    QPushButton* success_continue_btn_ = nullptr;

    void build_email_page();
    void build_otp_sent_page();
    void build_reset_page();
    void build_success_page();
    void retranslateUi();

  private slots:
    void on_send_code();
    void on_reset_password();
    void on_resend();
};

} // namespace fincept::screens
