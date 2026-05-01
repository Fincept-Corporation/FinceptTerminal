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

// ── Phase 5 types (ROADMAP tab — buyback & burn dashboard) ──────────────
//
// All five payload types are **terminal-wide** (no <pubkey> segment) — the
// same numbers are shown to every user regardless of who's connected. The
// `BuybackBurnService` and `TreasuryService` producers fetch from the
// Fincept-operated buyback worker (`services/buyback-worker/`) which
// publishes per-epoch summaries to an HTTP endpoint configured via
// SecureStorage (`fincept.treasury_endpoint`). When the endpoint is absent
// the producers return a built-in mock payload so the UI is reviewable
// before the worker is deployed.

/// One epoch's buyback summary. Published on `treasury:buyback_epoch`.
/// "Epoch" is whatever cadence the worker chooses — Phase 5 plan §5.4
/// suggests weekly. `start_ts_ms` / `end_ts_ms` are unix ms; `revenue_*`
/// fields are USD; `fncpt_*_raw` are token-units (atomic, not decimal-shifted).
///
/// Plan §5.4 specifies a **50/25/25 budget split** (buyback / staker yield /
/// treasury top-up). All three USD splits are reported here so the dashboard
/// can show the full value loop, not just the headline buyback number.
/// The split percentages aren't enforced — the worker can choose any
/// distribution; the dashboard just displays whatever the worker reports.
struct BuybackEpoch {
    int     epoch_no = 0;
    qint64  start_ts_ms = 0;
    qint64  end_ts_ms = 0;
    double  revenue_total_usd = 0.0;        ///< end-of-epoch tally
    double  revenue_subs_usd = 0.0;         ///< subscriptions
    double  revenue_predmkt_usd = 0.0;      ///< prediction-market fees
    double  revenue_misc_usd = 0.0;         ///< everything else
    double  buyback_usd = 0.0;              ///< actually spent on buying $FNCPT (50%)
    double  staker_yield_usd = 0.0;         ///< paid out to veFNCPT lockers (25%)
    double  treasury_topup_usd = 0.0;       ///< retained as USDC in treasury (25%)
    QString fncpt_bought_raw;               ///< integer string (atomic)
    QString fncpt_burned_raw;               ///< usually == bought; may diverge briefly
    int     fncpt_decimals = 6;             ///< pump.fun standard
    double  avg_buy_price_usd = 0.0;        ///< buyback_usd / fncpt_bought_ui
    QString burn_signature;                 ///< base58 tx sig of the burn (Solscan-clickable)
    qint64  ts_ms = 0;
    bool    is_mock = false;                ///< true if the worker endpoint isn't configured
};

/// All-time totals. Published on `treasury:burn_total`.
struct BurnTotal {
    QString total_burned_raw;               ///< integer string (atomic)
    QString supply_remaining_raw;           ///< current circulating + uncirculated, minus burned
    int     decimals = 6;
    double  spent_on_buyback_usd = 0.0;
    qint64  ts_ms = 0;
    bool    is_mock = false;
};

/// One sample of supply over time. The supply chart subscribes
/// `treasury:supply_history` which carries a `QVector<SupplyHistoryPoint>`.
struct SupplyHistoryPoint {
    qint64  ts_ms = 0;                      ///< sample time (unix ms, weekly bucket)
    QString total_raw;                      ///< total supply at sample time (atomic)
    QString circulating_raw;                ///< circulating only (excludes vesting/locked)
    QString burned_raw;                     ///< cumulative burned to date
    int     decimals = 6;
};

/// Treasury reserves snapshot. Published on `treasury:reserves`.
struct TreasuryReserves {
    QString pubkey_b58;                     ///< treasury multisig pubkey (Squads vault)
    quint64 sol_lamports = 0;               ///< native SOL
    double  usdc_amount = 0.0;              ///< already decimal-shifted (UI-ready)
    double  sol_usd_price = 0.0;            ///< spot at sample time
    double  total_usd = 0.0;                ///< (sol_lamports/1e9 * sol_usd_price) + usdc_amount
    QString multisig_label;                 ///< "3-of-5" / "2-of-3" — for the chip
    QString multisig_url;                   ///< deeplink (squads.so/...) — for the chip click
    qint64  ts_ms = 0;
    bool    is_mock = false;
};

/// Treasury runway. Published on `treasury:runway`.
/// Months = `total_usd / monthly_opex_usd`. `monthly_opex_usd` comes from
/// SecureStorage (`fincept.treasury_monthly_opex_usd`) until the worker
/// publishes a tracked figure.
struct TreasuryRunway {
    double  total_usd = 0.0;                ///< snapshot from TreasuryReserves
    double  monthly_opex_usd = 0.0;         ///< current burn rate
    double  months = 0.0;                   ///< total_usd / monthly_opex_usd
    qint64  ts_ms = 0;
    bool    is_mock = false;
};

// ── Phase 3 types (STAKE tab — veFNCPT lock + tier system) ──────────────
//
// veFNCPT model (Curve veCRV-style, sized down): users lock $FNCPT for
// 3 months / 6 mo / 1 yr / 2 yr / 4 yr. Weight = amount × duration_multiplier
// where the multipliers are 0.0625 / 0.125 / 0.25 / 0.5 / 1.0 of a 4-yr
// max-weight lock. Yield: real-yield USDC distributions pro-rata by weight,
// settled per epoch by an admin-signed `emit_distribution` instruction.
// Plan §3.5 deciding factor: ship a minimal `fincept_lock` Anchor program
// in `solana/programs/fincept_lock/` (out-of-tree, separate repo).
//
// The terminal-side surface ships in mock mode (`is_mock=true` on every
// payload) until the Anchor program is deployed and `fincept.lock_program_id`
// is configured in SecureStorage.

/// One lock position. Published as part of `QVector<LockPosition>` on
/// `wallet:locks:<pubkey>`. A position is a non-transferable NFT under the
/// fincept_lock program; `position_id` is the on-chain account address
/// (base58) that uniquely identifies it.
struct LockPosition {
    QString position_id;                    ///< base58 PDA / position NFT mint
    QString amount_raw;                     ///< $FNCPT escrowed (atomic)
    int     decimals = 6;                   ///< $FNCPT decimals
    qint64  lock_start_ts = 0;              ///< unix ms — when the lock was created
    qint64  unlock_ts = 0;                  ///< unix ms — `lock_start + duration`
    qint64  duration_secs = 0;              ///< original lock duration
    QString weight_raw;                     ///< veFNCPT weight (atomic, decimals match $FNCPT)
    double  lifetime_yield_usdc = 0.0;      ///< total USDC paid to this position
    bool    is_mock = false;
};

/// Aggregate veFNCPT view per pubkey. Published on `wallet:vefncpt:<pubkey>`.
/// Sum across the user's `LockPosition`s, plus a forward-looking projection
/// for the next epoch's distribution.
struct VeFncptAggregate {
    QString pubkey_b58;
    QString total_weight_raw;               ///< sum of `LockPosition.weight_raw`
    int     decimals = 6;
    int     position_count = 0;
    double  projected_next_period_yield_usdc = 0.0; ///< weight share × next-epoch revenue × 25%
    qint64  ts_ms = 0;
    bool    is_mock = false;
};

/// Yield earned by a wallet. Published on `wallet:yield:<pubkey>`.
/// Decoupled from `VeFncptAggregate` because yield realised vs projected
/// have very different semantics — projection is forward-looking math,
/// realised is a sum of past on-chain distribution events.
struct YieldSnapshot {
    QString pubkey_b58;
    double  lifetime_usdc = 0.0;            ///< all-time
    double  last_period_usdc = 0.0;         ///< most recent epoch distribution
    qint64  last_period_end_ts = 0;         ///< unix ms — settlement time
    qint64  ts_ms = 0;
    bool    is_mock = false;
};

/// Terminal-wide weekly revenue aggregate. Published on `treasury:revenue`.
/// Used by `LockPanel` to compute "EST. YIELD" before the user locks; also
/// consumed by the Phase 5 buyback dashboard. Bucketed weekly to match
/// the buyback worker's epoch cadence.
struct TreasuryRevenue {
    qint64  period_start_ts = 0;
    qint64  period_end_ts = 0;
    double  total_usd = 0.0;
    qint64  ts_ms = 0;
    bool    is_mock = false;
};

/// Billing tier derived from a wallet's veFNCPT weight. Published on
/// `billing:tier:<pubkey>` by `TierService` (consumer-producer pattern,
/// like `FeeDiscountService`).
///
/// Tier thresholds live in `services/billing/TierConfig.h`. Plan §3.6:
///   Bronze   100+ veFNCPT  →  basic API quota
///   Silver   1k+  veFNCPT  →  premium screens
///   Gold     10k+ veFNCPT  →  all agents + arena
///
/// `Free` is the implicit zero-stake tier — no veFNCPT held. Drives the
/// nav-level gating: paid screens are hidden for Free, revealed for
/// Silver+ via a `tier_changed` signal.
struct TierStatus {
    enum class Tier {
        Free,    ///< no veFNCPT
        Bronze,  ///< 100+
        Silver,  ///< 1k+
        Gold     ///< 10k+
    };

    QString pubkey_b58;
    Tier    tier = Tier::Free;
    QString weight_raw;                     ///< current veFNCPT weight (atomic)
    QString next_threshold_raw;             ///< raw amount needed to reach next tier; empty at Gold
    int     decimals = 6;
    qint64  ts_ms = 0;
    bool    is_mock = false;
};

} // namespace fincept::wallet

Q_DECLARE_METATYPE(fincept::wallet::WalletBalance)
Q_DECLARE_METATYPE(fincept::wallet::TokenHolding)
Q_DECLARE_METATYPE(fincept::wallet::TokenPrice)
Q_DECLARE_METATYPE(fincept::wallet::TokenMetadata)
Q_DECLARE_METATYPE(fincept::wallet::ParsedActivity)
Q_DECLARE_METATYPE(QVector<fincept::wallet::ParsedActivity>)
Q_DECLARE_METATYPE(fincept::wallet::FncptDiscount)
Q_DECLARE_METATYPE(fincept::wallet::BuybackEpoch)
Q_DECLARE_METATYPE(fincept::wallet::BurnTotal)
Q_DECLARE_METATYPE(fincept::wallet::SupplyHistoryPoint)
Q_DECLARE_METATYPE(QVector<fincept::wallet::SupplyHistoryPoint>)
Q_DECLARE_METATYPE(fincept::wallet::TreasuryReserves)
Q_DECLARE_METATYPE(fincept::wallet::TreasuryRunway)
Q_DECLARE_METATYPE(fincept::wallet::LockPosition)
Q_DECLARE_METATYPE(QVector<fincept::wallet::LockPosition>)
Q_DECLARE_METATYPE(fincept::wallet::VeFncptAggregate)
Q_DECLARE_METATYPE(fincept::wallet::YieldSnapshot)
Q_DECLARE_METATYPE(fincept::wallet::TreasuryRevenue)
Q_DECLARE_METATYPE(fincept::wallet::TierStatus)
// FncptPrice is `TokenPrice`; no separate metatype needed.
