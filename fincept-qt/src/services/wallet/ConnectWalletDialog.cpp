#include "services/wallet/ConnectWalletDialog.h"

#include "core/logging/Logger.h"
#include "services/wallet/Ed25519Verifier.h"
#include "services/wallet/LocalWalletBridge.h"

#include <QByteArray>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QShowEvent>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::wallet {

namespace {

QByteArray make_random_nonce() {
    QByteArray buf;
    buf.resize(32);
    QRandomGenerator::system()->generate(reinterpret_cast<quint32*>(buf.data()),
                                         reinterpret_cast<quint32*>(buf.data() + buf.size()));
    return buf.toHex();
}

} // namespace

ConnectWalletDialog::ConnectWalletDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Connect Wallet"));
    setModal(true);
    resize(440, 220);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("Connect your Solana wallet"), this);
    title->setObjectName(QStringLiteral("walletDialogTitle"));
    layout->addWidget(title);

    status_label_ = new QLabel(this);
    status_label_->setObjectName(QStringLiteral("walletDialogStatus"));
    status_label_->setWordWrap(true);
    status_label_->setText(tr("Opening your browser to complete the handshake…"));
    layout->addWidget(status_label_);

    layout->addStretch(1);

    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    reopen_button_ = new QPushButton(tr("Reopen browser"), this);
    cancel_button_ = new QPushButton(tr("Cancel"), this);
    cancel_button_->setObjectName(QStringLiteral("walletDialogCancel"));
    row->addStretch(1);
    row->addWidget(reopen_button_);
    row->addWidget(cancel_button_);
    layout->addLayout(row);

    connect(cancel_button_, &QPushButton::clicked, this, [this]() {
        fail_and_close(tr("cancelled by user"));
    });
    connect(reopen_button_, &QPushButton::clicked, this, [this]() {
        if (!last_open_url_.isEmpty()) {
            QDesktopServices::openUrl(QUrl(last_open_url_));
        }
    });
}

ConnectWalletDialog::~ConnectWalletDialog() = default;

void ConnectWalletDialog::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
    if (!bridge_) {
        start_handshake();
    }
}

void ConnectWalletDialog::closeEvent(QCloseEvent* e) {
    if (bridge_) {
        bridge_->stop();
    }
    QDialog::closeEvent(e);
}

void ConnectWalletDialog::start_handshake() {
    nonce_hex_ = make_random_nonce();
    expected_message_ = QByteArrayLiteral(
        "Fincept Terminal wallet-connect challenge. Nonce: ") + nonce_hex_;

    bridge_ = new LocalWalletBridge(this);
    connect(bridge_, &LocalWalletBridge::connect_payload, this,
            &ConnectWalletDialog::on_payload);
    connect(bridge_, &LocalWalletBridge::timed_out, this,
            &ConnectWalletDialog::on_timed_out);
    connect(bridge_, &LocalWalletBridge::bridge_error, this,
            &ConnectWalletDialog::on_bridge_error);

    last_open_url_ = bridge_->start(nonce_hex_, /*timeout_seconds=*/120);
    if (last_open_url_.isEmpty()) {
        fail_and_close(tr("could not start local bridge server"));
        return;
    }
    QDesktopServices::openUrl(QUrl(last_open_url_));
    status_label_->setText(
        tr("Browser opened. Approve the connection and the signature in your wallet."));
}

void ConnectWalletDialog::on_payload(QString pubkey, QString signature, QString label) {
    LOG_INFO("ConnectWalletDialog",
             QStringLiteral("on_payload: pubkey=%1 sig_chars=%2 label=%3 msg_len=%4")
                 .arg(pubkey)
                 .arg(signature.size())
                 .arg(label)
                 .arg(expected_message_.size()));
    if (handshake_done_) {
        LOG_WARN("ConnectWalletDialog", "ignoring late payload (handshake already done)");
        return;
    }
    if (!Ed25519Verifier::verify(pubkey, expected_message_, signature)) {
        LOG_WARN("ConnectWalletDialog",
                 QStringLiteral("signature verification FAILED. pubkey=%1 sig=%2 msg_first40='%3'")
                     .arg(pubkey)
                     .arg(signature)
                     .arg(QString::fromUtf8(expected_message_.left(40))));
        fail_and_close(tr("signature verification failed"));
        return;
    }
    handshake_done_ = true;
    if (bridge_) {
        bridge_->stop();
    }
    LOG_INFO("ConnectWalletDialog", "Handshake verified for " + pubkey + " (" + label + ")");
    emit connected(pubkey, label);
    accept();
}

void ConnectWalletDialog::on_timed_out() {
    if (handshake_done_) {
        return;
    }
    fail_and_close(tr("timed out waiting for browser callback"));
}

void ConnectWalletDialog::on_bridge_error(QString message) {
    fail_and_close(tr("bridge error: %1").arg(message));
}

void ConnectWalletDialog::fail_and_close(const QString& reason) {
    if (handshake_done_) {
        return;
    }
    handshake_done_ = true;
    if (bridge_) {
        bridge_->stop();
    }
    emit failed(reason);
    reject();
}

} // namespace fincept::wallet
