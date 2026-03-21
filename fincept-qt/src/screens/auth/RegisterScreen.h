#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>

namespace fincept::screens {

/// Registration screen — name, email, phone, password, OTP verification.
class RegisterScreen : public QWidget {
    Q_OBJECT
public:
    explicit RegisterScreen(QWidget* parent = nullptr);

signals:
    void navigate_login();

private:
    QStackedWidget* pages_ = nullptr;

    // Form fields
    QLineEdit* first_name_ = nullptr;
    QLineEdit* last_name_  = nullptr;
    QLineEdit* email_      = nullptr;
    QLineEdit* phone_      = nullptr;
    QLineEdit* password_   = nullptr;
    QLineEdit* confirm_pw_ = nullptr;
    QPushButton* register_btn_ = nullptr;
    QLabel*    error_label_ = nullptr;

    // Password strength labels
    QLabel* pw_len_  = nullptr;
    QLabel* pw_up_   = nullptr;
    QLabel* pw_low_  = nullptr;
    QLabel* pw_num_  = nullptr;
    QLabel* pw_spec_ = nullptr;

    // OTP page
    QLineEdit*   otp_input_  = nullptr;
    QPushButton* verify_btn_ = nullptr;
    QLabel*      otp_error_  = nullptr;
    QLabel*      otp_email_  = nullptr;

    void build_form_page();
    void build_otp_page();
    void update_password_strength();

private slots:
    void on_register();
    void on_verify_otp();
    void on_resend_otp();
};

} // namespace fincept::screens
