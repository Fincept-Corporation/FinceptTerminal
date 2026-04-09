#include "screens/auth/ForgotPasswordScreen.h"

#include "auth/AuthManager.h"
#include "auth/AuthTypes.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Obsidian Styles ──────────────────────────────────────────────────────────

static QString card_style() {
    return QString("background: %1; border: 1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM);
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
        .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::AMBER,
             ui::colors::BG_BASE, ui::colors::BORDER_BRIGHT, ui::colors::TEXT_DIM);
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
        .arg(ui::colors::AMBER, ui::colors::AMBER_DIM, ui::colors::BG_BASE, ui::colors::TEXT_DIM, ui::colors::BG_RAISED,
             ui::colors::BORDER_DIM);
}

static QString btn_standard() {
    return QString("QPushButton {"
                   "  background: %1; color: %2;"
                   "  border: 1px solid %3;"
                   "  padding: 0 12px; font-size: 14px; font-weight: 700;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "}"
                   "QPushButton:hover { color: %4; background: %5; }")
        .arg(ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY,
             ui::colors::BG_HOVER);
}

static QString link_style() {
    return QString("QPushButton {"
                   "  color: %1; background: transparent; border: none;"
                   "  font-size: 13px; font-weight: 400;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "}"
                   "QPushButton:hover { color: %2; }")
        .arg(ui::colors::TEXT_SECONDARY, ui::colors::TEXT_PRIMARY);
}

static QString label_style() {
    return QString("color: %1; font-size: 13px; font-weight: 700;"
                   "background: transparent; letter-spacing: 0.5px;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_SECONDARY);
}

static QString muted_style() {
    return QString("color: %1; font-size: 13px; background: transparent;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_TERTIARY);
}

static QFrame* make_separator() {
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_DIM));
    return sep;
}

// ── Constructor ──────────────────────────────────────────────────────────────

ForgotPasswordScreen::ForgotPasswordScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* overlay = new QVBoxLayout;
    overlay->setAlignment(Qt::AlignCenter);
    root->addLayout(overlay, 1);

    pages_ = new QStackedWidget;
    pages_->setMinimumWidth(480);
    pages_->setMaximumWidth(620);
    pages_->setStyleSheet("background: transparent;");

    build_email_page();
    build_otp_sent_page();
    build_reset_page();
    build_success_page();

    overlay->addWidget(pages_, 0, Qt::AlignCenter);

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

// ── Background ───────────────────────────────────────────────────────────────

void ForgotPasswordScreen::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.fillRect(rect(), QColor(ui::colors::BG_BASE()));

    p.setPen(QPen(QColor(ui::colors::BG_RAISED()), 1));
    const int step = 24;
    for (int x = 0; x < width(); x += step)
        p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += step)
        p.drawLine(0, y, width(), y);
}

// ── Email Page ───────────────────────────────────────────────────────────────

void ForgotPasswordScreen::build_email_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    // Header
    auto* header = new QWidget(this);
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* back = new QPushButton("<");
    back->setFixedSize(24, 24);
    back->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none;"
                                "  font-size: 16px; font-family: 'Consolas','Courier New',monospace; }"
                                "QPushButton:hover { color: %2; }")
                            .arg(ui::colors::TEXT_SECONDARY, ui::colors::TEXT_PRIMARY));
    connect(back, &QPushButton::clicked, this, &ForgotPasswordScreen::navigate_login);
    hl->addWidget(back);

    auto* title = new QLabel("RESET PASSWORD");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                 "background: transparent; letter-spacing: 1px;"
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::AMBER));
    hl->addWidget(title);
    hl->addStretch();
    vl->addWidget(header);

    auto* sub = new QLabel("Enter your email and we'll send a verification code.");
    sub->setWordWrap(true);
    sub->setStyleSheet(muted_style());
    vl->addWidget(sub);

    vl->addWidget(make_separator());

    auto* lbl = new QLabel("EMAIL");
    lbl->setStyleSheet(label_style());
    vl->addWidget(lbl);

    email_input_ = new QLineEdit;
    email_input_->setPlaceholderText("user@domain.com");
    email_input_->setFixedHeight(34);
    email_input_->setStyleSheet(input_style());
    vl->addWidget(email_input_);

    error_label_ = new QLabel;
    error_label_->setWordWrap(true);
    error_label_->setStyleSheet(QString("color: %1; font-size: 13px;"
                                        "background: rgba(220,38,38,0.08);"
                                        "border: 1px solid #7f1d1d; padding: 6px 8px;"
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::NEGATIVE));
    error_label_->hide();
    vl->addWidget(error_label_);

    auto* send_btn = new QPushButton("  SEND CODE  ");
    send_btn->setFixedHeight(32);
    send_btn->setStyleSheet(btn_primary());
    connect(send_btn, &QPushButton::clicked, this, &ForgotPasswordScreen::on_send_code);
    connect(email_input_, &QLineEdit::returnPressed, this, &ForgotPasswordScreen::on_send_code);
    vl->addWidget(send_btn);

    vl->addWidget(make_separator());

    auto* back_login = new QPushButton("REMEMBER YOUR PASSWORD? SIGN IN");
    back_login->setStyleSheet(link_style());
    connect(back_login, &QPushButton::clicked, this, &ForgotPasswordScreen::navigate_login);
    vl->addWidget(back_login, 0, Qt::AlignCenter);

    vl->addStretch();
    pages_->addWidget(page);
}

// ── OTP Sent Page ────────────────────────────────────────────────────────────

void ForgotPasswordScreen::build_otp_sent_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    auto* header = new QWidget(this);
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* title = new QLabel("CHECK YOUR EMAIL");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                 "background: transparent; letter-spacing: 1px;"
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::AMBER));
    hl->addWidget(title);
    hl->addStretch();
    vl->addWidget(header);

    auto* sub = new QLabel("We've sent a verification code. Enter it on the next screen to reset your password.");
    sub->setWordWrap(true);
    sub->setStyleSheet(muted_style());
    vl->addWidget(sub);

    vl->addWidget(make_separator());

    auto* cont_btn = new QPushButton("  I HAVE THE CODE  ");
    cont_btn->setFixedHeight(32);
    cont_btn->setStyleSheet(btn_primary());
    connect(cont_btn, &QPushButton::clicked, this, [this]() { pages_->setCurrentIndex(2); });
    vl->addWidget(cont_btn);

    auto* resend = new QPushButton("DIDN'T RECEIVE? RESEND");
    resend->setStyleSheet(link_style());
    connect(resend, &QPushButton::clicked, this, &ForgotPasswordScreen::on_resend);
    vl->addWidget(resend, 0, Qt::AlignCenter);

    vl->addStretch();
    pages_->addWidget(page);
}

// ── Reset Page ───────────────────────────────────────────────────────────────

void ForgotPasswordScreen::build_reset_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);

    auto* header = new QWidget(this);
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* title = new QLabel("RESET PASSWORD");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                 "background: transparent; letter-spacing: 1px;"
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::AMBER));
    hl->addWidget(title);
    hl->addStretch();
    vl->addWidget(header);

    vl->addWidget(make_separator());

    auto add_field = [&](const char* label, const char* placeholder, bool is_password = false) -> QLineEdit* {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(label_style());
        vl->addWidget(lbl);
        auto* input = new QLineEdit;
        input->setPlaceholderText(placeholder);
        input->setFixedHeight(34);
        input->setStyleSheet(input_style());
        if (is_password)
            input->setEchoMode(QLineEdit::Password);
        vl->addWidget(input);
        return input;
    };

    otp_input_ = add_field("VERIFICATION CODE", "enter code from email");
    new_password_ = add_field("NEW PASSWORD", "min 8 characters", true);
    confirm_password_ = add_field("CONFIRM PASSWORD", "re-enter password", true);

    auto* err = new QLabel;
    err->setWordWrap(true);
    err->setStyleSheet(QString("color: %1; font-size: 13px;"
                               "background: rgba(220,38,38,0.08);"
                               "border: 1px solid #7f1d1d; padding: 6px 8px;"
                               "font-family: 'Consolas','Courier New',monospace;")
                           .arg(ui::colors::NEGATIVE));
    err->hide();
    vl->addWidget(err);
    connect(&auth::AuthManager::instance(), &auth::AuthManager::password_reset_failed, this, [err](const QString& e) {
        err->setText(e);
        err->show();
    });

    auto* reset_btn = new QPushButton("  RESET PASSWORD  ");
    reset_btn->setFixedHeight(32);
    reset_btn->setStyleSheet(btn_primary());
    connect(reset_btn, &QPushButton::clicked, this, &ForgotPasswordScreen::on_reset_password);
    vl->addWidget(reset_btn);

    vl->addStretch();
    pages_->addWidget(page);
}

// ── Success Page ─────────────────────────────────────────────────────────────

void ForgotPasswordScreen::build_success_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 22, 28, 22);
    vl->setSpacing(10);
    vl->setAlignment(Qt::AlignCenter);

    auto* header = new QWidget(this);
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* title = new QLabel("PASSWORD RESET");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                 "background: transparent; letter-spacing: 1px;"
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::POSITIVE));
    hl->addWidget(title);
    hl->addStretch();

    auto* status = new QLabel("SUCCESS");
    status->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                  "background: transparent; letter-spacing: 0.5px;"
                                  "font-family: 'Consolas','Courier New',monospace;")
                              .arg(ui::colors::POSITIVE));
    hl->addWidget(status);
    vl->addWidget(header);

    auto* sub = new QLabel("Your password has been reset. You can now sign in with your new password.");
    sub->setWordWrap(true);
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet(muted_style());
    vl->addWidget(sub);

    vl->addWidget(make_separator());

    auto* login_btn = new QPushButton("  CONTINUE TO LOGIN  ");
    login_btn->setFixedHeight(32);
    login_btn->setStyleSheet(btn_primary());
    connect(login_btn, &QPushButton::clicked, this, &ForgotPasswordScreen::navigate_login);
    vl->addWidget(login_btn);

    pages_->addWidget(page);
}

// ── Actions ──────────────────────────────────────────────────────────────────

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
    if (otp.isEmpty() || pw.length() < 8 || pw != cpw)
        return;
    auth::AuthManager::instance().reset_password(email_input_->text().trimmed(), otp, pw);
}

void ForgotPasswordScreen::on_resend() {
    auth::AuthManager::instance().forgot_password(email_input_->text().trimmed());
}

} // namespace fincept::screens
