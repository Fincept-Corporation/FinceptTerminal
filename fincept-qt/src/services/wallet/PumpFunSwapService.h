#pragma once

#include "core/result/Result.h"

#include <QObject>
#include <QString>

#include <functional>

class QNetworkAccessManager;

namespace fincept::wallet {

/// One-shot service that produces a signable Solana transaction for a
/// user-initiated $FNCPT swap. Phase 2 rev 2 — replaces the earlier
/// Jupiter-based `JupiterSwapService`.
///
/// Why this is **not** a `fincept::datahub::Producer`:
///   - Each call has a unique input (action + amount) bound to a single
///     user click; nothing benefits from caching or coalescing.
///   - The output (`tx_base64`) is consumed exactly once by
///     `WalletService::sign_and_send` and never republished.
///   - Per CLAUDE.md D2 (exception clause), one-shot user-initiated
///     services don't have to be Producers.
///
/// The HTTP transport is `pumpportal.fun/api/trade-local`. PumpPortal
/// returns a fully-built unsigned versioned transaction in raw binary
/// (about 1 KB); we base64-encode it and hand it to the wallet via the
/// existing `WalletTxBridge` → `swap.html` pipeline.
class PumpFunSwapService : public QObject {
    Q_OBJECT
  public:
    enum class Action { Buy, Sell };

    /// Result returned by `build_swap()`. Mirrors the shape of
    /// `JupiterSwapService::SwapTransaction` so `SwapPanel` can swap
    /// service implementations without touching its submit-flow.
    struct SwapTransaction {
        QString tx_base64;
        /// PumpPortal does not expose `lastValidBlockHeight` directly. The
        /// field stays in the struct for forward-compatibility (the wallet
        /// adapter reads it from the deserialised tx itself); set to 0 for
        /// "unknown — let the wallet/RPC reject if stale".
        quint64 last_valid_block_height = 0;
    };
    using SwapBuildCallback = std::function<void(Result<SwapTransaction>)>;

    explicit PumpFunSwapService(QObject* parent = nullptr);
    ~PumpFunSwapService() override;

    PumpFunSwapService(const PumpFunSwapService&) = delete;
    PumpFunSwapService& operator=(const PumpFunSwapService&) = delete;

    /// Build an unsigned swap transaction. All parameters are required.
    ///
    /// `mint`             — base58 SPL mint of the token being bought / sold
    ///                      (the **non-SOL** side; SOL is implied as the
    ///                      counter-asset).
    /// `amount`           — number, interpreted per `denominated_in_sol`.
    /// `denominated_in_sol` — `true` means `amount` is in SOL (e.g. "buy
    ///                      0.5 SOL worth of $FNCPT"); `false` means
    ///                      `amount` is in the token (e.g. "sell 1000
    ///                      $FNCPT for SOL"). PumpPortal also accepts
    ///                      `"100%"` as a string when `false`, but this API
    ///                      surfaces a clean numeric path only — clients
    ///                      that need MAX should compute it from balance.
    /// `user_pubkey`      — base58 of the connected wallet.
    /// `slippage_pct`     — integer percent, 1..5. Clamped server-side.
    /// `priority_fee_sol` — SOL prio-fee, e.g. 0.00005. Clamped to a
    ///                      sensible range internally.
    void build_swap(Action action,
                    const QString& mint,
                    double amount,
                    bool denominated_in_sol,
                    const QString& user_pubkey,
                    int slippage_pct,
                    double priority_fee_sol,
                    SwapBuildCallback cb);

  private:
    QNetworkAccessManager* nam_ = nullptr;
};

} // namespace fincept::wallet
