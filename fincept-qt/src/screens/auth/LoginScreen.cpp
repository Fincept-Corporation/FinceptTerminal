#include "screens/auth/LoginScreen.h"

#include "auth/AuthManager.h"
#include "auth/AuthTypes.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/LanguageSwitcher.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
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

// White "Continue with Google" button — high contrast against the dark theme,
// matching Google's own button styling (white surface, dark text, hairline border).
static QString btn_google() {
    return QString("QPushButton {"
                   "  background: #ffffff; color: #3c4043;"
                   "  border: 1px solid #dadce0; border-radius: 2px;"
                   "  padding: 0 16px; font-size: 14px; font-weight: 700;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "}"
                   "QPushButton:hover { background: #f5f6f7; border-color: #c6c8ca; }"
                   "QPushButton:disabled { background: %1; color: %2; border-color: %3; }")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_DIM(), ui::colors::BORDER_DIM());
}

// Render the multi-colour Google "G" with QPainter (no QtSvg / asset dependency).
// Super-sampled 2× for crisp edges on both retina and non-retina displays.
static QPixmap google_g_pixmap(int logical_px) {
    constexpr qreal dpr = 2.0;
    const qreal s = logical_px * dpr;
    QPixmap pm(static_cast<int>(s), static_cast<int>(s));
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor blue(0x42, 0x85, 0xF4), red(0xEA, 0x43, 0x35), yellow(0xFB, 0xBC, 0x05), green(0x34, 0xA8, 0x53);
    const qreal w = s * 0.22; // ring thickness
    const qreal margin = w / 2.0 + s * 0.04;
    const QRectF ring(margin, margin, s - 2 * margin, s - 2 * margin);

    QPen pen;
    pen.setWidthF(w);
    pen.setCapStyle(Qt::FlatCap);
    auto arc = [&](const QColor& c, int start_deg, int span_deg) {
        pen.setColor(c);
        p.setPen(pen);
        p.drawArc(ring, start_deg * 16, span_deg * 16); // Qt angles: 0°=east, CCW positive
    };
    arc(blue, -20, 110);  // right → top
    arc(red, 90, 60);     // top → upper-left
    arc(yellow, 150, 70); // upper-left → lower-left
    arc(green, 220, 90);  // lower-left → bottom (gap 310°–340° = the G's mouth)

    // Blue crossbar (the "tongue") — from centre to the right inner edge at mid-height.
    p.setPen(Qt::NoPen);
    p.setBrush(blue);
    const qreal cy = s / 2.0;
    p.drawRect(QRectF(s / 2.0, cy - w / 2.0, ring.right() - s / 2.0, w));

    p.end();
    pm.setDevicePixelRatio(dpr);
    return pm;
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

    // Top-right language picker — available before login so users can change
    // the UI language without authenticating. Posts QEvent::LanguageChange
    // app-wide, which retranslates this screen via changeEvent below.
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

    build_login_page();
    build_mfa_page();
    build_conflict_page();

    overlay->addWidget(pages_, 0, Qt::AlignCenter);

    retranslateUi();

    auto& auth = auth::AuthManager::instance();
    connect(&auth, &auth::AuthManager::login_succeeded, this, &LoginScreen::on_login_succeeded);
    connect(&auth, &auth::AuthManager::login_failed, this, &LoginScreen::on_login_failed);
    connect(&auth, &auth::AuthManager::login_mfa_required, this, &LoginScreen::on_mfa_required);
    connect(&auth, &auth::AuthManager::login_active_session, this, &LoginScreen::on_active_session);
    connect(&auth, &auth::AuthManager::mfa_verified, this, &LoginScreen::on_mfa_verified);
    connect(&auth, &auth::AuthManager::mfa_failed, this, &LoginScreen::on_mfa_failed);
    connect(&auth, &auth::AuthManager::logged_out, this, [this]() {
        if (email_input_) email_input_->clear();
        if (password_input_) {
            password_input_->clear();
            password_input_->setEchoMode(QLineEdit::Password);
        }
        if (mfa_input_) mfa_input_->clear();
        clear_error();
        if (mfa_error_) mfa_error_->hide();
        if (pages_) pages_->setCurrentIndex(0);
    });
}

// ── Background Paint ─────────────────────────────────────────────────────────

void LoginScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
}

void LoginScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
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

    login_title_ = new QLabel;
    login_title_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                        "background: transparent; letter-spacing: 1px;"
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::AMBER()));
    hl->addWidget(login_title_);
    hl->addStretch();

    // Brand mark — not translated.
    auto* brand = new QLabel(QStringLiteral("FINCEPT"));
    brand->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                 "background: transparent; letter-spacing: 0.5px;"
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::TEXT_DIM()));
    hl->addWidget(brand);
    vl->addWidget(header);

    login_subtitle_ = new QLabel;
    login_subtitle_->setStyleSheet(muted_style());
    vl->addWidget(login_subtitle_);

    vl->addWidget(make_separator());

    // Email
    email_lbl_ = new QLabel;
    email_lbl_->setStyleSheet(label_style());
    vl->addWidget(email_lbl_);

    email_input_ = new QLineEdit;
    email_input_->setFixedHeight(34);
    email_input_->setStyleSheet(input_style());
    vl->addWidget(email_input_);

    // Password
    pw_lbl_ = new QLabel;
    pw_lbl_->setStyleSheet(label_style());
    vl->addWidget(pw_lbl_);

    auto* pw_row = new QWidget(this);
    pw_row->setStyleSheet("background: transparent;");
    auto* prl = new QHBoxLayout(pw_row);
    prl->setContentsMargins(0, 0, 0, 0);
    prl->setSpacing(4);

    password_input_ = new QLineEdit;
    password_input_->setEchoMode(QLineEdit::Password);
    password_input_->setFixedHeight(34);
    password_input_->setStyleSheet(input_style());
    prl->addWidget(password_input_);

    show_pw_btn_ = new QPushButton;
    show_pw_btn_->setFixedSize(58, 34);
    show_pw_btn_->setStyleSheet(btn_standard());
    connect(show_pw_btn_, &QPushButton::clicked, this, [this]() {
        bool hidden = password_input_->echoMode() == QLineEdit::Password;
        password_input_->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
        show_pw_btn_->setText(hidden ? tr("HIDE") : tr("SHOW"));
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

    forgot_btn_ = new QPushButton;
    forgot_btn_->setStyleSheet(link_style());
    connect(forgot_btn_, &QPushButton::clicked, this, &LoginScreen::navigate_forgot_password);
    brl->addWidget(forgot_btn_);
    brl->addStretch();

    login_btn_ = new QPushButton;
    login_btn_->setFixedHeight(32);
    login_btn_->setStyleSheet(btn_primary());
    connect(login_btn_, &QPushButton::clicked, this, &LoginScreen::on_login);
    brl->addWidget(login_btn_);
    vl->addWidget(btn_row);

    // ── "or" divider + Google sign-in ───────────────────────────────────────
    or_lbl_ = new QLabel;
    or_lbl_->setAlignment(Qt::AlignCenter);
    or_lbl_->setStyleSheet(muted_style());
    vl->addWidget(or_lbl_);

    google_btn_ = new QPushButton;
    google_btn_->setFixedHeight(38);
    google_btn_->setStyleSheet(btn_google());
    google_btn_->setIcon(QIcon(google_g_pixmap(18)));
    google_btn_->setIconSize(QSize(18, 18));
    google_btn_->setCursor(Qt::PointingHandCursor);
    connect(google_btn_, &QPushButton::clicked, this, [this]() {
        clear_error();
        set_loading(true);
        auth::AuthManager::instance().login_with_google();
    });
    vl->addWidget(google_btn_);

    vl->addWidget(make_separator());

    // Register link
    auto* reg_row = new QWidget(this);
    reg_row->setStyleSheet("background: transparent;");
    auto* rrl = new QHBoxLayout(reg_row);
    rrl->setContentsMargins(0, 0, 0, 0);
    rrl->setAlignment(Qt::AlignCenter);

    no_account_lbl_ = new QLabel;
    no_account_lbl_->setStyleSheet(muted_style());
    rrl->addWidget(no_account_lbl_);

    signup_btn_ = new QPushButton;
    signup_btn_->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none;"
                                       "  font-size: 13px; font-weight: 700;"
                                       "  font-family: 'Consolas','Courier New',monospace; }"
                                       "QPushButton:hover { color: %2; }")
                                   .arg(ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));
    connect(signup_btn_, &QPushButton::clicked, this, &LoginScreen::navigate_register);
    rrl->addWidget(signup_btn_);
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

    mfa_title_ = new QLabel;
    mfa_title_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                      "background: transparent; letter-spacing: 1px;"
                                      "font-family: 'Consolas','Courier New',monospace;")
                                  .arg(ui::colors::AMBER()));
    hl->addWidget(mfa_title_);
    hl->addStretch();

    mfa_indicator_ = new QLabel;
    mfa_indicator_->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                          "background: transparent; letter-spacing: 0.5px;"
                                          "font-family: 'Consolas','Courier New',monospace;")
                                      .arg(ui::colors::POSITIVE()));
    hl->addWidget(mfa_indicator_);
    vl->addWidget(header);

    mfa_sub_ = new QLabel;
    mfa_sub_->setWordWrap(true);
    mfa_sub_->setStyleSheet(muted_style());
    vl->addWidget(mfa_sub_);

    vl->addWidget(make_separator());

    mfa_code_lbl_ = new QLabel;
    mfa_code_lbl_->setStyleSheet(label_style());
    vl->addWidget(mfa_code_lbl_);

    mfa_input_ = new QLineEdit;
    mfa_input_->setPlaceholderText(QStringLiteral("000000"));
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

    mfa_verify_btn_ = new QPushButton;
    mfa_verify_btn_->setFixedHeight(32);
    mfa_verify_btn_->setStyleSheet(btn_primary());
    connect(mfa_verify_btn_, &QPushButton::clicked, this, &LoginScreen::on_mfa_verify);
    vl->addWidget(mfa_verify_btn_);

    mfa_back_btn_ = new QPushButton;
    mfa_back_btn_->setStyleSheet(link_style());
    connect(mfa_back_btn_, &QPushButton::clicked, this, [this]() { pages_->setCurrentIndex(0); });
    vl->addWidget(mfa_back_btn_, 0, Qt::AlignCenter);

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

    conflict_title_ = new QLabel;
    conflict_title_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                           "background: transparent; letter-spacing: 1px;"
                                           "font-family: 'Consolas','Courier New',monospace;")
                                       .arg(ui::colors::WARNING()));
    hl->addWidget(conflict_title_);
    hl->addStretch();

    conflict_warning_lbl_ = new QLabel;
    conflict_warning_lbl_->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700;"
                                                 "background: transparent; letter-spacing: 0.5px;"
                                                 "font-family: 'Consolas','Courier New',monospace;")
                                             .arg(ui::colors::WARNING()));
    hl->addWidget(conflict_warning_lbl_);
    vl->addWidget(header);

    conflict_msg_ = new QLabel;
    conflict_msg_->setWordWrap(true);
    conflict_msg_->setStyleSheet(QString("color: %1; font-size: 14px; background: transparent;"
                                         "font-family: 'Consolas','Courier New',monospace;")
                                     .arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(conflict_msg_);

    vl->addWidget(make_separator());

    force_login_btn_ = new QPushButton;
    force_login_btn_->setFixedHeight(32);
    force_login_btn_->setStyleSheet(btn_danger());
    connect(force_login_btn_, &QPushButton::clicked, this, &LoginScreen::on_force_login);
    vl->addWidget(force_login_btn_);

    cancel_btn_ = new QPushButton;
    cancel_btn_->setFixedHeight(32);
    cancel_btn_->setStyleSheet(btn_standard());
    connect(cancel_btn_, &QPushButton::clicked, this, [this]() { pages_->setCurrentIndex(0); });
    vl->addWidget(cancel_btn_);

    vl->addStretch();
    pages_->addWidget(conflict_page_);
}

// ── Re-translation ───────────────────────────────────────────────────────────

void LoginScreen::retranslateUi() {
    if (login_title_)    login_title_->setText(tr("SIGN IN"));
    if (login_subtitle_) login_subtitle_->setText(tr("Access your terminal account"));
    if (email_lbl_)      email_lbl_->setText(tr("EMAIL"));
    if (pw_lbl_)         pw_lbl_->setText(tr("PASSWORD"));
    if (email_input_)    email_input_->setPlaceholderText(tr("user@domain.com"));
    if (password_input_) password_input_->setPlaceholderText(tr("enter password"));
    if (show_pw_btn_) {
        // Preserve current visibility state when re-translating.
        const bool showing = password_input_ && password_input_->echoMode() != QLineEdit::Password;
        show_pw_btn_->setText(showing ? tr("HIDE") : tr("SHOW"));
    }
    if (forgot_btn_)      forgot_btn_->setText(tr("FORGOT PASSWORD?"));
    if (login_btn_)       login_btn_->setText(tr("  SIGN IN  "));
    if (or_lbl_)          or_lbl_->setText(tr("or"));
    if (google_btn_)      google_btn_->setText(tr("CONTINUE WITH GOOGLE"));
    if (no_account_lbl_)  no_account_lbl_->setText(tr("No account?"));
    if (signup_btn_)      signup_btn_->setText(tr("SIGN UP"));

    if (mfa_title_)     mfa_title_->setText(tr("TWO-FACTOR AUTH"));
    if (mfa_indicator_) mfa_indicator_->setText(tr("SECURE"));
    if (mfa_sub_)       mfa_sub_->setText(tr("Enter the 6-digit code from your authenticator"));
    if (mfa_code_lbl_)  mfa_code_lbl_->setText(tr("VERIFICATION CODE"));
    if (mfa_verify_btn_) mfa_verify_btn_->setText(tr("  VERIFY  "));
    if (mfa_back_btn_)   mfa_back_btn_->setText(tr("BACK TO LOGIN"));

    if (conflict_title_)       conflict_title_->setText(tr("SESSION CONFLICT"));
    if (conflict_warning_lbl_) conflict_warning_lbl_->setText(tr("WARNING"));
    if (force_login_btn_)      force_login_btn_->setText(tr("  LOG OUT OTHER SESSION & CONTINUE  "));
    if (cancel_btn_)           cancel_btn_->setText(tr("  CANCEL  "));
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
        show_error(tr("Please enter your password"));
        return;
    }

    clear_error();
    set_loading(true);
    auth::AuthManager::instance().login(email, password);
}

void LoginScreen::on_mfa_verify() {
    QString code = mfa_input_->text().trimmed();
    if (code.isEmpty()) {
        mfa_error_->setText(tr("Please enter the code"));
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
    if (email_input_) email_input_->clear();
    if (password_input_) {
        password_input_->clear();
        password_input_->setEchoMode(QLineEdit::Password);
    }
    if (mfa_input_) mfa_input_->clear();
    clear_error();
    if (mfa_error_) mfa_error_->hide();
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
    login_btn_->setText(loading ? tr("  SIGNING IN...  ") : tr("  SIGN IN  "));
    if (google_btn_) {
        google_btn_->setEnabled(!loading);
        google_btn_->setText(loading ? tr("WAITING FOR BROWSER…") : tr("CONTINUE WITH GOOGLE"));
    }
}

} // namespace fincept::screens
