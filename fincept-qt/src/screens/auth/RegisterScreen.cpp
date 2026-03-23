#include "screens/auth/RegisterScreen.h"

#include "auth/AuthManager.h"
#include "auth/AuthTypes.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::screens {

RegisterScreen::RegisterScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(ui::colors::DARK));

    auto* root = new QVBoxLayout(this);
    root->setAlignment(Qt::AlignCenter);

    pages_ = new QStackedWidget;
    pages_->setFixedWidth(440);

    build_form_page();
    build_otp_page();

    root->addWidget(pages_);

    auto& auth = auth::AuthManager::instance();
    connect(&auth, &auth::AuthManager::signup_succeeded, this, [this]() {
        register_btn_->setEnabled(true);
        register_btn_->setText("Create Account");
        otp_email_->setText(email_->text().trimmed());
        otp_input_->clear();
        otp_error_->hide();
        pages_->setCurrentIndex(1);
    });
    connect(&auth, &auth::AuthManager::signup_failed, this, [this](const QString& err) {
        register_btn_->setEnabled(true);
        register_btn_->setText("Create Account");
        error_label_->setText(err);
        error_label_->show();
    });
    connect(&auth, &auth::AuthManager::otp_verified, this, [this]() { verify_btn_->setEnabled(true); });
    connect(&auth, &auth::AuthManager::otp_failed, this, [this](const QString& err) {
        verify_btn_->setEnabled(true);
        verify_btn_->setText("Verify");
        otp_error_->setText(err);
        otp_error_->show();
    });
}

void RegisterScreen::build_form_page() {
    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px;")
                            .arg(ui::colors::PANEL, ui::colors::BORDER));

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 20, 24, 20);
    vl->setSpacing(8);

    // Back + title
    auto* header = new QWidget;
    header->setStyleSheet("background: transparent;");
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(0, 0, 0, 0);

    auto* back = new QPushButton("<");
    back->setFixedSize(28, 28);
    back->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; font-size: 16px; }")
                            .arg(ui::colors::MUTED));
    connect(back, &QPushButton::clicked, this, &RegisterScreen::navigate_login);
    hl->addWidget(back);

    auto* title = new QLabel("Create Account");
    title->setStyleSheet(
        QString("color: %1; font-size: 20px; font-weight: 300; background: transparent;").arg(ui::colors::WHITE));
    hl->addWidget(title);
    hl->addStretch();
    vl->addWidget(header);

    // Name row
    auto* name_row = new QWidget;
    name_row->setStyleSheet("background: transparent;");
    auto* nrl = new QHBoxLayout(name_row);
    nrl->setContentsMargins(0, 0, 0, 0);
    nrl->setSpacing(8);

    auto add_field = [&](QLineEdit*& field, const QString& label, const QString& placeholder, QVBoxLayout* parent) {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::WHITE));
        parent->addWidget(lbl);
        field = new QLineEdit;
        field->setPlaceholderText(placeholder);
        field->setFixedHeight(30);
        parent->addWidget(field);
    };

    auto* fn_col = new QVBoxLayout;
    fn_col->setSpacing(2);
    add_field(first_name_, "First Name", "First", fn_col);
    nrl->addLayout(fn_col);

    auto* ln_col = new QVBoxLayout;
    ln_col->setSpacing(2);
    add_field(last_name_, "Last Name", "Last", ln_col);
    nrl->addLayout(ln_col);
    vl->addWidget(name_row);

    add_field(email_, "Email", "you@example.com", vl);
    add_field(phone_, "Phone (with country code)", "+1 234 567 8900", vl);
    add_field(password_, "Password", "Min 8 characters", vl);
    password_->setEchoMode(QLineEdit::Password);

    // Password strength
    auto make_check = [&](QLabel*& lbl, const QString& text) {
        lbl = new QLabel(text);
        lbl->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;").arg(ui::colors::MUTED));
        vl->addWidget(lbl);
    };
    make_check(pw_len_, "  8+ characters");
    make_check(pw_up_, "  Uppercase letter");
    make_check(pw_low_, "  Lowercase letter");
    make_check(pw_num_, "  Number");
    make_check(pw_spec_, "  Special character");

    connect(password_, &QLineEdit::textChanged, this, &RegisterScreen::update_password_strength);

    add_field(confirm_pw_, "Confirm Password", "Re-enter password", vl);
    confirm_pw_->setEchoMode(QLineEdit::Password);

    error_label_ = new QLabel;
    error_label_->setWordWrap(true);
    error_label_->setStyleSheet("color: #FF4444; font-size: 12px; background: rgba(255,0,0,0.1); "
                                "border: 1px solid rgba(255,0,0,0.3); border-radius: 2px; padding: 6px;");
    error_label_->hide();
    vl->addWidget(error_label_);

    register_btn_ = new QPushButton("Create Account");
    register_btn_->setFixedHeight(32);
    register_btn_->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3; font-size: 13px; }"
                "QPushButton:hover { background: #1A1A1A; }")
            .arg(ui::colors::PANEL, ui::colors::WHITE, ui::colors::BORDER));
    connect(register_btn_, &QPushButton::clicked, this, &RegisterScreen::on_register);
    vl->addWidget(register_btn_);

    // Login link
    auto* login_row = new QWidget;
    login_row->setStyleSheet("background: transparent;");
    auto* lrl = new QHBoxLayout(login_row);
    lrl->setAlignment(Qt::AlignCenter);
    lrl->setContentsMargins(0, 0, 0, 0);
    auto* have = new QLabel("Already have an account?");
    have->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::MUTED));
    lrl->addWidget(have);
    auto* signin = new QPushButton("Sign In");
    signin->setStyleSheet("QPushButton { color: #4488FF; background: transparent; border: none; font-size: 12px; }");
    connect(signin, &QPushButton::clicked, this, &RegisterScreen::navigate_login);
    lrl->addWidget(signin);
    vl->addWidget(login_row);

    pages_->addWidget(page);
}

void RegisterScreen::build_otp_page() {
    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 4px;")
                            .arg(ui::colors::PANEL, ui::colors::BORDER));
    page->setFixedHeight(320);

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(12);

    auto* title = new QLabel("Verify Email");
    title->setStyleSheet(
        QString("color: %1; font-size: 20px; font-weight: 300; background: transparent;").arg(ui::colors::WHITE));
    vl->addWidget(title);

    otp_email_ = new QLabel;
    otp_email_->setWordWrap(true);
    otp_email_->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::MUTED));
    vl->addWidget(otp_email_);

    auto* lbl = new QLabel("Verification Code");
    lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(ui::colors::WHITE));
    vl->addWidget(lbl);

    otp_input_ = new QLineEdit;
    otp_input_->setPlaceholderText("Enter code from email");
    otp_input_->setFixedHeight(32);
    vl->addWidget(otp_input_);

    otp_error_ = new QLabel;
    otp_error_->setWordWrap(true);
    otp_error_->setStyleSheet("color: #FF4444; font-size: 12px; background: rgba(255,0,0,0.1); padding: 6px;");
    otp_error_->hide();
    vl->addWidget(otp_error_);

    verify_btn_ = new QPushButton("Verify");
    verify_btn_->setFixedHeight(32);
    connect(verify_btn_, &QPushButton::clicked, this, &RegisterScreen::on_verify_otp);
    vl->addWidget(verify_btn_);

    auto* resend = new QPushButton("Didn't receive the code? Resend");
    resend->setStyleSheet("QPushButton { color: #4488FF; background: transparent; border: none; font-size: 12px; }");
    connect(resend, &QPushButton::clicked, this, &RegisterScreen::on_resend_otp);
    vl->addWidget(resend, 0, Qt::AlignCenter);

    auto* back2 = new QPushButton("Back to form");
    back2->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; font-size: 12px; }")
                             .arg(ui::colors::MUTED));
    connect(back2, &QPushButton::clicked, this, [this]() { pages_->setCurrentIndex(0); });
    vl->addWidget(back2, 0, Qt::AlignCenter);

    vl->addStretch();

    connect(otp_input_, &QLineEdit::returnPressed, this, &RegisterScreen::on_verify_otp);

    pages_->addWidget(page);
}

void RegisterScreen::update_password_strength() {
    auto s = auth::validate_password(password_->text());
    auto style = [](bool ok) {
        return ok ? "color: #00B050; font-size: 11px; background: transparent;"
                  : "color: #505050; font-size: 11px; background: transparent;";
    };
    pw_len_->setStyleSheet(style(s.min_length));
    pw_up_->setStyleSheet(style(s.has_upper));
    pw_low_->setStyleSheet(style(s.has_lower));
    pw_num_->setStyleSheet(style(s.has_number));
    pw_spec_->setStyleSheet(style(s.has_special));
}

void RegisterScreen::on_register() {
    error_label_->hide();

    QString fn = first_name_->text().trimmed();
    QString ln = last_name_->text().trimmed();
    QString em = email_->text().trimmed();
    QString ph = phone_->text().trimmed();
    QString pw = password_->text();
    QString cpw = confirm_pw_->text();

    if (fn.isEmpty() || ln.isEmpty() || em.isEmpty() || pw.isEmpty() || cpw.isEmpty()) {
        error_label_->setText("All fields are required");
        error_label_->show();
        return;
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
        error_label_->setText("Username must be 3–50 characters");
        error_label_->show();
        return;
    }

    register_btn_->setEnabled(false);
    register_btn_->setText("Creating...");

    auth::AuthManager::instance().signup(username, em, pw, ph, {}, {});
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
    verify_btn_->setText("Verifying...");
    auth::AuthManager::instance().verify_otp(email_->text().trimmed(), code);
}

void RegisterScreen::on_resend_otp() {
    // Re-trigger signup to resend OTP
    QString fn = first_name_->text().trimmed();
    QString ln = last_name_->text().trimmed();
    QString username = auth::sanitize_input(fn + ln).toLower();
    auth::AuthManager::instance().signup(username, email_->text().trimmed(), password_->text(),
                                         phone_->text().trimmed(), {}, {});
}

} // namespace fincept::screens
