#include "screens/auth/LoginScreen.h"

#include "auth/AuthManager.h"
#include "auth/AuthTypes.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Obsidian Design System Styles ────────────────────────────────────────────

static QString card_style() {
    return QString("background: %1; border: 1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM());
}

static QString input_style() {
    return QString("QLineEdit {"
                   "  background: %1; color: %2;"
                   "  border: 1px solid %3;"
                   "  padding: 6px 10px; font-size: 15px;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "  selection-background-color: %4; selection-color: %5;"
                   "}"
                   "QLineEdit:focus { border: 1px solid %6; }"
                   "QLineEdit::placeholder { color: %7; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
             ui::colors::BG_BASE(), ui::colors::BORDER_BRIGHT(), ui::colors::TEXT_DIM());
}

static QString btn_primary() {
    return QString("QPushButton {"
                   "  background: rgba(217,119,6,0.1); color: %1;"
                   "  border: 1px solid %2;"
                   "  padding: 0 16px; font-size: 14px; font-weight: 700;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "}"
                   "QPushButton:hover { background: %1; color: %3; }"
                   "QPushButton:disabled { color: %4; background: %5; border-color: %6; }")
        .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_BASE(), ui::colors::TEXT_DIM(), ui::colors::BG_RAISED(),
             ui::colors::BORDER_DIM());
}

static QString btn_standard() {
    return QString("QPushButton {"
                   "  background: %1; color: %2;"
                   "  border: 1px solid %3;"
                   "  padding: 0 12px; font-size: 14px; font-weight: 700;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "}"
                   "QPushButton:hover { color: %4; background: %5; }")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BG_HOVER());
}

static QString btn_danger() {
    return QString("QPushButton {"
                   "  background: rgba(220,38,38,0.1); color: %1;"
                   "  border: 1px solid #7f1d1d;"
                   "  padding: 0 16px; font-size: 14px; font-weight: 700;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "}"
                   "QPushButton:hover { background: %1; color: %2; }"
                   "QPushButton:disabled { color: %3; background: %4; border-color: %5; }")
        .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_DIM(), ui::colors::BG_RAISED(),
             ui::colors::BORDER_DIM());
}

static QString link_style() {
    return QString("QPushButton {"
                   "  color: %1; background: transparent; border: none;"
                   "  font-size: 13px; font-weight: 400;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "}"
                   "QPushButton:hover { color: %2; }")
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_PRIMARY());
}

static QString label_style() {
    return QString("color: %1; font-size: 13px; font-weight: 700;"
                   "background: transparent; letter-spacing: 0.5px;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_SECONDARY());
}

static QString muted_style() {
    return QString("color: %1; font-size: 13px; background: transparent;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_TERTIARY());
}

static QFrame* make_separator() {
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_DIM()));
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
    pages_->setMinimumWidth(480);
    pages_->setMaximumWidth(620);
    pages_->setStyleSheet("background: transparent;");

    build_login_page();
    build_mfa_page();
    build_conflict_page();

    overlay->addWidget(pages_, 0, Qt::AlignCenter);

    auto& auth = auth::AuthManager::instance();
    connect(&auth, &auth::AuthManager::login_succeeded, this, &LoginScreen::on_login_succeeded);
    connect(&auth, &auth::AuthManager::login_failed, this, &LoginScreen::on_login_failed);
    connect(&auth, &auth::AuthManager::login_mfa_required, this, &LoginScreen::on_mfa_required);
    connect(&auth, &auth::AuthManager::login_active_session, this, &LoginScreen::on_active_session);
    connect(&auth, &auth::AuthManager::mfa_verified, this, &LoginScreen::on_mfa_verified);
    connect(&auth, &auth::AuthManager::mfa_failed, this, &LoginScreen::on_mfa_failed);
}

// ── Background Paint ─────────────────────────────────────────────────────────

void LoginScreen::hideEvent(QHideEvent* event) {
    // Wipe credential + MFA inputs whenever this screen leaves the stack so
    // they can't be recovered or pre-filled after a logout/navigation.
    if (email_input_) email_input_->clear();
    if (password_input_) {
        password_input_->clear();
        password_input_->setEchoMode(QLineEdit::Password);
    }
    if (mfa_input_) mfa_input_->clear();
    clear_error();
    if (mfa_error_) mfa_error_->hide();
    if (pages_) pages_->setCurrentIndex(0);
    QWidget::hideEvent(event);
}

void LoginScreen::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.fillRect(rect(), QColor(ui::colors::BG_BASE()));

    p.setPen(QPen(QColor(ui::colors::BG_RAISED()), 1));
    const int step = 24;
    for (int x = 0; x < width(); x += step)
        p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += step)
        p.drawLine(0, y, width(), y);

    int cx = width() / 2;
    int cy = height() / 2;
    p.setPen(QPen(QColor(ui::colors::BORDER_DIM()), 1));
    p.drawLine(cx - 60, cy, cx + 60, cy);
    p.drawLine(cx, cy - 60, cx, cy + 60);
}

// ── Login Page ───────────────────────────────────────────────────────────────

void LoginScreen::build_login_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    // Header bar
    auto* header = new QWidget(this);
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* title = new QLabel("SIGN IN");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                 "background: transparent; letter-spacing: 1px;"
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::AMBER()));
    hl->addWidget(title);
    hl->addStretch();

    auto* brand = new QLabel("FINCEPT");
    brand->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                 "background: transparent; letter-spacing: 0.5px;"
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::TEXT_DIM()));
    hl->addWidget(brand);
    vl->addWidget(header);

    auto* subtitle = new QLabel("Access your terminal account");
    subtitle->setStyleSheet(muted_style());
    vl->addWidget(subtitle);

    vl->addWidget(make_separator());

    // Email
    auto* email_lbl = new QLabel("EMAIL");
    email_lbl->setStyleSheet(label_style());
    vl->addWidget(email_lbl);

    email_input_ = new QLineEdit;
    email_input_->setPlaceholderText("user@domain.com");
    email_input_->setFixedHeight(34);
    email_input_->setStyleSheet(input_style());
    vl->addWidget(email_input_);

    // Password
    auto* pw_lbl = new QLabel("PASSWORD");
    pw_lbl->setStyleSheet(label_style());
    vl->addWidget(pw_lbl);

    auto* pw_row = new QWidget(this);
    pw_row->setStyleSheet("background: transparent;");
    auto* prl = new QHBoxLayout(pw_row);
    prl->setContentsMargins(0, 0, 0, 0);
    prl->setSpacing(4);

    password_input_ = new QLineEdit;
    password_input_->setPlaceholderText("enter password");
    password_input_->setEchoMode(QLineEdit::Password);
    password_input_->setFixedHeight(34);
    password_input_->setStyleSheet(input_style());
    prl->addWidget(password_input_);

    show_pw_btn_ = new QPushButton("SHOW");
    show_pw_btn_->setFixedSize(58, 34);
    show_pw_btn_->setStyleSheet(btn_standard());
    connect(show_pw_btn_, &QPushButton::clicked, this, [this]() {
        bool hidden = password_input_->echoMode() == QLineEdit::Password;
        password_input_->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
        show_pw_btn_->setText(hidden ? "HIDE" : "SHOW");
    });
    prl->addWidget(show_pw_btn_);
    vl->addWidget(pw_row);

    // Error
    error_label_ = new QLabel;
    error_label_->setWordWrap(true);
    error_label_->setStyleSheet(QString("color: %1; font-size: 13px;"
                                        "background: rgba(220,38,38,0.08);"
                                        "border: 1px solid #7f1d1d; padding: 6px 8px;"
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::NEGATIVE()));
    error_label_->hide();
    vl->addWidget(error_label_);

    // Buttons
    auto* btn_row = new QWidget(this);
    btn_row->setStyleSheet("background: transparent;");
    auto* brl = new QHBoxLayout(btn_row);
    brl->setContentsMargins(0, 0, 0, 0);

    auto* forgot_btn = new QPushButton("FORGOT PASSWORD?");
    forgot_btn->setStyleSheet(link_style());
    connect(forgot_btn, &QPushButton::clicked, this, &LoginScreen::navigate_forgot_password);
    brl->addWidget(forgot_btn);
    brl->addStretch();

    login_btn_ = new QPushButton("  SIGN IN  ");
    login_btn_->setFixedHeight(32);
    login_btn_->setStyleSheet(btn_primary());
    connect(login_btn_, &QPushButton::clicked, this, &LoginScreen::on_login);
    brl->addWidget(login_btn_);
    vl->addWidget(btn_row);

    vl->addWidget(make_separator());

    // Register link
    auto* reg_row = new QWidget(this);
    reg_row->setStyleSheet("background: transparent;");
    auto* rrl = new QHBoxLayout(reg_row);
    rrl->setContentsMargins(0, 0, 0, 0);
    rrl->setAlignment(Qt::AlignCenter);

    auto* no_acct = new QLabel("No account?");
    no_acct->setStyleSheet(muted_style());
    rrl->addWidget(no_acct);

    auto* signup_btn = new QPushButton("SIGN UP");
    signup_btn->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none;"
                                      "  font-size: 13px; font-weight: 700;"
                                      "  font-family: 'Consolas','Courier New',monospace; }"
                                      "QPushButton:hover { color: %2; }")
                                  .arg(ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));
    connect(signup_btn, &QPushButton::clicked, this, &LoginScreen::navigate_register);
    rrl->addWidget(signup_btn);
    vl->addWidget(reg_row);

    connect(password_input_, &QLineEdit::returnPressed, this, &LoginScreen::on_login);
    connect(email_input_, &QLineEdit::returnPressed, this, [this]() { password_input_->setFocus(); });

    pages_->addWidget(page);
}

// ── MFA Page ─────────────────────────────────────────────────────────────────

void LoginScreen::build_mfa_page() {
    mfa_page_ = new QWidget(this);
    mfa_page_->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(mfa_page_);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    auto* header = new QWidget(this);
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* title = new QLabel("TWO-FACTOR AUTH");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                 "background: transparent; letter-spacing: 1px;"
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::AMBER()));
    hl->addWidget(title);
    hl->addStretch();

    auto* indicator = new QLabel("SECURE");
    indicator->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                     "background: transparent; letter-spacing: 0.5px;"
                                     "font-family: 'Consolas','Courier New',monospace;")
                                 .arg(ui::colors::POSITIVE()));
    hl->addWidget(indicator);
    vl->addWidget(header);

    auto* sub = new QLabel("Enter the 6-digit code from your authenticator");
    sub->setWordWrap(true);
    sub->setStyleSheet(muted_style());
    vl->addWidget(sub);

    vl->addWidget(make_separator());

    auto* lbl = new QLabel("VERIFICATION CODE");
    lbl->setStyleSheet(label_style());
    vl->addWidget(lbl);

    mfa_input_ = new QLineEdit;
    mfa_input_->setPlaceholderText("000000");
    mfa_input_->setMaxLength(6);
    mfa_input_->setAlignment(Qt::AlignCenter);
    mfa_input_->setFixedHeight(40);
    mfa_input_->setStyleSheet(QString("QLineEdit {"
                                      "  background: %1; color: %2;"
                                      "  border: 1px solid %3;"
                                      "  font-size: 20px; letter-spacing: 10px;"
                                      "  font-family: 'Consolas','Courier New',monospace;"
                                      "  selection-background-color: %4; selection-color: %5;"
                                      "}"
                                      "QLineEdit:focus { border: 1px solid %6; }")
                                  .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                       ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::BORDER_BRIGHT()));
    vl->addWidget(mfa_input_);

    mfa_error_ = new QLabel;
    mfa_error_->setWordWrap(true);
    mfa_error_->setStyleSheet(QString("color: %1; font-size: 13px;"
                                      "background: rgba(220,38,38,0.08);"
                                      "border: 1px solid #7f1d1d; padding: 6px 8px;"
                                      "font-family: 'Consolas','Courier New',monospace;")
                                  .arg(ui::colors::NEGATIVE()));
    mfa_error_->hide();
    vl->addWidget(mfa_error_);

    mfa_verify_btn_ = new QPushButton("  VERIFY  ");
    mfa_verify_btn_->setFixedHeight(32);
    mfa_verify_btn_->setStyleSheet(btn_primary());
    connect(mfa_verify_btn_, &QPushButton::clicked, this, &LoginScreen::on_mfa_verify);
    vl->addWidget(mfa_verify_btn_);

    auto* back = new QPushButton("BACK TO LOGIN");
    back->setStyleSheet(link_style());
    connect(back, &QPushButton::clicked, this, [this]() { pages_->setCurrentIndex(0); });
    vl->addWidget(back, 0, Qt::AlignCenter);

    vl->addStretch();
    connect(mfa_input_, &QLineEdit::returnPressed, this, &LoginScreen::on_mfa_verify);
    pages_->addWidget(mfa_page_);
}

// ── Conflict Page ────────────────────────────────────────────────────────────

void LoginScreen::build_conflict_page() {
    conflict_page_ = new QWidget(this);
    conflict_page_->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(conflict_page_);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    auto* header = new QWidget(this);
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* title = new QLabel("SESSION CONFLICT");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                 "background: transparent; letter-spacing: 1px;"
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::WARNING()));
    hl->addWidget(title);
    hl->addStretch();

    auto* warn = new QLabel("WARNING");
    warn->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                "background: transparent; letter-spacing: 0.5px;"
                                "font-family: 'Consolas','Courier New',monospace;")
                            .arg(ui::colors::WARNING()));
    hl->addWidget(warn);
    vl->addWidget(header);

    conflict_msg_ = new QLabel;
    conflict_msg_->setWordWrap(true);
    conflict_msg_->setStyleSheet(QString("color: %1; font-size: 14px; background: transparent;"
                                         "font-family: 'Consolas','Courier New',monospace;")
                                     .arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(conflict_msg_);

    vl->addWidget(make_separator());

    auto* force_btn = new QPushButton("  LOG OUT OTHER SESSION & CONTINUE  ");
    force_btn->setFixedHeight(32);
    force_btn->setStyleSheet(btn_danger());
    connect(force_btn, &QPushButton::clicked, this, &LoginScreen::on_force_login);
    vl->addWidget(force_btn);

    auto* cancel_btn = new QPushButton("  CANCEL  ");
    cancel_btn->setFixedHeight(32);
    cancel_btn->setStyleSheet(btn_standard());
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
    for (auto* b : conflict_page_->findChildren<QPushButton*>())
        b->setEnabled(false);
    auth::AuthManager::instance().login(email_input_->text().trimmed(), password_input_->text(), true);
}

// ── Slots ────────────────────────────────────────────────────────────────────

void LoginScreen::on_login_succeeded() {
    set_loading(false);
    pages_->setCurrentIndex(0);
    for (auto* b : conflict_page_->findChildren<QPushButton*>())
        b->setEnabled(true);
}

void LoginScreen::on_login_failed(const QString& error) {
    set_loading(false);
    for (auto* b : conflict_page_->findChildren<QPushButton*>())
        b->setEnabled(true);
    if (pages_->currentIndex() == 2)
        conflict_msg_->setText(error);
    else
        show_error(error);
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
    for (auto* b : conflict_page_->findChildren<QPushButton*>())
        b->setEnabled(true);
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
