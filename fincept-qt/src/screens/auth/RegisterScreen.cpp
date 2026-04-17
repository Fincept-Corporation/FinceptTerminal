#include "screens/auth/RegisterScreen.h"

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
    return QString("background: %1; border: 1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM());
}

static QString input_style() {
    return QString("QLineEdit {"
                   "  background: %1; color: %2;"
                   "  border: 1px solid %3;"
                   "  padding: 4px 8px; font-size: 14px;"
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
                   "  padding: 0 16px; font-size: 13px; font-weight: 700;"
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
                   "  font-size: 12px; font-weight: 400;"
                   "  font-family: 'Consolas','Courier New',monospace;"
                   "}"
                   "QPushButton:hover { color: %2; }")
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_PRIMARY());
}

static QString label_style() {
    return QString("color: %1; font-size: 12px; font-weight: 700;"
                   "background: transparent; letter-spacing: 0.5px;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_SECONDARY());
}

static QString muted_style() {
    return QString("color: %1; font-size: 12px; background: transparent;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_TERTIARY());
}

static QString check_off() {
    return QString("color: %1; font-size: 11px; background: transparent;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::TEXT_DIM());
}

static QString check_on() {
    return QString("color: %1; font-size: 11px; background: transparent;"
                   "font-family: 'Consolas','Courier New',monospace;")
        .arg(ui::colors::POSITIVE());
}

static QFrame* make_separator() {
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_DIM()));
    return sep;
}

// ── Constructor ──────────────────────────────────────────────────────────────

RegisterScreen::RegisterScreen(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* overlay = new QVBoxLayout;
    overlay->setAlignment(Qt::AlignCenter);
    root->addLayout(overlay, 1);

    pages_ = new QStackedWidget;
    pages_->setMinimumWidth(480);
    pages_->setMaximumWidth(620);
    pages_->setStyleSheet("background: transparent;");

    build_form_page();
    build_otp_page();

    overlay->addWidget(pages_, 0, Qt::AlignCenter);

    auto& auth = auth::AuthManager::instance();
    connect(&auth, &auth::AuthManager::signup_succeeded, this, [this]() {
        register_btn_->setEnabled(true);
        register_btn_->setText("  CREATE ACCOUNT  ");
        otp_email_->setText(email_->text().trimmed());
        otp_input_->clear();
        otp_error_->hide();
        pages_->setCurrentIndex(1);
    });
    connect(&auth, &auth::AuthManager::signup_failed, this, [this](const QString& err) {
        register_btn_->setEnabled(true);
        register_btn_->setText("  CREATE ACCOUNT  ");
        error_label_->setText(err);
        error_label_->show();
    });
    connect(&auth, &auth::AuthManager::otp_verified, this, [this]() { verify_btn_->setEnabled(true); });
    connect(&auth, &auth::AuthManager::otp_failed, this, [this](const QString& err) {
        verify_btn_->setEnabled(true);
        verify_btn_->setText("  VERIFY  ");
        otp_error_->setText(err);
        otp_error_->show();
    });
}

// ── Background ───────────────────────────────────────────────────────────────

void RegisterScreen::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.fillRect(rect(), QColor(ui::colors::BG_BASE()));

    p.setPen(QPen(QColor(ui::colors::BG_RAISED()), 1));
    const int step = 24;
    for (int x = 0; x < width(); x += step)
        p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += step)
        p.drawLine(0, y, width(), y);
}

// ── Form Page ────────────────────────────────────────────────────────────────

void RegisterScreen::build_form_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(card_style());

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 18, 28, 18);
    vl->setSpacing(6);

    // Header bar
    auto* header = new QWidget(this);
    header->setFixedHeight(38);
    header->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BG_RAISED()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* back = new QPushButton("<");
    back->setFixedSize(24, 24);
    back->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none;"
                                "  font-size: 16px; font-family: 'Consolas','Courier New',monospace; }"
                                "QPushButton:hover { color: %2; }")
                            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_PRIMARY()));
    connect(back, &QPushButton::clicked, this, &RegisterScreen::navigate_login);
    hl->addWidget(back);

    auto* title = new QLabel("CREATE ACCOUNT");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                 "background: transparent; letter-spacing: 1px;"
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::AMBER()));
    hl->addWidget(title);
    hl->addStretch();
    vl->addWidget(header);

    vl->addWidget(make_separator());

    // Helper to add a compact field
    auto add_field = [&](QLineEdit*& field, const char* label, const char* placeholder, QLayout* parent) {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(label_style());
        if (auto* vbl = qobject_cast<QVBoxLayout*>(static_cast<QObject*>(nullptr)); !vbl) {
            // parent is a layout — just add
        }
        static_cast<QBoxLayout*>(parent)->addWidget(lbl);
        field = new QLineEdit;
        field->setPlaceholderText(placeholder);
        field->setFixedHeight(30);
        field->setStyleSheet(input_style());
        static_cast<QBoxLayout*>(parent)->addWidget(field);
    };

    // Name row — side by side
    auto* name_row = new QWidget(this);
    name_row->setStyleSheet("background: transparent;");
    auto* nrl = new QHBoxLayout(name_row);
    nrl->setContentsMargins(0, 0, 0, 0);
    nrl->setSpacing(8);

    auto* fn_col = new QVBoxLayout;
    fn_col->setSpacing(2);
    add_field(first_name_, "FIRST NAME", "First", fn_col);
    nrl->addLayout(fn_col);

    auto* ln_col = new QVBoxLayout;
    ln_col->setSpacing(2);
    add_field(last_name_, "LAST NAME", "Last", ln_col);
    nrl->addLayout(ln_col);
    vl->addWidget(name_row);

    add_field(email_, "EMAIL", "user@domain.com", vl);

    // Phone + country code side by side
    auto* ph_row = new QWidget(this);
    ph_row->setStyleSheet("background: transparent;");
    auto* phl = new QHBoxLayout(ph_row);
    phl->setContentsMargins(0, 0, 0, 0);
    phl->setSpacing(8);

    auto* cc_col = new QVBoxLayout;
    cc_col->setSpacing(2);
    add_field(country_code_, "CODE", "+1", cc_col);
    country_code_->setFixedWidth(72);
    phl->addLayout(cc_col);

    auto* ph_col = new QVBoxLayout;
    ph_col->setSpacing(2);
    add_field(phone_, "PHONE", "234 567 8900", ph_col);
    phl->addLayout(ph_col);

    vl->addWidget(ph_row);

    add_field(password_, "PASSWORD", "min 8 characters", vl);
    password_->setEchoMode(QLineEdit::Password);

    // Password strength — compact single row hints
    auto* pw_checks = new QWidget(this);
    pw_checks->setStyleSheet("background: transparent;");
    auto* pcl = new QHBoxLayout(pw_checks);
    pcl->setContentsMargins(0, 0, 0, 0);
    pcl->setSpacing(8);

    auto make_check = [&](QLabel*& lbl, const char* text) {
        lbl = new QLabel(text);
        lbl->setStyleSheet(check_off());
        pcl->addWidget(lbl);
    };
    make_check(pw_len_, "8+");
    make_check(pw_up_, "A-Z");
    make_check(pw_low_, "a-z");
    make_check(pw_num_, "0-9");
    make_check(pw_spec_, "!@#");
    pcl->addStretch();
    vl->addWidget(pw_checks);

    connect(password_, &QLineEdit::textChanged, this, &RegisterScreen::update_password_strength);

    add_field(confirm_pw_, "CONFIRM PASSWORD", "re-enter password", vl);
    confirm_pw_->setEchoMode(QLineEdit::Password);

    // Error
    error_label_ = new QLabel;
    error_label_->setWordWrap(true);
    error_label_->setStyleSheet(QString("color: %1; font-size: 12px;"
                                        "background: rgba(220,38,38,0.08);"
                                        "border: 1px solid #7f1d1d; padding: 4px 8px;"
                                        "font-family: 'Consolas','Courier New',monospace;")
                                    .arg(ui::colors::NEGATIVE()));
    error_label_->hide();
    vl->addWidget(error_label_);

    vl->addSpacing(2);

    register_btn_ = new QPushButton("  CREATE ACCOUNT  ");
    register_btn_->setFixedHeight(32);
    register_btn_->setStyleSheet(btn_primary());
    connect(register_btn_, &QPushButton::clicked, this, &RegisterScreen::on_register);
    vl->addWidget(register_btn_);

    vl->addWidget(make_separator());

    // Login link
    auto* login_row = new QWidget(this);
    login_row->setStyleSheet("background: transparent;");
    auto* lrl = new QHBoxLayout(login_row);
    lrl->setAlignment(Qt::AlignCenter);
    lrl->setContentsMargins(0, 0, 0, 0);

    auto* have = new QLabel("Already have an account?");
    have->setStyleSheet(muted_style());
    lrl->addWidget(have);

    auto* signin = new QPushButton("SIGN IN");
    signin->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none;"
                                  "  font-size: 12px; font-weight: 700;"
                                  "  font-family: 'Consolas','Courier New',monospace; }"
                                  "QPushButton:hover { color: %2; }")
                              .arg(ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));
    connect(signin, &QPushButton::clicked, this, &RegisterScreen::navigate_login);
    lrl->addWidget(signin);
    vl->addWidget(login_row);

    pages_->addWidget(page);
}

// ── OTP Page ─────────────────────────────────────────────────────────────────

void RegisterScreen::build_otp_page() {
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

    auto* title = new QLabel("VERIFY EMAIL");
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700;"
                                 "background: transparent; letter-spacing: 1px;"
                                 "font-family: 'Consolas','Courier New',monospace;")
                             .arg(ui::colors::AMBER()));
    hl->addWidget(title);
    hl->addStretch();
    vl->addWidget(header);

    otp_email_ = new QLabel;
    otp_email_->setWordWrap(true);
    otp_email_->setStyleSheet(muted_style());
    vl->addWidget(otp_email_);

    vl->addWidget(make_separator());

    auto* lbl = new QLabel("VERIFICATION CODE");
    lbl->setStyleSheet(label_style());
    vl->addWidget(lbl);

    otp_input_ = new QLineEdit;
    otp_input_->setPlaceholderText("enter code from email");
    otp_input_->setFixedHeight(34);
    otp_input_->setStyleSheet(QString("QLineEdit {"
                                      "  background: %1; color: %2;"
                                      "  border: 1px solid %3;"
                                      "  padding: 6px 10px; font-size: 15px;"
                                      "  font-family: 'Consolas','Courier New',monospace;"
                                      "  selection-background-color: %4; selection-color: %5;"
                                      "}"
                                      "QLineEdit:focus { border: 1px solid %6; }")
                                  .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                       ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::BORDER_BRIGHT()));
    vl->addWidget(otp_input_);

    otp_error_ = new QLabel;
    otp_error_->setWordWrap(true);
    otp_error_->setStyleSheet(QString("color: %1; font-size: 12px;"
                                      "background: rgba(220,38,38,0.08);"
                                      "border: 1px solid #7f1d1d; padding: 4px 8px;"
                                      "font-family: 'Consolas','Courier New',monospace;")
                                  .arg(ui::colors::NEGATIVE()));
    otp_error_->hide();
    vl->addWidget(otp_error_);

    verify_btn_ = new QPushButton("  VERIFY  ");
    verify_btn_->setFixedHeight(32);
    verify_btn_->setStyleSheet(btn_primary());
    connect(verify_btn_, &QPushButton::clicked, this, &RegisterScreen::on_verify_otp);
    vl->addWidget(verify_btn_);

    auto* resend = new QPushButton("DIDN'T RECEIVE? RESEND");
    resend->setStyleSheet(link_style());
    connect(resend, &QPushButton::clicked, this, &RegisterScreen::on_resend_otp);
    vl->addWidget(resend, 0, Qt::AlignCenter);

    auto* back2 = new QPushButton("BACK TO FORM");
    back2->setStyleSheet(link_style());
    connect(back2, &QPushButton::clicked, this, [this]() { pages_->setCurrentIndex(0); });
    vl->addWidget(back2, 0, Qt::AlignCenter);

    vl->addStretch();
    connect(otp_input_, &QLineEdit::returnPressed, this, &RegisterScreen::on_verify_otp);
    pages_->addWidget(page);
}

// ── Password Strength ────────────────────────────────────────────────────────

void RegisterScreen::update_password_strength() {
    auto s = auth::validate_password(password_->text());
    pw_len_->setStyleSheet(s.min_length ? check_on() : check_off());
    pw_up_->setStyleSheet(s.has_upper ? check_on() : check_off());
    pw_low_->setStyleSheet(s.has_lower ? check_on() : check_off());
    pw_num_->setStyleSheet(s.has_number ? check_on() : check_off());
    pw_spec_->setStyleSheet(s.has_special ? check_on() : check_off());
}

// ── Actions ──────────────────────────────────────────────────────────────────

void RegisterScreen::on_register() {
    error_label_->hide();

    QString fn = first_name_->text().trimmed();
    QString ln = last_name_->text().trimmed();
    QString em = email_->text().trimmed();
    QString ph = phone_->text().trimmed();
    QString cc = country_code_->text().trimmed();
    QString pw = password_->text();
    QString cpw = confirm_pw_->text();

    if (fn.isEmpty() || ln.isEmpty() || em.isEmpty() || ph.isEmpty() || pw.isEmpty() || cpw.isEmpty()) {
        error_label_->setText("All fields are required");
        error_label_->show();
        return;
    }
    if (cc.isEmpty()) {
        error_label_->setText("Country code is required (e.g. +1, +91)");
        error_label_->show();
        return;
    }
    // Ensure country code starts with +
    if (!cc.startsWith('+')) {
        cc = '+' + cc;
        country_code_->setText(cc);
    }
    auto ev = auth::validate_email(em);
    if (!ev.valid) {
        error_label_->setText(ev.error);
        error_label_->show();
        return;
    }
    if (pw != cpw) {
        error_label_->setText("Passwords do not match");
        error_label_->show();
        return;
    }
    if (pw.length() < 8) {
        error_label_->setText("Password must be at least 8 characters");
        error_label_->show();
        return;
    }

    QString username = auth::sanitize_input(fn + ln).toLower();
    if (username.length() < 3 || username.length() > 50) {
        error_label_->setText("Username must be 3-50 characters");
        error_label_->show();
        return;
    }

    register_btn_->setEnabled(false);
    register_btn_->setText("  CREATING...  ");
    auth::AuthManager::instance().signup(username, em, pw, ph, {}, cc);
}

void RegisterScreen::on_verify_otp() {
    otp_error_->hide();
    QString code = otp_input_->text().trimmed();
    if (code.isEmpty()) {
        otp_error_->setText("Enter the verification code");
        otp_error_->show();
        return;
    }
    verify_btn_->setEnabled(false);
    verify_btn_->setText("  VERIFYING...  ");
    auth::AuthManager::instance().verify_otp(email_->text().trimmed(), code);
}

void RegisterScreen::on_resend_otp() {
    QString fn = first_name_->text().trimmed();
    QString ln = last_name_->text().trimmed();
    QString username = auth::sanitize_input(fn + ln).toLower();
    QString cc = country_code_->text().trimmed();
    if (!cc.isEmpty() && !cc.startsWith('+'))
        cc = '+' + cc;
    auth::AuthManager::instance().signup(username, email_->text().trimmed(), password_->text(),
                                         phone_->text().trimmed(), {}, cc);
}

} // namespace fincept::screens
