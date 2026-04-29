#pragma once

#include "core/result/Result.h"

#include <QObject>
#include <QString>

#include <functional>

class QWidget;

namespace fincept::wallet {

class WalletBalanceProducer;
class TokenPriceProducer;
class WalletActivityProducer;

/// In-memory wallet state. Persisted across runs (pubkey + label only;
/// **never** keys, signatures, or session secrets).
struct WalletState {
    QString pubkey_b58;
    QString label;       ///< wallet name as reported by the connect page (e.g. "phantom")
    qint64 connected_at_ms = 0;
    bool soft_connected = false;  ///< restored from SecureStorage but not yet re-challenged
    bool is_connected() const noexcept { return !pubkey_b58.isEmpty(); }
};

/// Singleton owning the user's connected wallet identity.
///
/// Phase 1 scope: read-only — pubkey, label, balance display.
/// No transaction signing or fund movement happens here.
///
/// Persisted state lives in `SecureStorage`. We restore it on startup as a
/// "soft-connected" session: balances are visible but the next operation that
/// requires user authority will trigger a fresh sign-message challenge.
class WalletService : public QObject {
    Q_OBJECT
  public:
    static WalletService& instance();

    /// Ensure the producers are registered with DataHub. Idempotent.
    /// Called once from main.cpp during startup.
    void ensure_registered_with_hub();

    /// Restore pubkey from SecureStorage if present. No network calls.
    /// Idempotent. Emits `wallet_connected` if a saved state was restored.
    void restore_from_storage();

    /// Spawn the connect dialog modally on `parent`. Async — returns
    /// immediately. On success, emits `wallet_connected(pubkey)`.
    void connect_with_dialog(QWidget* parent);

    /// Forget the current wallet. Clears SecureStorage state and emits
    /// `wallet_disconnected()`. Safe to call when not connected.
    void disconnect();

    /// Current state snapshot. Read on the main thread.
    const WalletState& state() const noexcept { return state_; }
    bool is_connected() const noexcept { return state_.is_connected(); }
    QString current_pubkey() const noexcept { return state_.pubkey_b58; }

    /// Switch the balance producer between polling and streaming mode.
    /// Persists the choice to SecureStorage so it survives restart.
    /// Emits balance_mode_changed.
    void set_balance_mode(bool stream);
    bool balance_mode_is_stream() const noexcept;

    /// User-driven hard refresh of the balance topic. In Poll mode this is a
    /// REST round-trip; in Stream mode it re-runs the seed (the only way
    /// to refresh FNCPT, which has no per-account WS subscription). No-op
    /// when not connected.
    void force_balance_refresh();

    /// The PumpPortal swap service owned by this WalletService. One-shot;
    /// not a Producer (per CLAUDE.md D2 exception). `SwapPanel` calls
    /// `build_swap()` directly when the user clicks SWAP.
    class PumpFunSwapService* swap_service() noexcept { return swap_service_; }

    /// Forward `tx_base64` to the user's wallet for `signAndSendTransaction`.
    ///
    /// Opens a small modal `SignTransactionDialog` parented to `parent` (or
    /// `nullptr` for a top-level dialog), spins up a per-call `WalletTxBridge`,
    /// and launches the wallet HTML page in the user's default browser.
    ///
    /// `dialog_title` and `dialog_lede` are the user-facing strings shown on
    /// the wait dialog (e.g. "Sign swap" / "Approve in Phantom to complete
    /// the trade.").
    ///
    /// `cb` resolves with the base58 signature on success, or an error code
    /// on cancel / timeout / wallet error. Always called on the UI thread.
    void sign_and_send(const QString& tx_base64,
                       const QString& dialog_title,
                       const QString& dialog_lede,
                       QWidget* parent,
                       std::function<void(Result<QString>)> cb);

  signals:
    void wallet_connected(QString pubkey_b58, QString label);
    void wallet_disconnected();
    void connect_failed(QString reason);
    void balance_mode_changed(bool is_stream);

  private:
    WalletService();
    ~WalletService() override;
    WalletService(const WalletService&) = delete;
    WalletService& operator=(const WalletService&) = delete;

    void persist_state();
    void clear_persisted_state();
    void set_state(WalletState s);

    WalletState state_;
    bool hub_registered_ = false;
    WalletBalanceProducer* balance_producer_ = nullptr;
    TokenPriceProducer* price_producer_ = nullptr;
    class PumpFunSwapService* swap_service_ = nullptr;
    WalletActivityProducer* activity_producer_ = nullptr;
};

} // namespace fincept::wallet
