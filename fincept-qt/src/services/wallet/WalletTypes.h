#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::wallet {

/// $FNCPT canonical mint. Used by the back-compat accessors on `WalletBalance`
/// and by the price-topic alias mapping.
inline constexpr const char* kFncptMint =
    "9LUqJ5aQTjQiUCL93gi33LZcscUoSBJNhVCYpPzEpump";

/// Wrapped-SOL mint. SOL itself isn't an SPL token (it's the native asset),
/// but most pricing APIs key it by the wSOL mint.
inline constexpr const char* kWrappedSolMint =
    "So11111111111111111111111111111111111111112";

/// Display metadata for a single SPL token. Provided by `TokenMetadataService`
/// (2A.5.2). Cached daily; persisted to SecureStorage between runs.
struct TokenMetadata {
    QString mint;
    QString symbol;
    QString name;
    int     decimals = 0;
    QString icon_url;
    bool    verified = false; ///< present in Jupiter's verified tagged list
};

/// One asset held by a wallet — SOL or any SPL token. SOL is represented
/// as a `TokenHolding` with `mint = kWrappedSolMint` and `symbol = "SOL"`
/// inside `WalletBalance::tokens`, with the actual lamports also surfaced
/// via the dedicated `sol_lamports` field for callers that want native SOL
/// distinct from a wrapped-SOL holding.
struct TokenHolding {
    QString mint;          ///< base58 SPL mint
    QString symbol;        ///< resolved via TokenMetadataService; truncated mint if unknown
    QString name;          ///< full name; "" if unknown
    QString amount_raw;    ///< integer string (atomic, not decimal-shifted)
    int     decimals = 0;  ///< from on-chain parsed token-account info
    bool    verified = false;
    QString icon_url;      ///< empty if unknown

    /// `amount_raw / 10^decimals`. 0.0 if amount_raw is empty / unparseable.
    double ui_amount() const noexcept;
};

/// One snapshot of a wallet's balances. Published to `wallet:balance:<pubkey>`.
///
/// Shape upgraded in Phase 2 Stage 2A.5: `tokens` carries every SPL token
/// account the wallet holds, sorted by USD value at publish time. The legacy
/// `fncpt_*` fields are gone; existing callers should use the back-compat
/// accessors `fncpt_holding()` / `fncpt_ui()` / `fncpt_decimals()`.
struct WalletBalance {
    QString               pubkey_b58;
    quint64               sol_lamports = 0;
    QVector<TokenHolding> tokens; ///< sorted by USD value desc at publish time
    qint64                ts_ms = 0;

    double sol() const noexcept { return static_cast<double>(sol_lamports) / 1e9; }

    /// Find the FNCPT entry in `tokens`, or nullptr if the wallet doesn't
    /// hold any FNCPT.
    const TokenHolding* fncpt_holding() const noexcept;

    /// Convenience: 0.0 if not held.
    double fncpt_ui() const noexcept;

    /// Convenience: returns the on-chain decimals if FNCPT is held; otherwise
    /// the canonical FNCPT decimals (6, pump.fun standard).
    int    fncpt_decimals() const noexcept;
};

/// Price snapshot for a single token mint. Published to
/// `market:price:token:<mint>` (and aliased to `market:price:fncpt` for the
/// canonical FNCPT mint, deprecated for one phase).
struct TokenPrice {
    QString mint;
    double  usd = 0.0;
    double  sol = 0.0;
    qint64  ts_ms = 0;
    bool    valid = false;
};

/// Back-compat alias. Existing call sites that read `FncptPrice` continue to
/// compile against the new `TokenPrice` shape — same fields, same semantics.
/// Removed in Phase 3 cleanup once all consumers migrate.
using FncptPrice = TokenPrice;

/// Fee-discount eligibility for a wallet. Published on
/// `billing:fncpt_discount:<pubkey>` (Phase 2 §2C).
///
/// Derived state: `FeeDiscountService` subscribes to `wallet:balance:<pk>`
/// and republishes a `FncptDiscount` whenever the held FNCPT amount crosses
/// the threshold (or the balance topic emits anew at the same eligibility
/// state — consumers re-render either way; cheap).
struct FncptDiscount {
    bool        eligible = false;
    quint64     threshold_raw = 0;     ///< from FeeDiscountConfig::kThresholdRaw
    int         threshold_decimals = 6;
    int         discount_pct = 0;
    QStringList applied_skus;          ///< stable SKU ids; panel maps to labels
    QString     pubkey_b58;            ///< for diagnostics; the topic also encodes it
    qint64      ts_ms = 0;
};

/// One parsed on-chain operation involving the wallet. Published as part of
/// `QVector<ParsedActivity>` on `wallet:activity:<pubkey>` (Phase 2 §2B).
///
/// Source priority:
///   - Helius parsed-tx (`api.helius.xyz/v0/addresses/<pk>/transactions`)
///     when `solana.helius_api_key` is set in SecureStorage. Helius classifies
///     SWAP / RECEIVE / SEND directly.
///   - Fallback: `SolanaRpcClient::get_signatures_for_address` returns raw
///     signatures only; `kind` defaults to `Other` and `asset`/`amount_ui`
///     are empty in that case.
struct ParsedActivity {
    enum class Kind {
        Swap,     ///< token-for-token (or SOL) swap on a DEX
        Receive,  ///< incoming transfer
        Send,     ///< outgoing transfer
        Other     ///< Helius didn't classify, or fallback path
    };

    qint64  ts_ms = 0;         ///< unix ms; 0 if Helius didn't include block time
    Kind    kind = Kind::Other;
    QString asset;             ///< e.g. "SOL → $FNCPT" or just "SOL"
    QString amount_ui;         ///< pre-formatted amount string, e.g. "0.50 → 12,304"
    QString signature;         ///< base58 tx signature (clickable to Solscan)
    QString status;            ///< "confirmed" / "failed" / "" if unknown
    QString counterparty;      ///< for SEND/RECEIVE; empty otherwise
};

} // namespace fincept::wallet

Q_DECLARE_METATYPE(fincept::wallet::WalletBalance)
Q_DECLARE_METATYPE(fincept::wallet::TokenHolding)
Q_DECLARE_METATYPE(fincept::wallet::TokenPrice)
Q_DECLARE_METATYPE(fincept::wallet::TokenMetadata)
Q_DECLARE_METATYPE(fincept::wallet::ParsedActivity)
Q_DECLARE_METATYPE(QVector<fincept::wallet::ParsedActivity>)
Q_DECLARE_METATYPE(fincept::wallet::FncptDiscount)
// FncptPrice is `TokenPrice`; no separate metatype needed.
