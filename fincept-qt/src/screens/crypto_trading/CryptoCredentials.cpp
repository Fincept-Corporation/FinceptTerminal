#include "screens/crypto_trading/CryptoCredentials.h"

#include "services/crypto/TotpService.h"
#include "ui/theme/Theme.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

using namespace fincept::ui;

// Single dialog-level stylesheet using objectName selectors — 18+ inline
// setStyleSheet calls collapsed into one CSS parse. Applied in the ctor.
namespace {
static QString kDialogStyle() {
    const QString mono = "'Consolas', 'Courier New', monospace";
    return QString(
        // Dialog background
        "QDialog { background: %1; color: %2; font-family: %13; }"
        // Inputs (API key/secret/password/TOTP)
        "QLineEdit { background: %3; border: 1px solid %4; color: %2; padding: 6px; "
        "           font-size: 13px; font-family: %13; }"
        "QLineEdit:focus { border-color: %5; }"
        // Labels by role
        "QLabel#credTitle { color: %6; font-size: 14px; font-weight: 700; letter-spacing: 0.5px; font-family: %13; }"
        "QLabel#credInfo { color: %7; font-size: 12px; font-family: %13; }"
        "QLabel#credFieldLabel { color: %7; font-size: 12px; font-weight: 700; letter-spacing: 0.5px; font-family: %13; }"
        "QLabel#credTotpCode { color: %6; font-size: 18px; font-weight: 700; letter-spacing: 2px; font-family: %13; }"
        "QLabel#credTotpCountdown { color: %8; font-size: 11px; font-family: %13; }"
        "QLabel#credStatusErr { color: %9; font-size: 12px; font-family: %13; }"
        "QLabel#credStatusOk { color: %10; font-size: 12px; font-family: %13; }"
        "QLabel#credStatusWarn { color: %11; font-size: 12px; font-family: %13; }"
        // Buttons
        "QPushButton#credClearBtn { background: rgba(220,38,38,0.1); color: %9; border: 1px solid %12; "
        "                          padding: 6px 16px; font-weight: 700; font-size: 12px; font-family: %13; }"
        "QPushButton#credClearBtn:hover { background: %9; color: %2; }"
        "QPushButton#credSaveBtn { background: rgba(217,119,6,0.1); color: %6; border: 1px solid %14; "
        "                         padding: 6px 16px; font-weight: 700; font-size: 12px; font-family: %13; }"
        "QPushButton#credSaveBtn:hover { background: %6; color: %1; }")
        .arg(colors::BG_BASE(),           // %1
             colors::TEXT_PRIMARY(),      // %2
             colors::BG_SURFACE(),        // %3
             colors::BORDER_DIM(),        // %4
             colors::BORDER_BRIGHT(),     // %5
             colors::AMBER(),             // %6
             colors::TEXT_SECONDARY(),    // %7
             colors::TEXT_TERTIARY(),     // %8
             colors::NEGATIVE(),          // %9
             colors::POSITIVE(),          // %10
             colors::WARNING(),           // %11
             colors::NEGATIVE_DIM(),      // %12
             mono)                        // %13
        .arg(colors::AMBER_DIM());        // %14
}
} // namespace

namespace fincept::screens::crypto {

CryptoCredentials::CryptoCredentials(const QString& exchange_id, QWidget* parent)
    : QDialog(parent), exchange_id_(exchange_id) {
    setWindowTitle(tr("API Credentials — %1").arg(exchange_id.toUpper()));
    setMinimumWidth(400);
    // ONE dialog-level stylesheet drives all widgets via objectName selectors.
    setStyleSheet(kDialogStyle());

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(14, 14, 14, 14);

    // Header
    title_label_ = new QLabel(tr("API CREDENTIALS  %1").arg(exchange_id.toUpper()));
    title_label_->setObjectName("credTitle");
    layout->addWidget(title_label_);

    info_label_ = new QLabel(tr("Enter your exchange API credentials for live trading.\n"
                                "Keys are stored locally in encrypted secure storage."));
    info_label_->setObjectName("credInfo");
    info_label_->setWordWrap(true);
    layout->addWidget(info_label_);

    layout->addSpacing(4);

    // API Key
    key_field_label_ = new QLabel(tr("API KEY"));
    key_field_label_->setObjectName("credFieldLabel");
    layout->addWidget(key_field_label_);
    key_edit_ = new QLineEdit;
    key_edit_->setPlaceholderText(tr("Enter API key"));
    key_edit_->setFixedHeight(28);
    layout->addWidget(key_edit_);

    // API Secret
    secret_field_label_ = new QLabel(tr("API SECRET"));
    secret_field_label_->setObjectName("credFieldLabel");
    layout->addWidget(secret_field_label_);
    secret_edit_ = new QLineEdit;
    secret_edit_->setPlaceholderText(tr("Enter API secret"));
    secret_edit_->setEchoMode(QLineEdit::Password);
    secret_edit_->setFixedHeight(28);
    layout->addWidget(secret_edit_);

    // Password / API passphrase — required by OKX, KuCoin, Bitget, Coinbase.
    password_field_label_ = new QLabel(tr("PASSPHRASE (OKX/KUCOIN/BITGET/COINBASE)"));
    password_field_label_->setObjectName("credFieldLabel");
    layout->addWidget(password_field_label_);
    password_edit_ = new QLineEdit;
    password_edit_->setPlaceholderText(tr("Optional"));
    password_edit_->setEchoMode(QLineEdit::Password);
    password_edit_->setFixedHeight(28);
    layout->addWidget(password_edit_);

    // Wallet address + private key — required by Hyperliquid (DEX-style auth)
    // instead of API key/secret. Shown only for that exchange.
    wallet_field_label_ = new QLabel(tr("WALLET ADDRESS"));
    wallet_field_label_->setObjectName("credFieldLabel");
    layout->addWidget(wallet_field_label_);
    wallet_edit_ = new QLineEdit;
    wallet_edit_->setPlaceholderText(tr("Enter wallet address (0x…)"));
    wallet_edit_->setFixedHeight(28);
    layout->addWidget(wallet_edit_);

    private_key_field_label_ = new QLabel(tr("PRIVATE KEY"));
    private_key_field_label_->setObjectName("credFieldLabel");
    layout->addWidget(private_key_field_label_);
    private_key_edit_ = new QLineEdit;
    private_key_edit_->setPlaceholderText(tr("Enter private key"));
    private_key_edit_->setEchoMode(QLineEdit::Password);
    private_key_edit_->setFixedHeight(28);
    layout->addWidget(private_key_edit_);

    // TOTP section
    layout->addSpacing(4);
    totp_field_label_ = new QLabel(tr("TOTP SECRET (2FA)"));
    totp_field_label_->setObjectName("credFieldLabel");
    layout->addWidget(totp_field_label_);

    totp_secret_edit_ = new QLineEdit;
    totp_secret_edit_->setPlaceholderText(tr("Base32 secret (optional)"));
    totp_secret_edit_->setEchoMode(QLineEdit::Password);
    totp_secret_edit_->setFixedHeight(28);
    layout->addWidget(totp_secret_edit_);

    auto* totp_row = new QHBoxLayout;
    totp_row->setSpacing(8);

    totp_code_label_ = new QLabel(tr("CODE: --"));
    totp_code_label_->setObjectName("credTotpCode");
    totp_row->addWidget(totp_code_label_);

    totp_countdown_label_ = new QLabel("");
    totp_countdown_label_->setObjectName("credTotpCountdown");
    totp_row->addWidget(totp_countdown_label_);
    totp_row->addStretch();
    layout->addLayout(totp_row);

    totp_timer_ = new QTimer(this);
    totp_timer_->setInterval(1000);
    connect(totp_timer_, &QTimer::timeout, this, &CryptoCredentials::on_totp_tick);
    connect(totp_secret_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.trimmed().isEmpty()) {
            totp_timer_->stop();
            totp_code_label_->setText(tr("CODE: --"));
            totp_countdown_label_->setText("");
        } else {
            refresh_totp();
            totp_timer_->start();
        }
    });

    // Status — objectName flipped by on_save/on_clear to pick status color
    status_label_ = new QLabel("");
    status_label_->setObjectName("credStatusOk");
    layout->addWidget(status_label_);

    // Buttons — Obsidian danger + accent
    auto* btn_row = new QHBoxLayout;

    clear_btn_ = new QPushButton(tr("CLEAR"));
    clear_btn_->setObjectName("credClearBtn");
    connect(clear_btn_, &QPushButton::clicked, this, &CryptoCredentials::on_clear);
    btn_row->addWidget(clear_btn_);

    btn_row->addStretch();

    save_btn_ = new QPushButton(tr("SAVE & CONNECT"));
    save_btn_->setObjectName("credSaveBtn");
    connect(save_btn_, &QPushButton::clicked, this, &CryptoCredentials::on_save);
    btn_row->addWidget(save_btn_);

    layout->addLayout(btn_row);

    // Hyperliquid uses wallet-based auth (walletAddress + privateKey) rather
    // than API key/secret. Toggle which credential fields are visible.
    const bool wallet_auth = (exchange_id_.compare(QStringLiteral("hyperliquid"), Qt::CaseInsensitive) == 0);
    key_field_label_->setVisible(!wallet_auth);
    key_edit_->setVisible(!wallet_auth);
    secret_field_label_->setVisible(!wallet_auth);
    secret_edit_->setVisible(!wallet_auth);
    password_field_label_->setVisible(!wallet_auth);
    password_edit_->setVisible(!wallet_auth);
    totp_field_label_->setVisible(!wallet_auth);
    totp_secret_edit_->setVisible(!wallet_auth);
    totp_code_label_->setVisible(!wallet_auth);
    totp_countdown_label_->setVisible(!wallet_auth);
    wallet_field_label_->setVisible(wallet_auth);
    wallet_edit_->setVisible(wallet_auth);
    private_key_field_label_->setVisible(wallet_auth);
    private_key_edit_->setVisible(wallet_auth);
}

void CryptoCredentials::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void CryptoCredentials::retranslateUi() {
    setWindowTitle(tr("API Credentials — %1").arg(exchange_id_.toUpper()));
    if (title_label_)          title_label_->setText(tr("API CREDENTIALS  %1").arg(exchange_id_.toUpper()));
    if (info_label_)           info_label_->setText(tr("Enter your exchange API credentials for live trading.\n"
                                                       "Keys are stored locally in encrypted secure storage."));
    if (key_field_label_)      key_field_label_->setText(tr("API KEY"));
    if (key_edit_)             key_edit_->setPlaceholderText(tr("Enter API key"));
    if (secret_field_label_)   secret_field_label_->setText(tr("API SECRET"));
    if (secret_edit_)          secret_edit_->setPlaceholderText(tr("Enter API secret"));
    if (password_field_label_) password_field_label_->setText(tr("PASSPHRASE (OKX/KUCOIN/BITGET/COINBASE)"));
    if (password_edit_)        password_edit_->setPlaceholderText(tr("Optional"));
    if (wallet_field_label_)   wallet_field_label_->setText(tr("WALLET ADDRESS"));
    if (wallet_edit_)          wallet_edit_->setPlaceholderText(tr("Enter wallet address (0x…)"));
    if (private_key_field_label_) private_key_field_label_->setText(tr("PRIVATE KEY"));
    if (private_key_edit_)     private_key_edit_->setPlaceholderText(tr("Enter private key"));
    if (totp_field_label_)     totp_field_label_->setText(tr("TOTP SECRET (2FA)"));
    if (totp_secret_edit_)     totp_secret_edit_->setPlaceholderText(tr("Base32 secret (optional)"));
    if (clear_btn_)            clear_btn_->setText(tr("CLEAR"));
    if (save_btn_)             save_btn_->setText(tr("SAVE & CONNECT"));
    // TOTP code label only resets to the idle placeholder when no secret is set;
    // a live code is data and is left untouched.
    if (totp_code_label_ && totp_secret_edit_ && totp_secret_edit_->text().trimmed().isEmpty())
        totp_code_label_->setText(tr("CODE: --"));
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
QString CryptoCredentials::wallet_address() const {
    return wallet_edit_->text().trimmed();
}
QString CryptoCredentials::private_key() const {
    return private_key_edit_->text().trimmed();
}

void CryptoCredentials::set_values(const QString& key, const QString& secret, const QString& pw,
                                   const QString& wallet, const QString& pk) {
    key_edit_->setText(key);
    secret_edit_->setText(secret);
    password_edit_->setText(pw);
    wallet_edit_->setText(wallet);
    private_key_edit_->setText(pk);
}

void CryptoCredentials::on_save() {
    const bool wallet_auth = (exchange_id_.compare(QStringLiteral("hyperliquid"), Qt::CaseInsensitive) == 0);
    if (wallet_auth) {
        if (wallet_address().isEmpty() || private_key().isEmpty()) {
            set_status(tr("Wallet address and private key are required"), "credStatusErr");
            return;
        }
    } else if (api_key().isEmpty() || api_secret().isEmpty()) {
        set_status(tr("API key and secret are required"), "credStatusErr");
        return;
    }
    emit credentials_saved(api_key(), api_secret(), password(), wallet_address(), private_key());
    set_status(tr("Credentials saved"), "credStatusOk");
    accept();
}

void CryptoCredentials::on_clear() {
    key_edit_->clear();
    secret_edit_->clear();
    password_edit_->clear();
    wallet_edit_->clear();
    private_key_edit_->clear();
    set_status(tr("Credentials cleared"), "credStatusWarn");
}

void CryptoCredentials::set_status(const QString& text, const QString& object_name) {
    status_label_->setText(text);
    if (status_label_->objectName() != object_name) {
        status_label_->setObjectName(object_name);
        // Force re-evaluation of object-name-based selectors in the dialog stylesheet.
        status_label_->style()->unpolish(status_label_);
        status_label_->style()->polish(status_label_);
    }
}

void CryptoCredentials::on_totp_tick() {
    refresh_totp();
}

void CryptoCredentials::refresh_totp() {
    const QString secret = totp_secret_edit_->text().trimmed();
    if (secret.isEmpty())
        return;

    QPointer<CryptoCredentials> self = this;
    services::crypto::TotpService::instance().generate(
        secret, [self](const services::crypto::TotpResult& r) {
            if (!self)
                return;
            if (!r.success) {
                self->totp_code_label_->setText(tr("CODE: ERR"));
                self->totp_countdown_label_->setText("");
                return;
            }
            self->totp_code_label_->setText(tr("CODE: %1").arg(r.code));
            self->totp_countdown_label_->setText(QString("(%1s)").arg(r.valid_for));
        });
}

} // namespace fincept::screens::crypto
