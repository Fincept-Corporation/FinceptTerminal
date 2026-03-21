#include "screens/crypto_trading/CryptoCredentials.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QGroupBox>

namespace fincept::screens::crypto {

CryptoCredentials::CryptoCredentials(const QString& exchange_id, QWidget* parent)
    : QDialog(parent), exchange_id_(exchange_id) {
    setWindowTitle(QString("API Credentials — %1").arg(exchange_id.toUpper()));
    setMinimumWidth(400);
    setStyleSheet("QDialog { background: #0d1117; color: #ccc; }");

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* info = new QLabel(
        "Enter your exchange API credentials for live trading.\n"
        "Keys are stored locally in encrypted secure storage.");
    info->setStyleSheet("color: #888; font-size: 12px;");
    info->setWordWrap(true);
    layout->addWidget(info);

    QString input_style = "QLineEdit { background: #1a1a2e; border: 1px solid #333; border-radius: 3px; "
                           "color: #ccc; padding: 6px; font-size: 13px; }";

    layout->addWidget(new QLabel("API Key:"));
    key_edit_ = new QLineEdit;
    key_edit_->setPlaceholderText("Enter API key");
    key_edit_->setStyleSheet(input_style);
    layout->addWidget(key_edit_);

    layout->addWidget(new QLabel("API Secret:"));
    secret_edit_ = new QLineEdit;
    secret_edit_->setPlaceholderText("Enter API secret");
    secret_edit_->setEchoMode(QLineEdit::Password);
    secret_edit_->setStyleSheet(input_style);
    layout->addWidget(secret_edit_);

    layout->addWidget(new QLabel("Password (OKX/KuCoin):"));
    password_edit_ = new QLineEdit;
    password_edit_->setPlaceholderText("Optional — only for exchanges that require it");
    password_edit_->setEchoMode(QLineEdit::Password);
    password_edit_->setStyleSheet(input_style);
    layout->addWidget(password_edit_);

    status_label_ = new QLabel("");
    status_label_->setStyleSheet("font-size: 12px;");
    layout->addWidget(status_label_);

    // Buttons
    auto* btn_row = new QHBoxLayout;
    auto* clear_btn = new QPushButton("Clear");
    clear_btn->setStyleSheet("QPushButton { background: #5c1a1a; color: #ff4444; padding: 6px 16px; "
                              "border-radius: 4px; } QPushButton:hover { background: #772222; }");
    connect(clear_btn, &QPushButton::clicked, this, &CryptoCredentials::on_clear);
    btn_row->addWidget(clear_btn);

    btn_row->addStretch();

    auto* save_btn = new QPushButton("Save & Connect");
    save_btn->setStyleSheet("QPushButton { background: #00aa55; color: white; padding: 6px 16px; "
                             "border-radius: 4px; font-weight: bold; } QPushButton:hover { background: #00cc66; }");
    connect(save_btn, &QPushButton::clicked, this, &CryptoCredentials::on_save);
    btn_row->addWidget(save_btn);

    layout->addLayout(btn_row);
}

QString CryptoCredentials::api_key() const { return key_edit_->text().trimmed(); }
QString CryptoCredentials::api_secret() const { return secret_edit_->text().trimmed(); }
QString CryptoCredentials::password() const { return password_edit_->text().trimmed(); }

void CryptoCredentials::set_values(const QString& key, const QString& secret, const QString& pw) {
    key_edit_->setText(key);
    secret_edit_->setText(secret);
    password_edit_->setText(pw);
}

void CryptoCredentials::on_save() {
    if (api_key().isEmpty() || api_secret().isEmpty()) {
        status_label_->setText("API key and secret are required");
        status_label_->setStyleSheet("color: #ff4444; font-size: 12px;");
        return;
    }
    emit credentials_saved(api_key(), api_secret(), password());
    status_label_->setText("Credentials saved");
    status_label_->setStyleSheet("color: #00ff88; font-size: 12px;");
    accept();
}

void CryptoCredentials::on_clear() {
    key_edit_->clear();
    secret_edit_->clear();
    password_edit_->clear();
    status_label_->setText("Credentials cleared");
    status_label_->setStyleSheet("color: #ff8800; font-size: 12px;");
}

} // namespace fincept::screens::crypto
