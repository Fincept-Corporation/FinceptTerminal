#pragma once

#include "core/result/Result.h"

#include <QByteArray>
#include <QDialog>
#include <QString>

class QCloseEvent;
class QLabel;
class QPushButton;
class QShowEvent;

namespace fincept::wallet {

class WalletTxBridge;

/// Modal "browser is open, please sign in your wallet" dialog used for any
/// fund-moving operation (Phase 2 swap + burn, Phase 3 lock, Phase 4 stake).
///
/// Mirrors `ConnectWalletDialog`'s shape:
///   - Owns a `WalletTxBridge` (per-dialog instance — single-use).
///   - On show, registers the tx with the bridge, opens the resulting URL in
///     the system browser via `QDesktopServices::openUrl`.
///   - User signs in their installed wallet extension (Phantom/Solflare/etc).
///   - On `/result` POST, the bridge resolves and the dialog accepts.
///   - On cancel / timeout / error, the dialog rejects.
///
/// The dialog is intentionally minimal — it does not render the action
/// summary. That's `WalletActionConfirmDialog`'s job, which runs *before*
/// this dialog opens. The trip from confirm to sign is:
///   WalletActionConfirmDialog (Accept)
///     → WalletService::sign_and_send(tx_b64, cb)
///       → SignTransactionDialog (owns WalletTxBridge)
///         → system browser → wallet ext → POST /result
///       → cb(Result<sig_b58>)
class SignTransactionDialog : public QDialog {
    Q_OBJECT
  public:
    /// `tx_base64`: the unsigned (or partially-signed) transaction body to
    ///              forward to the wallet for `signAndSendTransaction`.
    /// `title`:     dialog window title, e.g. "Sign swap" / "Sign burn".
    /// `lede`:      one-liner shown above the status message, e.g.
    ///              "Sign the swap in Phantom to complete the trade."
    SignTransactionDialog(const QString& tx_base64,
                          const QString& title,
                          const QString& lede,
                          QWidget* parent = nullptr);
    ~SignTransactionDialog() override;

    /// Available after the dialog closes with `Accepted` — the base58
    /// signature returned by the wallet. Empty if the dialog was rejected.
    QString signature() const noexcept { return signature_; }

    /// If the dialog rejected, this carries the reason: "user_cancelled",
    /// "timeout", or whatever the bridge / wallet reported.
    QString failure_reason() const noexcept { return failure_reason_; }

  protected:
    void showEvent(QShowEvent* e) override;
    void closeEvent(QCloseEvent* e) override;

  private:
    void start_signing();
    void on_signature_resolved(Result<QString> r);

    WalletTxBridge* bridge_ = nullptr;
    QLabel* status_label_ = nullptr;
    QPushButton* reopen_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;

    QString tx_base64_;
    QString lede_;
    QString last_open_url_;
    QString signature_;
    QString failure_reason_;
    bool resolved_ = false;
};

} // namespace fincept::wallet
