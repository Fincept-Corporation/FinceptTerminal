// EquityCredentials.cpp — broker API key dialog
#include "screens/equity_trading/EquityCredentials.h"
#include "screens/equity_trading/EquityTypes.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::screens::equity {

EquityCredentials::EquityCredentials(const QString& broker_id, QWidget* parent)
    : QDialog(parent), broker_id_(broker_id) {
    setWindowTitle(QString("Connect Broker — %1").arg(broker_id.toUpper()));
    setFixedSize(380, 300);
    setStyleSheet(
        "QDialog { background: #0a0a0a; color: #e5e5e5; }"
        "QLabel { color: #808080; font-size: 11px; font-weight: 700; }"
        "QLineEdit { background: #080808; border: 1px solid #222222; color: #e5e5e5; "
        "  padding: 6px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #d97706; }"
        "QPushButton { padding: 8px 16px; font-weight: 700; font-size: 12px; }"
    );

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* title = new QLabel(QString("BROKER: %1").arg(broker_id.toUpper()));
    title->setStyleSheet("color: #d97706; font-size: 14px; font-weight: 700;");
    layout->addWidget(title);

    layout->addWidget(new QLabel("API KEY"));
    key_edit_ = new QLineEdit;
    key_edit_->setPlaceholderText("Enter API key...");
    layout->addWidget(key_edit_);

    layout->addWidget(new QLabel("API SECRET"));
    secret_edit_ = new QLineEdit;
    secret_edit_->setPlaceholderText("Enter API secret...");
    secret_edit_->setEchoMode(QLineEdit::Password);
    layout->addWidget(secret_edit_);

    layout->addWidget(new QLabel("AUTH CODE / TOTP"));
    auth_code_edit_ = new QLineEdit;
    auth_code_edit_->setPlaceholderText("Auth code (if required)...");
    layout->addWidget(auth_code_edit_);

    // Buttons
    auto* btn_row = new QHBoxLayout;
    auto* save_btn = new QPushButton("CONNECT");
    save_btn->setStyleSheet(
        "background: rgba(217,119,6,0.15); color: #d97706; border: 1px solid #92400e;");
    save_btn->setCursor(Qt::PointingHandCursor);
    connect(save_btn, &QPushButton::clicked, this, &EquityCredentials::on_save);

    auto* clear_btn = new QPushButton("CLEAR");
    clear_btn->setStyleSheet(
        "background: rgba(220,38,38,0.1); color: #dc2626; border: 1px solid #7f1d1d;");
    clear_btn->setCursor(Qt::PointingHandCursor);
    connect(clear_btn, &QPushButton::clicked, this, &EquityCredentials::on_clear);

    btn_row->addWidget(save_btn);
    btn_row->addWidget(clear_btn);
    layout->addLayout(btn_row);

    // Status
    status_label_ = new QLabel("");
    status_label_->setWordWrap(true);
    layout->addWidget(status_label_);

    layout->addStretch();
}

void EquityCredentials::on_save() {
    const auto key = key_edit_->text().trimmed();
    const auto secret = secret_edit_->text().trimmed();
    const auto auth = auth_code_edit_->text().trimmed();

    if (key.isEmpty() || secret.isEmpty()) {
        status_label_->setText("API Key and Secret are required");
        status_label_->setStyleSheet("color: #dc2626;");
        return;
    }

    emit credentials_saved(broker_id_, key, secret, auth);
    status_label_->setText("Credentials saved. Connecting...");
    status_label_->setStyleSheet("color: #16a34a;");
    accept();
}

void EquityCredentials::on_clear() {
    key_edit_->clear();
    secret_edit_->clear();
    auth_code_edit_->clear();
    status_label_->setText("Cleared");
    status_label_->setStyleSheet("color: #ca8a04;");
}

} // namespace fincept::screens::equity
