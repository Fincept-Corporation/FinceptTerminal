#include "screens/auth/LoginScreen.h"
#include "auth/AuthManager.h"
#include "auth/AuthTypes.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/GeometricBackground.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QFrame>
#include <QGraphicsDropShadowEffect>

namespace fincept::screens {

static const char* CARD_STYLE =
    "background: rgba(15,15,15,0.92); border: 1px solid #333333; border-radius: 8px;";

static const char* INPUT_STYLE =
    "QLineEdit { background: #1a1a1a; color: #e0e0e0; border: 1px solid #333333; "
    "border-radius: 4px; padding: 8px 12px; font-size: 14px; font-family: 'Inter', 'Segoe UI Variable', sans-serif; }"
    "QLineEdit:focus { border: 1px solid #555555; }"
    "QLineEdit::placeholder { color: #555555; }";

static const char* BTN_PRIMARY =
    "QPushButton { background: #2a2a2a; color: #e0e0e0; border: 1px solid #444444; "
    "border-radius: 4px; padding: 8px 28px; font-size: 14px; font-weight: 500; "
    "font-family: 'Inter', 'Segoe UI Variable', sans-serif; }"
    "QPushButton:hover { background: #333333; border-color: #555555; }"
    "QPushButton:disabled { color: #555555; }";

static const char* LINK_STYLE =
    "QPushButton { color: #60a5fa; background: transparent; border: none; "
    "font-size: 13px; font-family: 'Inter', 'Segoe UI Variable', sans-serif; }"
    "QPushButton:hover { color: #93bbfc; }";

static const char* LABEL_STYLE =
    "color: #e0e0e0; font-size: 13px; font-weight: 500; background: transparent; "
    "font-family: 'Inter', 'Segoe UI Variable', sans-serif;";

static const char* MUTED_STYLE =
    "color: #888888; font-size: 13px; background: transparent; "
    "font-family: 'Inter', 'Segoe UI Variable', sans-serif;";

LoginScreen::LoginScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // Geometric background fills entire screen
    auto* bg = new ui::GeometricBackground(this);
    bg->setFixedSize(4000, 4000);  // oversized, will be clipped
    bg->move(-500, -500);
    bg->lower();

    auto* overlay = new QVBoxLayout;
    overlay->setAlignment(Qt::AlignCenter);
    root->addLayout(overlay, 1);

    pages_ = new QStackedWidget;
    pages_->setFixedSize(400, 480);
    pages_->setStyleSheet("background: transparent;");

    build_login_page();
    build_mfa_page();
    build_conflict_page();

    overlay->addWidget(pages_, 0, Qt::AlignCenter);

    // Connect to AuthManager signals
    auto& auth = auth::AuthManager::instance();
    connect(&auth, &auth::AuthManager::login_succeeded, this, &LoginScreen::on_login_succeeded);
    connect(&auth, &auth::AuthManager::login_failed, this, &LoginScreen::on_login_failed);
    connect(&auth, &auth::AuthManager::login_mfa_required, this, &LoginScreen::on_mfa_required);
    connect(&auth, &auth::AuthManager::login_active_session, this, &LoginScreen::on_active_session);
    connect(&auth, &auth::AuthManager::mfa_verified, this, &LoginScreen::on_mfa_verified);
    connect(&auth, &auth::AuthManager::mfa_failed, this, &LoginScreen::on_mfa_failed);
}

void LoginScreen::build_login_page() {
    auto* page = new QWidget;
    page->setStyleSheet(CARD_STYLE);

    // Drop shadow
    auto* shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(40);
    shadow->setColor(QColor(0, 0, 0, 120));
    shadow->setOffset(0, 4);
    page->setGraphicsEffect(shadow);

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(32, 32, 32, 28);
    vl->setSpacing(14);

    // Title
    auto* title = new QLabel("Sign In");
    title->setStyleSheet(
        "color: #ffffff; font-size: 26px; font-weight: 300; background: transparent; "
        "font-family: 'Inter', 'Segoe UI Variable', sans-serif;");
    vl->addWidget(title);

    auto* subtitle = new QLabel("Access your Fincept Terminal account");
    subtitle->setStyleSheet(MUTED_STYLE);
    vl->addWidget(subtitle);

    vl->addSpacing(6);

    // Email
    auto* email_lbl = new QLabel("Email");
    email_lbl->setStyleSheet(LABEL_STYLE);
    vl->addWidget(email_lbl);

    email_input_ = new QLineEdit;
    email_input_->setPlaceholderText("Enter your email");
    email_input_->setFixedHeight(38);
    email_input_->setStyleSheet(INPUT_STYLE);
    vl->addWidget(email_input_);

    // Password
    auto* pw_lbl = new QLabel("Password");
    pw_lbl->setStyleSheet(LABEL_STYLE);
    vl->addWidget(pw_lbl);

    auto* pw_row = new QWidget;
    pw_row->setStyleSheet("background: transparent;");
    auto* prl = new QHBoxLayout(pw_row);
    prl->setContentsMargins(0, 0, 0, 0);
    prl->setSpacing(6);

    password_input_ = new QLineEdit;
    password_input_->setPlaceholderText("Enter your password");
    password_input_->setEchoMode(QLineEdit::Password);
    password_input_->setFixedHeight(38);
    password_input_->setStyleSheet(INPUT_STYLE);
    prl->addWidget(password_input_);

    show_pw_btn_ = new QPushButton("Show");
    show_pw_btn_->setFixedSize(52, 38);
    show_pw_btn_->setStyleSheet(
        "QPushButton { color: #666666; background: #1a1a1a; border: 1px solid #333333; "
        "border-radius: 4px; font-size: 12px; font-family: 'Inter', sans-serif; }"
        "QPushButton:hover { color: #999999; }");
    connect(show_pw_btn_, &QPushButton::clicked, this, [this]() {
        bool hidden = password_input_->echoMode() == QLineEdit::Password;
        password_input_->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
        show_pw_btn_->setText(hidden ? "Hide" : "Show");
    });
    prl->addWidget(show_pw_btn_);
    vl->addWidget(pw_row);

    // Error label
    error_label_ = new QLabel;
    error_label_->setWordWrap(true);
    error_label_->setStyleSheet(
        "color: #f87171; font-size: 13px; background: rgba(239,68,68,0.08); "
        "border: 1px solid rgba(239,68,68,0.25); border-radius: 4px; padding: 8px 12px; "
        "font-family: 'Inter', sans-serif;");
    error_label_->hide();
    vl->addWidget(error_label_);

    // Buttons row
    auto* btn_row = new QWidget;
    btn_row->setStyleSheet("background: transparent;");
    auto* brl = new QHBoxLayout(btn_row);
    brl->setContentsMargins(0, 0, 0, 0);

    auto* forgot_btn = new QPushButton("Forgot Password?");
    forgot_btn->setStyleSheet(LINK_STYLE);
    connect(forgot_btn, &QPushButton::clicked, this, &LoginScreen::navigate_forgot_password);
    brl->addWidget(forgot_btn);

    brl->addStretch();

    login_btn_ = new QPushButton("Sign In");
    login_btn_->setFixedHeight(38);
    login_btn_->setStyleSheet(BTN_PRIMARY);
    connect(login_btn_, &QPushButton::clicked, this, &LoginScreen::on_login);
    brl->addWidget(login_btn_);

    vl->addWidget(btn_row);

    // Separator
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #2a2a2a; border: none;");
    vl->addWidget(sep);

    // Register link
    auto* reg_row = new QWidget;
    reg_row->setStyleSheet("background: transparent;");
    auto* rrl = new QHBoxLayout(reg_row);
    rrl->setContentsMargins(0, 0, 0, 0);
    rrl->setAlignment(Qt::AlignCenter);

    auto* no_acct = new QLabel("Don't have an account?");
    no_acct->setStyleSheet(MUTED_STYLE);
    rrl->addWidget(no_acct);

    auto* signup_btn = new QPushButton("Sign Up");
    signup_btn->setStyleSheet(LINK_STYLE);
    connect(signup_btn, &QPushButton::clicked, this, &LoginScreen::navigate_register);
    rrl->addWidget(signup_btn);

    vl->addWidget(reg_row);

    // Enter key triggers
    connect(password_input_, &QLineEdit::returnPressed, this, &LoginScreen::on_login);
    connect(email_input_, &QLineEdit::returnPressed, this, [this]() { password_input_->setFocus(); });

    pages_->addWidget(page);
}

void LoginScreen::build_mfa_page() {
    mfa_page_ = new QWidget;
    mfa_page_->setStyleSheet(CARD_STYLE);

    auto* shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(40);
    shadow->setColor(QColor(0, 0, 0, 120));
    shadow->setOffset(0, 4);
    mfa_page_->setGraphicsEffect(shadow);

    auto* vl = new QVBoxLayout(mfa_page_);
    vl->setContentsMargins(32, 32, 32, 28);
    vl->setSpacing(14);

    auto* title = new QLabel("Two-Factor Authentication");
    title->setStyleSheet(
        "color: #ffffff; font-size: 24px; font-weight: 300; background: transparent; "
        "font-family: 'Inter', 'Segoe UI Variable', sans-serif;");
    vl->addWidget(title);

    auto* sub = new QLabel("Enter the 6-digit code from your authenticator app");
    sub->setWordWrap(true);
    sub->setStyleSheet(MUTED_STYLE);
    vl->addWidget(sub);

    vl->addSpacing(8);

    auto* lbl = new QLabel("Verification Code");
    lbl->setStyleSheet(LABEL_STYLE);
    vl->addWidget(lbl);

    mfa_input_ = new QLineEdit;
    mfa_input_->setPlaceholderText("000000");
    mfa_input_->setMaxLength(6);
    mfa_input_->setAlignment(Qt::AlignCenter);
    mfa_input_->setFixedHeight(44);
    mfa_input_->setStyleSheet(
        "QLineEdit { background: #1a1a1a; color: #e0e0e0; border: 1px solid #333333; "
        "border-radius: 4px; font-size: 22px; letter-spacing: 10px; "
        "font-family: 'Inter', 'Segoe UI Variable', sans-serif; }"
        "QLineEdit:focus { border: 1px solid #555555; }");
    vl->addWidget(mfa_input_);

    mfa_error_ = new QLabel;
    mfa_error_->setWordWrap(true);
    mfa_error_->setStyleSheet(
        "color: #f87171; font-size: 13px; background: rgba(239,68,68,0.08); "
        "border: 1px solid rgba(239,68,68,0.25); border-radius: 4px; padding: 8px; "
        "font-family: 'Inter', sans-serif;");
    mfa_error_->hide();
    vl->addWidget(mfa_error_);

    mfa_verify_btn_ = new QPushButton("Verify");
    mfa_verify_btn_->setFixedHeight(38);
    mfa_verify_btn_->setStyleSheet(
        "QPushButton { background: #1d4ed8; color: white; border: none; border-radius: 4px; "
        "font-size: 14px; font-family: 'Inter', sans-serif; }"
        "QPushButton:hover { background: #2563eb; }");
    connect(mfa_verify_btn_, &QPushButton::clicked, this, &LoginScreen::on_mfa_verify);
    vl->addWidget(mfa_verify_btn_);

    auto* back = new QPushButton("Back to Login");
    back->setStyleSheet(
        "QPushButton { color: #666666; background: transparent; border: none; font-size: 13px; "
        "font-family: 'Inter', sans-serif; }"
        "QPushButton:hover { color: #999999; }");
    connect(back, &QPushButton::clicked, this, [this]() { pages_->setCurrentIndex(0); });
    vl->addWidget(back, 0, Qt::AlignCenter);

    vl->addStretch();

    connect(mfa_input_, &QLineEdit::returnPressed, this, &LoginScreen::on_mfa_verify);

    pages_->addWidget(mfa_page_);
}

void LoginScreen::build_conflict_page() {
    conflict_page_ = new QWidget;
    conflict_page_->setStyleSheet(CARD_STYLE);

    auto* shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(40);
    shadow->setColor(QColor(0, 0, 0, 120));
    shadow->setOffset(0, 4);
    conflict_page_->setGraphicsEffect(shadow);

    auto* vl = new QVBoxLayout(conflict_page_);
    vl->setContentsMargins(32, 32, 32, 28);
    vl->setSpacing(14);

    auto* title = new QLabel("Active Session Detected");
    title->setStyleSheet(
        "color: #ffffff; font-size: 22px; font-weight: 600; background: transparent; "
        "font-family: 'Inter', 'Segoe UI Variable', sans-serif;");
    vl->addWidget(title);

    conflict_msg_ = new QLabel;
    conflict_msg_->setWordWrap(true);
    conflict_msg_->setStyleSheet(
        "color: #999999; font-size: 14px; background: transparent; "
        "font-family: 'Inter', 'Segoe UI Variable', sans-serif; line-height: 1.5;");
    vl->addWidget(conflict_msg_);

    vl->addSpacing(8);

    auto* force_btn = new QPushButton("Log Out Other Session && Continue");
    force_btn->setFixedHeight(38);
    force_btn->setStyleSheet(
        "QPushButton { background: #b45309; color: white; border: none; border-radius: 4px; "
        "font-size: 14px; font-weight: 500; font-family: 'Inter', sans-serif; }"
        "QPushButton:hover { background: #ca8a04; }"
        "QPushButton:disabled { background: #78350f; color: #a8a29e; }");
    connect(force_btn, &QPushButton::clicked, this, &LoginScreen::on_force_login);
    vl->addWidget(force_btn);

    auto* cancel_btn = new QPushButton("Cancel");
    cancel_btn->setFixedHeight(38);
    cancel_btn->setStyleSheet(
        "QPushButton { background: #1a1a1a; color: #999999; border: 1px solid #333333; "
        "border-radius: 4px; font-size: 14px; font-family: 'Inter', sans-serif; }"
        "QPushButton:hover { background: #222222; color: #cccccc; }");
    connect(cancel_btn, &QPushButton::clicked, this, [this]() { pages_->setCurrentIndex(0); });
    vl->addWidget(cancel_btn);

    vl->addStretch();

    pages_->addWidget(conflict_page_);
}

// ── Actions ──────────────────────────────────────────────────────────────────

void LoginScreen::on_login() {
    QString email = email_input_->text().trimmed();
    QString password = password_input_->text();

    auto v = auth::validate_email(email);
    if (!v.valid) { show_error(v.error); return; }
    if (password.isEmpty()) { show_error("Please enter your password"); return; }

    clear_error();
    set_loading(true);
    auth::AuthManager::instance().login(email, password);
}

void LoginScreen::on_mfa_verify() {
    QString code = mfa_input_->text().trimmed();
    if (code.isEmpty()) { mfa_error_->setText("Please enter the code"); mfa_error_->show(); return; }

    mfa_error_->hide();
    mfa_verify_btn_->setEnabled(false);
    auth::AuthManager::instance().verify_mfa(email_input_->text().trimmed(), code);
}

void LoginScreen::on_force_login() {
    for (auto* b : conflict_page_->findChildren<QPushButton*>()) {
        b->setEnabled(false);
    }
    auth::AuthManager::instance().login(email_input_->text().trimmed(), password_input_->text(), true);
}

// ── Slots from AuthManager signals ───────────────────────────────────────────

void LoginScreen::on_login_succeeded() {
    set_loading(false);
    pages_->setCurrentIndex(0);
    for (auto* b : conflict_page_->findChildren<QPushButton*>()) {
        b->setEnabled(true);
    }
}

void LoginScreen::on_login_failed(const QString& error) {
    set_loading(false);
    for (auto* b : conflict_page_->findChildren<QPushButton*>()) {
        b->setEnabled(true);
    }
    if (pages_->currentIndex() == 2) {
        conflict_msg_->setText(error);
    } else {
        show_error(error);
    }
}

void LoginScreen::on_mfa_required() {
    set_loading(false);
    mfa_input_->clear();
    mfa_error_->hide();
    pages_->setCurrentIndex(1);
    mfa_input_->setFocus();
}

void LoginScreen::on_active_session(const QString& msg) {
    set_loading(false);
    for (auto* b : conflict_page_->findChildren<QPushButton*>()) {
        b->setEnabled(true);
    }
    conflict_msg_->setText(msg);
    pages_->setCurrentIndex(2);
}

void LoginScreen::on_mfa_verified() {
    mfa_verify_btn_->setEnabled(true);
}

void LoginScreen::on_mfa_failed(const QString& error) {
    mfa_verify_btn_->setEnabled(true);
    mfa_error_->setText(error);
    mfa_error_->show();
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void LoginScreen::show_error(const QString& msg) {
    error_label_->setText(msg);
    error_label_->show();
}

void LoginScreen::clear_error() {
    error_label_->hide();
}

void LoginScreen::set_loading(bool loading) {
    login_btn_->setEnabled(!loading);
    email_input_->setEnabled(!loading);
    password_input_->setEnabled(!loading);
    login_btn_->setText(loading ? "Signing in..." : "Sign In");
}

} // namespace fincept::screens
