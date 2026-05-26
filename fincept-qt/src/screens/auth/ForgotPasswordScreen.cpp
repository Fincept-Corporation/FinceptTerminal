#include "screens/auth/ForgotPasswordScreen.h"

#include "auth/AuthManager.h"
#include "auth/AuthTypes.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/LanguageSwitcher.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QPainter>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Obsidian Styles ──────────────────────────────────────────────────────────

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

ForgotPasswordScreen::ForgotPasswordScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // Top-right language picker — visible without account.
    auto* top_row = new QHBoxLayout;
    top_row->setContentsMargins(20, 20, 20, 0);
    top_row->addStretch();
    top_row->addWidget(new ui::LanguageSwitcher(this));
    root->addLayout(top_row);

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

    retranslateUi();

    auto& auth = auth::AuthManager::instance();
    connect(&auth, &auth::AuthManager::forgot_password_sent, this, [this]() { pages_->setCurrentIndex(1); });
    connect(&auth, &auth::AuthManager::forgot_password_failed, this, [this](const QString& err) {
        error_label_->setText(err);
        error_label_->show();
    });
    connect(&auth, &auth::AuthManager::password_reset_succeeded, this, [this]() {
        for (QLineEdit* w : {email_input_, otp_input_, new_password_, confirm_password_}) {
            if (w) w->clear();
        }
        if (new_password_) new_password_->setEchoMode(QLineEdit::Password);
        if (confirm_password_) confirm_password_->setEchoMode(QLineEdit::Password);
        if (error_label_) error_label_->hide();
        pages_->setCurrentIndex(3);
    });
    connect(&auth, &auth::AuthManager::password_reset_failed, this, [this](const QString& err) {
        error_label_->setText(err);
        error_label_->show();
    });
}

// ── Background ───────────────────────────────────────────────────────────────

void ForgotPasswordScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
}

void ForgotPasswordScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

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
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* back = new QPushButton(QStringLiteral("<"));
    back->setFixedSize(24, 24);
    back->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none;"
                                "  font-size: 16px; font-family: 'Consolas','Courier New',monospace; }"
                                "QPushButton:hover { color: %2; }")
                            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_PRIMARY()));
    connect(back, &QPushButton::clicked, this, &ForgotPasswordScreen::navigate_login);
    hl->addWidget(back);

    email_title_ = new QLabel;
    email_title_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                        "background: transparent; letter-spacing: 1px;"
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::AMBER()));
    hl->addWidget(email_title_);
    hl->addStretch();
    vl->addWidget(header);

    email_sub_ = new QLabel;
    email_sub_->setWordWrap(true);
    email_sub_->setStyleSheet(muted_style());
    vl->addWidget(email_sub_);

    vl->addWidget(make_separator());

    email_lbl_ = new QLabel;
    email_lbl_->setStyleSheet(label_style());
    vl->addWidget(email_lbl_);

    email_input_ = new QLineEdit;
    email_input_->setFixedHeight(34);
    email_input_->setStyleSheet(input_style());
    vl->addWidget(email_input_);

    error_label_ = new QLabel;
    error_label_->setWordWrap(true);
    error_label_->setStyleSheet(QString("color: %1; font-size: 13px;"
                                        "background: rgba(220,38,38,0.08);"
                                        "border: 1px solid #7f1d1d; padding: 6px 8px;"
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::NEGATIVE()));
    error_label_->hide();
    vl->addWidget(error_label_);

    send_btn_ = new QPushButton;
    send_btn_->setFixedHeight(32);
    send_btn_->setStyleSheet(btn_primary());
    connect(send_btn_, &QPushButton::clicked, this, &ForgotPasswordScreen::on_send_code);
    connect(email_input_, &QLineEdit::returnPressed, this, &ForgotPasswordScreen::on_send_code);
    vl->addWidget(send_btn_);

    vl->addWidget(make_separator());

    back_login_btn_ = new QPushButton;
    back_login_btn_->setStyleSheet(link_style());
    connect(back_login_btn_, &QPushButton::clicked, this, &ForgotPasswordScreen::navigate_login);
    vl->addWidget(back_login_btn_, 0, Qt::AlignCenter);

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
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    sent_title_ = new QLabel;
    sent_title_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                       "background: transparent; letter-spacing: 1px;"
                                       "font-family: 'Consolas','Courier New',monospace;")
                                   .arg(ui::colors::AMBER()));
    hl->addWidget(sent_title_);
    hl->addStretch();
    vl->addWidget(header);

    sent_sub_ = new QLabel;
    sent_sub_->setWordWrap(true);
    sent_sub_->setStyleSheet(muted_style());
    vl->addWidget(sent_sub_);

    vl->addWidget(make_separator());

    sent_continue_btn_ = new QPushButton;
    sent_continue_btn_->setFixedHeight(32);
    sent_continue_btn_->setStyleSheet(btn_primary());
    connect(sent_continue_btn_, &QPushButton::clicked, this, [this]() { pages_->setCurrentIndex(2); });
    vl->addWidget(sent_continue_btn_);

    sent_resend_btn_ = new QPushButton;
    sent_resend_btn_->setStyleSheet(link_style());
    connect(sent_resend_btn_, &QPushButton::clicked, this, &ForgotPasswordScreen::on_resend);
    vl->addWidget(sent_resend_btn_, 0, Qt::AlignCenter);

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
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    reset_title_ = new QLabel;
    reset_title_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                        "background: transparent; letter-spacing: 1px;"
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::AMBER()));
    hl->addWidget(reset_title_);
    hl->addStretch();
    vl->addWidget(header);

    vl->addWidget(make_separator());

    auto add_field = [&](QLabel*& label_out, bool is_password = false) -> QLineEdit* {
        label_out = new QLabel;
        label_out->setStyleSheet(label_style());
        vl->addWidget(label_out);
        auto* input = new QLineEdit;
        input->setFixedHeight(34);
        input->setStyleSheet(input_style());
        if (is_password)
            input->setEchoMode(QLineEdit::Password);
        vl->addWidget(input);
        return input;
    };

    otp_input_ = add_field(reset_code_lbl_);
    new_password_ = add_field(reset_new_lbl_, true);
    confirm_password_ = add_field(reset_confirm_lbl_, true);

    auto* err = new QLabel;
    err->setWordWrap(true);
    err->setStyleSheet(QString("color: %1; font-size: 13px;"
                               "background: rgba(220,38,38,0.08);"
                               "border: 1px solid #7f1d1d; padding: 6px 8px;"
                               "font-family: 'Consolas','Courier New',monospace;")
                           .arg(ui::colors::NEGATIVE()));
    err->hide();
    vl->addWidget(err);
    connect(&auth::AuthManager::instance(), &auth::AuthManager::password_reset_failed, this, [err](const QString& e) {
        err->setText(e);
        err->show();
    });

    reset_btn_ = new QPushButton;
    reset_btn_->setFixedHeight(32);
    reset_btn_->setStyleSheet(btn_primary());
    connect(reset_btn_, &QPushButton::clicked, this, &ForgotPasswordScreen::on_reset_password);
    vl->addWidget(reset_btn_);

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
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    success_title_ = new QLabel;
    success_title_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                          "background: transparent; letter-spacing: 1px;"
                                          "font-family: 'Consolas','Courier New',monospace;")
                                      .arg(ui::colors::POSITIVE()));
    hl->addWidget(success_title_);
    hl->addStretch();

    success_status_ = new QLabel;
    success_status_->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                           "background: transparent; letter-spacing: 0.5px;"
                                           "font-family: 'Consolas','Courier New',monospace;")
                                       .arg(ui::colors::POSITIVE()));
    hl->addWidget(success_status_);
    vl->addWidget(header);

    success_sub_ = new QLabel;
    success_sub_->setWordWrap(true);
    success_sub_->setAlignment(Qt::AlignCenter);
    success_sub_->setStyleSheet(muted_style());
    vl->addWidget(success_sub_);

    vl->addWidget(make_separator());

    success_continue_btn_ = new QPushButton;
    success_continue_btn_->setFixedHeight(32);
    success_continue_btn_->setStyleSheet(btn_primary());
    connect(success_continue_btn_, &QPushButton::clicked, this, [this]() {
        pages_->setCurrentIndex(0);
        emit navigate_login();
    });
    vl->addWidget(success_continue_btn_);

    pages_->addWidget(page);
}

// ── Re-translation ───────────────────────────────────────────────────────────

void ForgotPasswordScreen::retranslateUi() {
    if (email_title_)    email_title_->setText(tr("RESET PASSWORD"));
    if (email_sub_)      email_sub_->setText(tr("Enter your email and we'll send a verification code."));
    if (email_lbl_)      email_lbl_->setText(tr("EMAIL"));
    if (email_input_)    email_input_->setPlaceholderText(tr("user@domain.com"));
    if (send_btn_)       send_btn_->setText(tr("  SEND CODE  "));
    if (back_login_btn_) back_login_btn_->setText(tr("REMEMBER YOUR PASSWORD? SIGN IN"));

    if (sent_title_)        sent_title_->setText(tr("CHECK YOUR EMAIL"));
    if (sent_sub_)          sent_sub_->setText(tr("We've sent a verification code. Enter it on the next screen to reset your password."));
    if (sent_continue_btn_) sent_continue_btn_->setText(tr("  I HAVE THE CODE  "));
    if (sent_resend_btn_)   sent_resend_btn_->setText(tr("DIDN'T RECEIVE? RESEND"));

    if (reset_title_)       reset_title_->setText(tr("RESET PASSWORD"));
    if (reset_code_lbl_)    reset_code_lbl_->setText(tr("VERIFICATION CODE"));
    if (reset_new_lbl_)     reset_new_lbl_->setText(tr("NEW PASSWORD"));
    if (reset_confirm_lbl_) reset_confirm_lbl_->setText(tr("CONFIRM PASSWORD"));
    if (otp_input_)         otp_input_->setPlaceholderText(tr("enter code from email"));
    if (new_password_)      new_password_->setPlaceholderText(tr("min 8 characters"));
    if (confirm_password_)  confirm_password_->setPlaceholderText(tr("re-enter password"));
    if (reset_btn_)         reset_btn_->setText(tr("  RESET PASSWORD  "));

    if (success_title_)        success_title_->setText(tr("PASSWORD RESET"));
    if (success_status_)       success_status_->setText(tr("SUCCESS"));
    if (success_sub_)          success_sub_->setText(tr("Your password has been reset. You can now sign in with your new password."));
    if (success_continue_btn_) success_continue_btn_->setText(tr("  CONTINUE TO LOGIN  "));
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
