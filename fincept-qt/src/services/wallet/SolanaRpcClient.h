#pragma once

#include "core/result/Result.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace fincept::wallet {

/// Token balance for a single SPL mint owned by some wallet.
struct SplTokenBalance {
    QString mint;       ///< base58 mint address
    QString amount_raw; ///< integer string (token units, not decimal-shifted)
    int decimals = 0;
    double ui_amount() const noexcept;
};

/// Thin async wrapper over Solana JSON-RPC.
///
/// One QNetworkAccessManager per service (P10). All callbacks run on the
/// originating QObject's thread.
///
/// Endpoint resolution order:
///   1. KeyConfigManager key "solana/rpc_url"  (full URL, takes precedence)
///   2. KeyConfigManager key "solana/helius_api_key" → Helius mainnet URL
///   3. Fallback: https://api.mainnet-beta.solana.com  (rate-limited, dev only)
class SolanaRpcClient : public QObject {
    Q_OBJECT
  public:
    explicit SolanaRpcClient(QObject* parent = nullptr);
    ~SolanaRpcClient() override;

    SolanaRpcClient(const SolanaRpcClient&) = delete;
    SolanaRpcClient& operator=(const SolanaRpcClient&) = delete;

    using BalanceCallback = std::function<void(Result<quint64>)>;
    using TokenBalancesCallback =
        std::function<void(Result<std::vector<SplTokenBalance>>)>;

    /// `getLatestBlockhash` result. `last_valid_block_height` is the slot at
    /// which a transaction signed against `blockhash` becomes invalid.
    struct LatestBlockhash {
        QString blockhash;
        quint64 last_valid_block_height = 0;
    };
    using LatestBlockhashCallback = std::function<void(Result<LatestBlockhash>)>;

    /// One row of `getSignatureStatuses`.
    struct SignatureStatus {
        QString signature;
        bool found = false;
        QString confirmation_status;  ///< "processed" | "confirmed" | "finalized"
        QString err;                  ///< empty if no error
        quint64 slot = 0;
    };
    using SignatureStatusesCallback =
        std::function<void(Result<std::vector<SignatureStatus>>)>;

    /// One row of `getSignaturesForAddress`.
    struct SignatureRow {
        QString signature;
        quint64 slot = 0;
        qint64 block_time = 0;        ///< unix seconds, 0 if unknown
        QString err;                  ///< empty if confirmed
        QString memo;
    };
    using SignaturesForAddressCallback =
        std::function<void(Result<std::vector<SignatureRow>>)>;

    /// Result of `simulateTransaction`. `err` is the JSON-RPC `err` field
    /// stringified — empty if simulation succeeded; non-empty (e.g.
    /// "InstructionError" / "InsufficientFundsForFee") if the tx would fail
    /// on-chain. `units_consumed` is informational only.
    ///
    /// Phase 2 plan §2 uses this as a pre-sign MITM gate: a malicious
    /// PumpPortal response that would actually revert (drain attempt with a
    /// failing safety check, malformed instructions, account mismatches) is
    /// caught here before the wallet is asked to sign. Non-trivial token
    /// balance-delta inspection (parsing inner instructions to verify only
    /// SOL/wSOL/$FNCPT change hands) is a Phase 2.5 polish — this gate
    /// catches the structural-error class of attacks.
    struct SimulationResult {
        bool ok = false;          ///< true iff err is empty
        QString err;              ///< stringified JSON-RPC err; empty on success
        QStringList logs;         ///< program log output, for diagnostics
        quint64 units_consumed = 0;
    };
    using SimulateTransactionCallback = std::function<void(Result<SimulationResult>)>;

    /// `getBalance` — returns lamports (1 SOL = 1e9 lamports).
    void get_sol_balance(const QString& pubkey_b58, BalanceCallback callback);

    /// `getTokenAccountsByOwner` filtered by mint, parsed for amount + decimals.
    void get_token_balance(const QString& owner_b58,
                           const QString& mint_b58,
                           TokenBalancesCallback callback);

    /// `getTokenAccountsByOwner` with no mint filter — returns every SPL
    /// token account the wallet owns under the standard SPL Token Program
    /// (TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA). One RPC call per
    /// wallet; cheap. Used by the multi-token holdings path (Phase 2 §2A.5).
    void get_all_token_balances(const QString& owner_b58,
                                TokenBalancesCallback callback);

    /// `getLatestBlockhash` (commitment = `confirmed`).
    /// Used by transaction-building paths to stamp a fresh recent blockhash
    /// before forwarding to the wallet for signing.
    void get_latest_blockhash(LatestBlockhashCallback callback);

    /// `sendTransaction` with a base64-encoded signed transaction.
    /// Returns the transaction signature (base58). The caller is responsible
    /// for polling `get_signature_statuses` to confirm landing.
    void send_transaction(const QString& tx_base64,
                          std::function<void(Result<QString>)> callback);

    /// `getSignatureStatuses` — batched. Order of `out` matches `signatures`.
    void get_signature_statuses(const QStringList& signatures,
                                SignatureStatusesCallback callback);

    /// `getSignaturesForAddress` — paged. `before` is optional (empty = newest).
    /// `limit` is clamped to [1, 1000].
    void get_signatures_for_address(const QString& address_b58,
                                    int limit,
                                    const QString& before_signature,
                                    SignaturesForAddressCallback callback);

    /// `simulateTransaction` — runs the unsigned transaction against the
    /// resolved RPC and returns whether it would succeed. The wallet doesn't
    /// have to sign anything for this; `sigVerify: false` is always set.
    ///
    /// `replace_recent_blockhash` controls how staleness is treated:
    ///   - `true` (default): the simulator swaps in a fresh blockhash before
    ///     running. Use this for the **MITM gate** (Phase 2 §2) — we want to
    ///     know whether the tx logic itself would revert, independent of
    ///     blockhash freshness.
    ///   - `false`: the simulator uses the tx's own embedded blockhash and
    ///     fails with `BlockhashNotFound` if it's expired. Use this for the
    ///     **freshness gate** (Phase 2 §2) immediately before `sign_and_send`,
    ///     so a stale tx that has been sitting in the confirm dialog is
    ///     caught before the wallet pops.
    void simulate_transaction(const QString& tx_base64,
                              SimulateTransactionCallback callback,
                              bool replace_recent_blockhash = true);

    /// Refresh the resolved endpoint from KeyConfigManager. Cheap; safe to call.
    void reload_endpoint();

    /// Currently-resolved HTTP endpoint (for diagnostics / WS-URL derivation).
    QString http_endpoint() const { return endpoint_; }

    /// Derive the WebSocket endpoint from the HTTP endpoint (Helius and the
    /// public mainnet RPC both use the same hostname for `wss://`).
    QString ws_endpoint() const;

  private:
    QString resolve_endpoint() const;
    void post_rpc(const QString& method,
                  const QJsonObject& params,
                  std::function<void(Result<QJsonObject>)> callback);

    QNetworkAccessManager* nam_ = nullptr;
    QString endpoint_;
};

} // namespace fincept::wallet
