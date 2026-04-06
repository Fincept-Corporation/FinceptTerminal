#include "screens/crypto_trading/CryptoCredentials.h"

#include "python/PythonRunner.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

// Obsidian Design System input style
namespace {
static const QString kInputStyle =
    "QLineEdit { background: #0a0a0a; border: 1px solid #1a1a1a; color: #e5e5e5; padding: 6px; "
    "font-size: 13px; font-family: 'Consolas', 'Courier New', monospace; }"
    "QLineEdit:focus { border-color: #333333; }";
} // namespace

namespace fincept::screens::crypto {

CryptoCredentials::CryptoCredentials(const QString& exchange_id, QWidget* parent)
    : QDialog(parent), exchange_id_(exchange_id) {
    setWindowTitle(QString("API Credentials — %1").arg(exchange_id.toUpper()));
    setMinimumWidth(400);
    setStyleSheet("QDialog { background: #080808; color: #e5e5e5; "
                  "font-family: 'Consolas', 'Courier New', monospace; }");

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(14, 14, 14, 14);

    // Header
    auto* title = new QLabel(QString("API CREDENTIALS  %1").arg(exchange_id.toUpper()));
    title->setStyleSheet("color: #d97706; font-size: 14px; font-weight: 700; letter-spacing: 0.5px; "
                         "font-family: 'Consolas', 'Courier New', monospace;");
    layout->addWidget(title);

    auto* info = new QLabel("Enter your exchange API credentials for live trading.\n"
                            "Keys are stored locally in encrypted secure storage.");
    info->setStyleSheet("color: #808080; font-size: 12px; "
                        "font-family: 'Consolas', 'Courier New', monospace;");
    info->setWordWrap(true);
    layout->addWidget(info);

    layout->addSpacing(4);

    // API Key
    auto* key_lbl = new QLabel("API KEY");
    key_lbl->setStyleSheet("color: #808080; font-size: 12px; font-weight: 700; letter-spacing: 0.5px; "
                           "font-family: 'Consolas', 'Courier New', monospace;");
    layout->addWidget(key_lbl);
    key_edit_ = new QLineEdit;
    key_edit_->setPlaceholderText("Enter API key");
    key_edit_->setStyleSheet(kInputStyle);
    key_edit_->setFixedHeight(28);
    layout->addWidget(key_edit_);

    // API Secret
    auto* secret_lbl = new QLabel("API SECRET");
    secret_lbl->setStyleSheet("color: #808080; font-size: 12px; font-weight: 700; letter-spacing: 0.5px; "
                              "font-family: 'Consolas', 'Courier New', monospace;");
    layout->addWidget(secret_lbl);
    secret_edit_ = new QLineEdit;
    secret_edit_->setPlaceholderText("Enter API secret");
    secret_edit_->setEchoMode(QLineEdit::Password);
    secret_edit_->setStyleSheet(kInputStyle);
    secret_edit_->setFixedHeight(28);
    layout->addWidget(secret_edit_);

    // Password
    auto* pw_lbl = new QLabel("PASSWORD (OKX/KUCOIN)");
    pw_lbl->setStyleSheet("color: #808080; font-size: 12px; font-weight: 700; letter-spacing: 0.5px; "
                          "font-family: 'Consolas', 'Courier New', monospace;");
    layout->addWidget(pw_lbl);
    password_edit_ = new QLineEdit;
    password_edit_->setPlaceholderText("Optional");
    password_edit_->setEchoMode(QLineEdit::Password);
    password_edit_->setStyleSheet(kInputStyle);
    password_edit_->setFixedHeight(28);
    layout->addWidget(password_edit_);

    // TOTP section
    layout->addSpacing(4);
    auto* totp_lbl = new QLabel("TOTP SECRET (2FA)");
    totp_lbl->setStyleSheet("color: #808080; font-size: 12px; font-weight: 700; letter-spacing: 0.5px; "
                            "font-family: 'Consolas', 'Courier New', monospace;");
    layout->addWidget(totp_lbl);

    totp_secret_edit_ = new QLineEdit;
    totp_secret_edit_->setPlaceholderText("Base32 secret (optional)");
    totp_secret_edit_->setEchoMode(QLineEdit::Password);
    totp_secret_edit_->setStyleSheet(kInputStyle);
    totp_secret_edit_->setFixedHeight(28);
    layout->addWidget(totp_secret_edit_);

    auto* totp_row = new QHBoxLayout;
    totp_row->setSpacing(8);

    totp_code_label_ = new QLabel("CODE: --");
    totp_code_label_->setStyleSheet(
        "color: #d97706; font-size: 18px; font-weight: 700; letter-spacing: 2px; "
        "font-family: 'Consolas', 'Courier New', monospace;");
    totp_row->addWidget(totp_code_label_);

    totp_countdown_label_ = new QLabel("");
    totp_countdown_label_->setStyleSheet(
        "color: #525252; font-size: 11px; font-family: 'Consolas', 'Courier New', monospace;");
    totp_row->addWidget(totp_countdown_label_);
    totp_row->addStretch();
    layout->addLayout(totp_row);

    totp_timer_ = new QTimer(this);
    totp_timer_->setInterval(1000);
    connect(totp_timer_, &QTimer::timeout, this, &CryptoCredentials::on_totp_tick);
    connect(totp_secret_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.trimmed().isEmpty()) {
            totp_timer_->stop();
            totp_code_label_->setText("CODE: --");
            totp_countdown_label_->setText("");
        } else {
            refresh_totp();
            totp_timer_->start();
        }
    });

    // Status
    status_label_ = new QLabel("");
    status_label_->setStyleSheet("font-size: 12px; font-family: 'Consolas', 'Courier New', monospace;");
    layout->addWidget(status_label_);

    // Buttons — Obsidian danger + accent
    auto* btn_row = new QHBoxLayout;

    auto* clear_btn = new QPushButton("CLEAR");
    clear_btn->setStyleSheet(
        "QPushButton { background: rgba(220,38,38,0.1); color: #dc2626; border: 1px solid #7f1d1d; "
        "padding: 6px 16px; font-weight: 700; font-size: 12px; "
        "font-family: 'Consolas', 'Courier New', monospace; }"
        "QPushButton:hover { background: #dc2626; color: #e5e5e5; }");
    connect(clear_btn, &QPushButton::clicked, this, &CryptoCredentials::on_clear);
    btn_row->addWidget(clear_btn);

    btn_row->addStretch();

    auto* save_btn = new QPushButton("SAVE & CONNECT");
    save_btn->setStyleSheet("QPushButton { background: rgba(217,119,6,0.1); color: #d97706; border: 1px solid #78350f; "
                            "padding: 6px 16px; font-weight: 700; font-size: 12px; "
                            "font-family: 'Consolas', 'Courier New', monospace; }"
                            "QPushButton:hover { background: #d97706; color: #080808; }");
    connect(save_btn, &QPushButton::clicked, this, &CryptoCredentials::on_save);
    btn_row->addWidget(save_btn);

    layout->addLayout(btn_row);
}

QString CryptoCredentials::api_key() const {
    return key_edit_->text().trimmed();
}
QString CryptoCredentials::api_secret() const {
    return secret_edit_->text().trimmed();
}
QString CryptoCredentials::password() const {
    return password_edit_->text().trimmed();
}

void CryptoCredentials::set_values(const QString& key, const QString& secret, const QString& pw) {
    key_edit_->setText(key);
    secret_edit_->setText(secret);
    password_edit_->setText(pw);
}

void CryptoCredentials::on_save() {
    if (api_key().isEmpty() || api_secret().isEmpty()) {
        status_label_->setText("API key and secret are required");
        status_label_->setStyleSheet("color: #dc2626; font-size: 12px; "
                                     "font-family: 'Consolas', 'Courier New', monospace;");
        return;
    }
    emit credentials_saved(api_key(), api_secret(), password());
    status_label_->setText("Credentials saved");
    status_label_->setStyleSheet("color: #16a34a; font-size: 12px; "
                                 "font-family: 'Consolas', 'Courier New', monospace;");
    accept();
}

void CryptoCredentials::on_clear() {
    key_edit_->clear();
    secret_edit_->clear();
    password_edit_->clear();
    status_label_->setText("Credentials cleared");
    status_label_->setStyleSheet("color: #ca8a04; font-size: 12px; "
                                 "font-family: 'Consolas', 'Courier New', monospace;");
}

void CryptoCredentials::on_totp_tick() {
    refresh_totp();
}

void CryptoCredentials::refresh_totp() {
    const QString secret = totp_secret_edit_->text().trimmed();
    if (secret.isEmpty())
        return;

    const QString script = python::PythonRunner::instance().scripts_dir()
                           + "/exchange/totp_gen.py";

    QPointer<CryptoCredentials> self = this;
    python::PythonRunner::instance().run(script, {secret},
        [self](const python::PythonResult& result) {
            if (!self)
                return;
            if (!result.success) {
                self->totp_code_label_->setText("CODE: ERR");
                self->totp_countdown_label_->setText("");
                return;
            }
            const auto doc = QJsonDocument::fromJson(result.output.toUtf8());
            const auto obj = doc.object();
            if (obj.contains("code")) {
                self->totp_code_label_->setText(QString("CODE: %1").arg(obj.value("code").toString()));
                const int valid_for = obj.value("valid_for").toInt();
                self->totp_countdown_label_->setText(QString("(%1s)").arg(valid_for));
            }
        });
}

} // namespace fincept::screens::crypto
