#include "screens/auth/ForgotPasswordScreen.h"

#include "auth/AuthManager.h"
#include "auth/AuthTypes.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::screens {

ForgotPasswordScreen::ForgotPasswordScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(ui::colors::DARK));

    auto* root = new QVBoxLayout(this);
    root->setAlignment(Qt::AlignCenter);

    pages_ = new QStackedWidget;
    pages_->setFixedSize(380, 400);

    build_email_page();
    build_otp_sent_page();
    build_reset_page();
    build_success_page();

    root->addWidget(pages_);

    auto& auth = auth::AuthManager::instance();
    connect(&auth, &auth::AuthManager::forgot_password_sent, this, [this]() { pages_->setCurrentIndex(1); });
    connect(&auth, &auth::AuthManager::forgot_password_failed, this, [this](const QString& err) {
        error_label_->setText(err);
        error_label_->show();
    });
    connect(&auth, &auth::AuthManager::password_reset_succeeded, this, [this]() { pages_->setCurrentIndex(3); });
    connect(&auth, &auth::AuthManager::password_reset_failed, this, [this](const QString& err) {
        error_label_->setText(err);
        error_label_->show();
    });
}

void ForgotPasswordScreen::build_email_page() {
    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px;")
                            .arg(ui::colors::PANEL, ui::colors::BORDER));

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(12);

    auto* header = new QWidget;
    header->setStyleSheet("background: transparent;");
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(0, 0, 0, 0);
    auto* back = new QPushButton("<");
    back->setFixedSize(28, 28);
    back->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; font-size: 16px; }")
                            .arg(ui::colors::MUTED));
    connect(back, &QPushButton::clicked, this, &ForgotPasswordScreen::navigate_login);
    hl->addWidget(back);
    auto* title = new QLabel("Reset Password");
    title->setStyleSheet(
        QString("color: %1; font-size: 20px; font-weight: 300; background: transparent;").arg(ui::colors::WHITE));
    hl->addWidget(title);
    hl->addStretch();
    vl->addWidget(header);

    auto* sub = new QLabel("Enter your email and we'll send a verification code.");
    sub->setWordWrap(true);
    sub->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::MUTED));
    vl->addWidget(sub);

    auto* lbl = new QLabel("Email");
    lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::WHITE));
    vl->addWidget(lbl);

    email_input_ = new QLineEdit;
    email_input_->setPlaceholderText("Enter your email");
    email_input_->setFixedHeight(32);
    vl->addWidget(email_input_);

    error_label_ = new QLabel;
    error_label_->setWordWrap(true);
    error_label_->setStyleSheet("color: #FF4444; font-size: 12px; background: rgba(255,0,0,0.1); padding: 6px;");
    error_label_->hide();
    vl->addWidget(error_label_);

    auto* send_btn = new QPushButton("Send Code");
    send_btn->setFixedHeight(32);
    connect(send_btn, &QPushButton::clicked, this, &ForgotPasswordScreen::on_send_code);
    connect(email_input_, &QLineEdit::returnPressed, this, &ForgotPasswordScreen::on_send_code);
    vl->addWidget(send_btn);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER));
    vl->addWidget(sep);

    auto* back_login = new QPushButton("Remember your password? Sign In");
    back_login->setStyleSheet(
        "QPushButton { color: #4488FF; background: transparent; border: none; font-size: 12px; }");
    connect(back_login, &QPushButton::clicked, this, &ForgotPasswordScreen::navigate_login);
    vl->addWidget(back_login, 0, Qt::AlignCenter);

    vl->addStretch();

    pages_->addWidget(page);
}

void ForgotPasswordScreen::build_otp_sent_page() {
    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px;")
                            .arg(ui::colors::PANEL, ui::colors::BORDER));

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(12);
    vl->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("Check Your Email");
    title->setStyleSheet(
        QString("color: %1; font-size: 20px; font-weight: 300; background: transparent;").arg(ui::colors::WHITE));
    title->setAlignment(Qt::AlignCenter);
    vl->addWidget(title);

    auto* sub = new QLabel("We've sent a verification code. Enter it on the next screen to reset your password.");
    sub->setWordWrap(true);
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::MUTED));
    vl->addWidget(sub);

    auto* cont_btn = new QPushButton("I Have the Code");
    cont_btn->setFixedHeight(32);
    connect(cont_btn, &QPushButton::clicked, this, [this]() { pages_->setCurrentIndex(2); });
    vl->addWidget(cont_btn);

    auto* resend = new QPushButton("Didn't receive? Resend");
    resend->setStyleSheet("QPushButton { color: #4488FF; background: transparent; border: none; font-size: 12px; }");
    connect(resend, &QPushButton::clicked, this, &ForgotPasswordScreen::on_resend);
    vl->addWidget(resend, 0, Qt::AlignCenter);

    pages_->addWidget(page);
}

void ForgotPasswordScreen::build_reset_page() {
    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px;")
                            .arg(ui::colors::PANEL, ui::colors::BORDER));

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(10);

    auto* title = new QLabel("Reset Password");
    title->setStyleSheet(
        QString("color: %1; font-size: 20px; font-weight: 300; background: transparent;").arg(ui::colors::WHITE));
    vl->addWidget(title);

    auto add_field = [&](const QString& label, const QString& placeholder, bool is_password = false) -> QLineEdit* {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::WHITE));
        vl->addWidget(lbl);
        auto* input = new QLineEdit;
        input->setPlaceholderText(placeholder);
        input->setFixedHeight(32);
        if (is_password)
            input->setEchoMode(QLineEdit::Password);
        vl->addWidget(input);
        return input;
    };

    otp_input_ = add_field("Verification Code", "Enter code from email");
    new_password_ = add_field("New Password", "Min 8 characters", true);
    confirm_password_ = add_field("Confirm Password", "Re-enter password", true);

    // Reuse the error label from page 0 by adding another one here
    auto* err = new QLabel;
    err->setWordWrap(true);
    err->setStyleSheet("color: #FF4444; font-size: 12px; background: rgba(255,0,0,0.1); padding: 6px;");
    err->hide();
    vl->addWidget(err);
    // We'll use error_label_ for all pages (it's on page 0), but for this page we need a local one
    // So let's just connect the signal to show errors on this label too
    connect(&auth::AuthManager::instance(), &auth::AuthManager::password_reset_failed, this, [err](const QString& e) {
        err->setText(e);
        err->show();
    });

    auto* reset_btn = new QPushButton("Reset Password");
    reset_btn->setFixedHeight(32);
    connect(reset_btn, &QPushButton::clicked, this, &ForgotPasswordScreen::on_reset_password);
    vl->addWidget(reset_btn);

    vl->addStretch();

    pages_->addWidget(page);
}

void ForgotPasswordScreen::build_success_page() {
    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px;")
                            .arg(ui::colors::PANEL, ui::colors::BORDER));

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(12);
    vl->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("Password Reset");
    title->setStyleSheet(
        QString("color: %1; font-size: 20px; font-weight: 300; background: transparent;").arg(ui::colors::WHITE));
    title->setAlignment(Qt::AlignCenter);
    vl->addWidget(title);

    auto* sub = new QLabel("Your password has been successfully reset. You can now sign in with your new password.");
    sub->setWordWrap(true);
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::GREEN));
    vl->addWidget(sub);

    auto* login_btn = new QPushButton("Continue to Login");
    login_btn->setFixedHeight(32);
    connect(login_btn, &QPushButton::clicked, this, &ForgotPasswordScreen::navigate_login);
    vl->addWidget(login_btn);

    pages_->addWidget(page);
}

void ForgotPasswordScreen::on_send_code() {
    error_label_->hide();
    QString email = email_input_->text().trimmed();
    auto v = auth::validate_email(email);
    if (!v.valid) {
        error_label_->setText(v.error);
        error_label_->show();
        return;
    }
    auth::AuthManager::instance().forgot_password(email);
}

void ForgotPasswordScreen::on_reset_password() {
    QString otp = otp_input_->text().trimmed();
    QString pw = new_password_->text();
    QString cpw = confirm_password_->text();

    if (otp.isEmpty())
        return;
    if (pw.length() < 8)
        return;
    if (pw != cpw)
        return;

    auth::AuthManager::instance().reset_password(email_input_->text().trimmed(), otp, pw);
}

void ForgotPasswordScreen::on_resend() {
    auth::AuthManager::instance().forgot_password(email_input_->text().trimmed());
}

} // namespace fincept::screens
