#include "services/wallet/SignTransactionDialog.h"

#include "core/logging/Logger.h"
#include "services/wallet/WalletTxBridge.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::wallet {

SignTransactionDialog::SignTransactionDialog(const QString& tx_base64,
                                             const QString& title,
                                             const QString& lede,
                                             QWidget* parent)
    : QDialog(parent), tx_base64_(tx_base64), lede_(lede) {
    setWindowTitle(title.isEmpty() ? tr("Sign transaction") : title);
    setModal(true);
    resize(440, 220);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    auto* heading = new QLabel(title.isEmpty() ? tr("Sign transaction") : title, this);
    heading->setObjectName(QStringLiteral("walletDialogTitle"));
    layout->addWidget(heading);

    auto* lede_label = new QLabel(
        lede_.isEmpty()
            ? tr("Approve the transaction in your wallet to complete this action.")
            : lede_,
        this);
    lede_label->setWordWrap(true);
    layout->addWidget(lede_label);

    status_label_ = new QLabel(this);
    status_label_->setObjectName(QStringLiteral("walletDialogStatus"));
    status_label_->setWordWrap(true);
    status_label_->setText(tr("Opening your browser to relay the transaction…"));
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
        if (resolved_) return;
        on_signature_resolved(Result<QString>::err("user_cancelled"));
    });
    connect(reopen_button_, &QPushButton::clicked, this, [this]() {
        if (!last_open_url_.isEmpty()) {
            QDesktopServices::openUrl(QUrl(last_open_url_));
        }
    });
}

SignTransactionDialog::~SignTransactionDialog() = default;

void SignTransactionDialog::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
    if (!bridge_) {
        start_signing();
    }
}

void SignTransactionDialog::closeEvent(QCloseEvent* e) {
    if (!resolved_) {
        // Window-close X behaves like cancel — resolve the bridge first so the
        // upstream callback fires with a clear reason, then let Qt close us.
        on_signature_resolved(Result<QString>::err("user_cancelled"));
    }
    QDialog::closeEvent(e);
}

void SignTransactionDialog::start_signing() {
    bridge_ = new WalletTxBridge(this);

    QPointer<SignTransactionDialog> self = this;
    auto url_res = bridge_->request_signature(
        tx_base64_,
        [self](Result<QString> r) {
            if (!self) return;
            self->on_signature_resolved(std::move(r));
        },
        /*timeout_seconds=*/180); // give the user 3 min to approve

    if (url_res.is_err()) {
        on_signature_resolved(
            Result<QString>::err("bridge_failed: " + url_res.error()));
        return;
    }
    last_open_url_ = url_res.value();
    QDesktopServices::openUrl(QUrl(last_open_url_));
    status_label_->setText(
        tr("Browser opened. Approve the transaction in your wallet. The terminal "
           "is waiting on a single-use loopback bridge — this dialog will close "
           "automatically when the wallet returns the signature."));
}

void SignTransactionDialog::on_signature_resolved(Result<QString> r) {
    if (resolved_) return;
    resolved_ = true;

    if (r.is_ok()) {
        signature_ = r.value();
        LOG_INFO("SignTxDialog", "signature received");
        accept();
        return;
    }
    failure_reason_ = QString::fromStdString(r.error());
    LOG_WARN("SignTxDialog", "rejected: " + failure_reason_);
    reject();
}

} // namespace fincept::wallet
