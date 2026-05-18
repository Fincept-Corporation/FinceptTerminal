#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens {

/// Registration screen — Obsidian design, compact form + OTP verification.
/// Internationalised via tr() + retranslateUi(); reacts to runtime language
/// switches through changeEvent(QEvent::LanguageChange).
class RegisterScreen : public QWidget {
    Q_OBJECT
  public:
    explicit RegisterScreen(QWidget* parent = nullptr);

  signals:
    void navigate_login();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    /// Wipe registration + OTP fields whenever the screen leaves the stack so
    /// entered credentials do not linger.
    void hideEvent(QHideEvent* event) override;

  private:
    QStackedWidget* pages_ = nullptr;

    // Form page — text-bearing widgets retained for retranslation
    QLabel* form_title_ = nullptr;
    QLabel* first_name_lbl_ = nullptr;
    QLabel* last_name_lbl_ = nullptr;
    QLabel* email_lbl_ = nullptr;
    QLabel* code_lbl_ = nullptr;
    QLabel* phone_lbl_ = nullptr;
    QLabel* password_lbl_ = nullptr;
    QLabel* confirm_pw_lbl_ = nullptr;

    QLineEdit* first_name_ = nullptr;
    QLineEdit* last_name_ = nullptr;
    QLineEdit* email_ = nullptr;
    QLineEdit* phone_ = nullptr;
    QLineEdit* country_code_ = nullptr;
    QLineEdit* password_ = nullptr;
    QLineEdit* confirm_pw_ = nullptr;
    QPushButton* register_btn_ = nullptr;
    QLabel* error_label_ = nullptr;
    QLabel* have_account_lbl_ = nullptr;
    QPushButton* signin_btn_ = nullptr;

    QLabel* pw_len_ = nullptr;
    QLabel* pw_up_ = nullptr;
    QLabel* pw_low_ = nullptr;
    QLabel* pw_num_ = nullptr;
    QLabel* pw_spec_ = nullptr;

    // OTP page
    QLabel* otp_title_ = nullptr;
    QLabel* otp_code_lbl_ = nullptr;
    QLineEdit* otp_input_ = nullptr;
    QPushButton* verify_btn_ = nullptr;
    QPushButton* resend_btn_ = nullptr;
    QPushButton* back_to_form_btn_ = nullptr;
    QLabel* otp_error_ = nullptr;
    QLabel* otp_email_ = nullptr;

    void build_form_page();
    void build_otp_page();
    void update_password_strength();
    void retranslateUi();

  private slots:
    void on_register();
    void on_verify_otp();
    void on_resend_otp();
};

} // namespace fincept::screens
