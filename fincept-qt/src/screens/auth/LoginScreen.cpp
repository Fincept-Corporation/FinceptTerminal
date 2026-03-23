#include "screens/auth/LoginScreen.h"

#include "auth/AuthManager.h"
#include "auth/AuthTypes.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Obsidian Design System Styles ────────────────────────────────────────────
// Zero rounded corners, no shadows, monospace, hairline borders.

static const char* FONT = "'Consolas','Courier New',monospace";

static const char* CARD_STYLE =
    "background: #0a0a0a;"
    "border: 1px solid #1a1a1a;";

static const char* INPUT_STYLE =
    "QLineEdit {"
    "  background: #0a0a0a; color: #e5e5e5;"
    "  border: 1px solid #1a1a1a;"
    "  padding: 8px 12px; font-size: 16px;"
    "  font-family: 'Consolas','Courier New',monospace;"
    "  selection-background-color: #d97706; selection-color: #080808;"
    "}"
    "QLineEdit:focus { border: 1px solid #333333; }"
    "QLineEdit::placeholder { color: #404040; }";

static const char* BTN_PRIMARY =
    "QPushButton {"
    "  background: rgba(217,119,6,0.1); color: #d97706;"
    "  border: 1px solid #78350f;"
    "  padding: 0 20px; font-size: 15px; font-weight: 700;"
    "  font-family: 'Consolas','Courier New',monospace;"
    "}"
    "QPushButton:hover { background: #d97706; color: #080808; }"
    "QPushButton:disabled { color: #404040; background: #111111; border-color: #1a1a1a; }";

static const char* BTN_STANDARD =
    "QPushButton {"
    "  background: #111111; color: #808080;"
    "  border: 1px solid #1a1a1a;"
    "  padding: 0 16px; font-size: 15px; font-weight: 700;"
    "  font-family: 'Consolas','Courier New',monospace;"
    "}"
    "QPushButton:hover { color: #e5e5e5; background: #161616; }";

static const char* BTN_DANGER =
    "QPushButton {"
    "  background: rgba(220,38,38,0.1); color: #dc2626;"
    "  border: 1px solid #7f1d1d;"
    "  padding: 0 20px; font-size: 15px; font-weight: 700;"
    "  font-family: 'Consolas','Courier New',monospace;"
    "}"
    "QPushButton:hover { background: #dc2626; color: #e5e5e5; }"
    "QPushButton:disabled { color: #404040; background: #111111; border-color: #1a1a1a; }";

static const char* LINK_STYLE =
    "QPushButton {"
    "  color: #808080; background: transparent; border: none;"
    "  font-size: 14px; font-weight: 400;"
    "  font-family: 'Consolas','Courier New',monospace;"
    "}"
    "QPushButton:hover { color: #e5e5e5; }";

static const char* LABEL_STYLE =
    "color: #808080; font-size: 14px; font-weight: 700;"
    "background: transparent; letter-spacing: 0.5px;"
    "font-family: 'Consolas','Courier New',monospace;";

static const char* MUTED_STYLE =
    "color: #525252; font-size: 14px; background: transparent;"
    "font-family: 'Consolas','Courier New',monospace;";

// ── Helpers ──────────────────────────────────────────────────────────────────

static QFrame* make_separator() {
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #1a1a1a; border: none;");
    return sep;
}

// ── Constructor ──────────────────────────────────────────────────────────────

LoginScreen::LoginScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* overlay = new QVBoxLayout;
    overlay->setAlignment(Qt::AlignCenter);
    root->addLayout(overlay, 1);

    pages_ = new QStackedWidget;
    pages_->setMinimumWidth(600);
    pages_->setMaximumWidth(840);
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

// ── Background Paint ─────────────────────────────────────────────────────────

void LoginScreen::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.fillRect(rect(), QColor(0x08, 0x08, 0x08)); // BG_BASE

    // Subtle grid pattern — terminal aesthetic
    p.setPen(QPen(QColor(0x12, 0x12, 0x12), 1));
    const int step = 24;
    for (int x = 0; x < width(); x += step)
        p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += step)
        p.drawLine(0, y, width(), y);

    // Subtle crosshair at center
    int cx = width() / 2;
    int cy = height() / 2;
    p.setPen(QPen(QColor(0x1a, 0x1a, 0x1a), 1));
    p.drawLine(cx - 60, cy, cx + 60, cy);
    p.drawLine(cx, cy - 60, cx, cy + 60);
}

// ── Login Page ───────────────────────────────────────────────────────────────

void LoginScreen::build_login_page() {
    auto* page = new QWidget;
    page->setStyleSheet(CARD_STYLE);

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(36, 28, 36, 28);
    vl->setSpacing(14);

    // ── Header bar ───────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setFixedHeight(44);
    header->setStyleSheet("background: #111111; border: none;");
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* title = new QLabel("SIGN IN");
    title->setStyleSheet(
        "color: #d97706; font-size: 16px; font-weight: 700;"
        "background: transparent; letter-spacing: 1px;"
        "font-family: 'Consolas','Courier New',monospace;");
    hl->addWidget(title);
    hl->addStretch();

    auto* brand = new QLabel("FINCEPT");
    brand->setStyleSheet(
        "color: #404040; font-size: 12px; font-weight: 700;"
        "background: transparent; letter-spacing: 0.5px;"
        "font-family: 'Consolas','Courier New',monospace;");
    hl->addWidget(brand);

    vl->addWidget(header);

    // ── Subtitle ─────────────────────────────────────────────────────────
    auto* subtitle = new QLabel("Access your terminal account");
    subtitle->setStyleSheet(MUTED_STYLE);
    vl->addWidget(subtitle);

    vl->addWidget(make_separator());
    vl->addSpacing(4);

    // ── Email ────────────────────────────────────────────────────────────
    auto* email_lbl = new QLabel("EMAIL");
    email_lbl->setStyleSheet(LABEL_STYLE);
    vl->addWidget(email_lbl);

    email_input_ = new QLineEdit;
    email_input_->setPlaceholderText("user@domain.com");
    email_input_->setFixedHeight(40);
    email_input_->setStyleSheet(INPUT_STYLE);
    vl->addWidget(email_input_);

    vl->addSpacing(4);

    // ── Password ─────────────────────────────────────────────────────────
    auto* pw_lbl = new QLabel("PASSWORD");
    pw_lbl->setStyleSheet(LABEL_STYLE);
    vl->addWidget(pw_lbl);

    auto* pw_row = new QWidget;
    pw_row->setStyleSheet("background: transparent;");
    auto* prl = new QHBoxLayout(pw_row);
    prl->setContentsMargins(0, 0, 0, 0);
    prl->setSpacing(4);

    password_input_ = new QLineEdit;
    password_input_->setPlaceholderText("enter password");
    password_input_->setEchoMode(QLineEdit::Password);
    password_input_->setFixedHeight(40);
    password_input_->setStyleSheet(INPUT_STYLE);
    prl->addWidget(password_input_);

    show_pw_btn_ = new QPushButton("SHOW");
    show_pw_btn_->setFixedSize(64, 40);
    show_pw_btn_->setStyleSheet(BTN_STANDARD);
    connect(show_pw_btn_, &QPushButton::clicked, this, [this]() {
        bool hidden = password_input_->echoMode() == QLineEdit::Password;
        password_input_->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
        show_pw_btn_->setText(hidden ? "HIDE" : "SHOW");
    });
    prl->addWidget(show_pw_btn_);
    vl->addWidget(pw_row);

    // ── Error label ──────────────────────────────────────────────────────
    error_label_ = new QLabel;
    error_label_->setWordWrap(true);
    error_label_->setStyleSheet(
        "color: #dc2626; font-size: 13px;"
        "background: rgba(220,38,38,0.08);"
        "border: 1px solid #7f1d1d;"
        "padding: 6px 8px;"
        "font-family: 'Consolas','Courier New',monospace;");
    error_label_->hide();
    vl->addWidget(error_label_);

    vl->addSpacing(4);

    // ── Button row ───────────────────────────────────────────────────────
    auto* btn_row = new QWidget;
    btn_row->setStyleSheet("background: transparent;");
    auto* brl = new QHBoxLayout(btn_row);
    brl->setContentsMargins(0, 0, 0, 0);

    auto* forgot_btn = new QPushButton("FORGOT PASSWORD?");
    forgot_btn->setStyleSheet(LINK_STYLE);
    connect(forgot_btn, &QPushButton::clicked, this, &LoginScreen::navigate_forgot_password);
    brl->addWidget(forgot_btn);

    brl->addStretch();

    login_btn_ = new QPushButton("  SIGN IN  ");
    login_btn_->setFixedHeight(38);
    login_btn_->setStyleSheet(BTN_PRIMARY);
    connect(login_btn_, &QPushButton::clicked, this, &LoginScreen::on_login);
    brl->addWidget(login_btn_);

    vl->addWidget(btn_row);

    vl->addWidget(make_separator());

    // ── Register link ────────────────────────────────────────────────────
    auto* reg_row = new QWidget;
    reg_row->setStyleSheet("background: transparent;");
    auto* rrl = new QHBoxLayout(reg_row);
    rrl->setContentsMargins(0, 0, 0, 0);
    rrl->setAlignment(Qt::AlignCenter);

    auto* no_acct = new QLabel("No account?");
    no_acct->setStyleSheet(MUTED_STYLE);
    rrl->addWidget(no_acct);

    auto* signup_btn = new QPushButton("SIGN UP");
    signup_btn->setStyleSheet(
        "QPushButton {"
        "  color: #d97706; background: transparent; border: none;"
        "  font-size: 13px; font-weight: 700;"
        "  font-family: 'Consolas','Courier New',monospace;"
        "}"
        "QPushButton:hover { color: #e5e5e5; }");
    connect(signup_btn, &QPushButton::clicked, this, &LoginScreen::navigate_register);
    rrl->addWidget(signup_btn);

    vl->addWidget(reg_row);

    // ── Enter key triggers ───────────────────────────────────────────────
    connect(password_input_, &QLineEdit::returnPressed, this, &LoginScreen::on_login);
    connect(email_input_, &QLineEdit::returnPressed, this, [this]() { password_input_->setFocus(); });

    pages_->addWidget(page);
}

// ── MFA Page ─────────────────────────────────────────────────────────────────

void LoginScreen::build_mfa_page() {
    mfa_page_ = new QWidget;
    mfa_page_->setStyleSheet(CARD_STYLE);

    auto* vl = new QVBoxLayout(mfa_page_);
    vl->setContentsMargins(20, 16, 20, 16);
    vl->setSpacing(8);

    // Header bar
    auto* header = new QWidget;
    header->setFixedHeight(34);
    header->setStyleSheet("background: #111111; border: none;");
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* title = new QLabel("TWO-FACTOR AUTH");
    title->setStyleSheet(
        "color: #d97706; font-size: 16px; font-weight: 700;"
        "background: transparent; letter-spacing: 1px;"
        "font-family: 'Consolas','Courier New',monospace;");
    hl->addWidget(title);
    hl->addStretch();

    auto* indicator = new QLabel("SECURE");
    indicator->setStyleSheet(
        "color: #16a34a; font-size: 12px; font-weight: 700;"
        "background: transparent; letter-spacing: 0.5px;"
        "font-family: 'Consolas','Courier New',monospace;");
    hl->addWidget(indicator);

    vl->addWidget(header);

    auto* sub = new QLabel("Enter the 6-digit code from your authenticator");
    sub->setWordWrap(true);
    sub->setStyleSheet(MUTED_STYLE);
    vl->addWidget(sub);

    vl->addWidget(make_separator());
    vl->addSpacing(4);

    auto* lbl = new QLabel("VERIFICATION CODE");
    lbl->setStyleSheet(LABEL_STYLE);
    vl->addWidget(lbl);

    mfa_input_ = new QLineEdit;
    mfa_input_->setPlaceholderText("000000");
    mfa_input_->setMaxLength(6);
    mfa_input_->setAlignment(Qt::AlignCenter);
    mfa_input_->setFixedHeight(48);
    mfa_input_->setStyleSheet(
        "QLineEdit {"
        "  background: #0a0a0a; color: #e5e5e5;"
        "  border: 1px solid #1a1a1a;"
        "  font-size: 20px; letter-spacing: 10px;"
        "  font-family: 'Consolas','Courier New',monospace;"
        "  selection-background-color: #d97706; selection-color: #080808;"
        "}"
        "QLineEdit:focus { border: 1px solid #333333; }");
    vl->addWidget(mfa_input_);

    mfa_error_ = new QLabel;
    mfa_error_->setWordWrap(true);
    mfa_error_->setStyleSheet(
        "color: #dc2626; font-size: 13px;"
        "background: rgba(220,38,38,0.08);"
        "border: 1px solid #7f1d1d; padding: 6px 8px;"
        "font-family: 'Consolas','Courier New',monospace;");
    mfa_error_->hide();
    vl->addWidget(mfa_error_);

    vl->addSpacing(4);

    mfa_verify_btn_ = new QPushButton("  VERIFY  ");
    mfa_verify_btn_->setFixedHeight(38);
    mfa_verify_btn_->setStyleSheet(BTN_PRIMARY);
    connect(mfa_verify_btn_, &QPushButton::clicked, this, &LoginScreen::on_mfa_verify);
    vl->addWidget(mfa_verify_btn_);

    auto* back = new QPushButton("BACK TO LOGIN");
    back->setStyleSheet(LINK_STYLE);
    connect(back, &QPushButton::clicked, this, [this]() { pages_->setCurrentIndex(0); });
    vl->addWidget(back, 0, Qt::AlignCenter);

    vl->addStretch();

    connect(mfa_input_, &QLineEdit::returnPressed, this, &LoginScreen::on_mfa_verify);

    pages_->addWidget(mfa_page_);
}

// ── Conflict Page ────────────────────────────────────────────────────────────

void LoginScreen::build_conflict_page() {
    conflict_page_ = new QWidget;
    conflict_page_->setStyleSheet(CARD_STYLE);

    auto* vl = new QVBoxLayout(conflict_page_);
    vl->setContentsMargins(20, 16, 20, 16);
    vl->setSpacing(8);

    // Header bar
    auto* header = new QWidget;
    header->setFixedHeight(34);
    header->setStyleSheet("background: #111111; border: none;");
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* title = new QLabel("SESSION CONFLICT");
    title->setStyleSheet(
        "color: #ca8a04; font-size: 16px; font-weight: 700;"
        "background: transparent; letter-spacing: 1px;"
        "font-family: 'Consolas','Courier New',monospace;");
    hl->addWidget(title);
    hl->addStretch();

    auto* warn = new QLabel("WARNING");
    warn->setStyleSheet(
        "color: #ca8a04; font-size: 12px; font-weight: 700;"
        "background: transparent; letter-spacing: 0.5px;"
        "font-family: 'Consolas','Courier New',monospace;");
    hl->addWidget(warn);

    vl->addWidget(header);

    conflict_msg_ = new QLabel;
    conflict_msg_->setWordWrap(true);
    conflict_msg_->setStyleSheet(
        "color: #808080; font-size: 14px; background: transparent;"
        "font-family: 'Consolas','Courier New',monospace;");
    vl->addWidget(conflict_msg_);

    vl->addWidget(make_separator());
    vl->addSpacing(4);

    auto* force_btn = new QPushButton("  LOG OUT OTHER SESSION & CONTINUE  ");
    force_btn->setFixedHeight(38);
    force_btn->setStyleSheet(BTN_DANGER);
    connect(force_btn, &QPushButton::clicked, this, &LoginScreen::on_force_login);
    vl->addWidget(force_btn);

    auto* cancel_btn = new QPushButton("  CANCEL  ");
    cancel_btn->setFixedHeight(38);
    cancel_btn->setStyleSheet(BTN_STANDARD);
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
    if (!v.valid) {
        show_error(v.error);
        return;
    }
    if (password.isEmpty()) {
        show_error("Please enter your password");
        return;
    }

    clear_error();
    set_loading(true);
    auth::AuthManager::instance().login(email, password);
}

void LoginScreen::on_mfa_verify() {
    QString code = mfa_input_->text().trimmed();
    if (code.isEmpty()) {
        mfa_error_->setText("Please enter the code");
        mfa_error_->show();
        return;
    }

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
    login_btn_->setText(loading ? "  SIGNING IN...  " : "  SIGN IN  ");
}

} // namespace fincept::screens
