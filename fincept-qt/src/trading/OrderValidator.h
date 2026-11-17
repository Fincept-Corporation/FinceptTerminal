#pragma once
// OrderValidator — pre-flight validation for orders before they reach a broker.
// Mirrors OpenAlgo's restx_api/schemas.py checks: required fields, valid exchange,
// positive quantity, price required for LIMIT, trigger required for SL/SL-M.
//
// Also hosts the settings-driven PRE-TRADE RISK GATE (check_risk / pre_trade_risk).
// It is called from UnifiedTrading's single order funnel (place_order — used by the
// order ticket, broadcast, approvals, MCP tools, workflows, algo deployments and the
// F&O screen) and from the paths that bypass place_order (basket legs, split orders,
// smart orders, GTT approvals). Configured limits are OFF by default; only the
// absurd-order sanity check is on by default.
//
// Where the code lives (the split keeps the rules testable with Qt Core alone):
//   OrderValidator.cpp — PURE: validate*, check_risk, parse/serialize_risk_settings,
//                        build_risk_context. No database, no logger, no globals.
//   OrderRiskGate.cpp  — the shell: settings persistence (SettingsRepository),
//                        logging, the thread-safe per-account snapshot (note_*)
//                        and pre_trade_risk(), which wires them to the pure rules.

#include "trading/TradingTypes.h"

#include <QHash>
#include <QPair>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept::trading {

class OrderValidator {
  public:
    struct ValidationResult {
        bool valid = true;
        QStringList errors;
    };

    static ValidationResult validate(const UnifiedOrder& order);
    static ValidationResult validate_smart(const SmartOrder& order);
    static ValidationResult validate_basket(const BasketOrderRequest& basket);

    static bool is_valid_exchange(const QString& exchange);

    // ── Pre-trade risk gate ─────────────────────────────────────────────────
    // Persisted in the settings table (category "risk"):
    //   risk.max_order_value      notional cap per order (0 = off)
    //   risk.max_order_quantity   quantity cap per order (0 = off)
    //   risk.blocked_symbols      comma list, "SYMBOL" or "EXCHANGE:SYMBOL" (empty = none)
    //   risk.daily_loss_limit     positive amount; new risk blocked once today's
    //                             position P&L <= -limit (0 = off)
    //   risk.sanity_checks        "1" (default) / "0" — absurd-order guard
    //   risk.limits_currency      currency of max_order_value and daily_loss_limit;
    //                             "" (default) = each account's own currency
    struct RiskSettings {
        double max_order_value = 0.0;
        double max_order_quantity = 0.0;
        QStringList blocked_symbols; // upper-cased
        double daily_loss_limit = 0.0;
        bool sanity_checks = true;
        /// ISO code the two money limits are written in ("USD", "INR"); empty = the
        /// account's own currency, so one number applies to every account in that
        /// account's currency (what the limits meant before they had a currency).
        QString limits_currency;
    };

    // ── Currencies ──────────────────────────────────────────────────────────
    // The order value is ref_price x quantity in the INSTRUMENT's currency (as
    // quoted: pence for an LSE share), buying power and day P&L are in the
    // ACCOUNT's currency, the money limits in RiskSettings::limits_currency. Before
    // two of them are compared the order value / P&L is converted with the FX
    // lookup. A conversion that is needed but has no rate is never taken as 1:1:
    // the rule that needed it refuses the order and says which rate is missing.
    // A stale rate (older than the user's "FX stale after") or a stablecoin taken
    // 1:1 for want of a rate is used, and named in any refusal it causes.

    /// One rate as the shell's FX lookup reports it.
    struct FxRate {
        double rate = 0.0;        ///< units of `to` per ONE unit of `from` as quoted (GBp -> USD = GBPUSD / 100)
        bool stale = false;       ///< older than the user's "FX stale after" setting
        bool approximate = false; ///< a stablecoin taken 1:1 to USD because its own rate has not arrived
        QString note;             ///< "1 GBP = 1.3183 USD · Nov 16, 2026 14:32 · stale"; empty = built from the fields
    };
    /// `from` -> `to` (codes as produced, minor units allowed). nullopt = no rate.
    /// Only called for two different codes.
    using FxLookup = std::function<std::optional<FxRate>(const QString& from, const QString& to)>;
    /// How an amount is written in a refusal message; empty = "1,234.50 USD".
    using MoneyText = std::function<QString(double amount, const QString& currency)>;

    // What the gate knows about the account at order time (snapshot, may be partial).
    struct RiskContext {
        bool broker_routed = true;     // false = local paper simulator: configured limits skipped
        double ref_price = 0.0;        // limit / trigger / last price; 0 = unknown
        double buying_power = 0.0;     // available funds; <= 0 = unknown
        bool day_pnl_known = false;
        double day_pnl = 0.0;          // sum of today's position P&L
        bool reduces_position = false; // order only shrinks an existing same-product position

        /// Currency ref_price is quoted in ("GBp", "USD", "USDT"). Empty = unknown,
        /// taken to be the account's currency (documented fallback: a broker that
        /// trades one market quotes in its account currency).
        QString price_currency;
        /// Currency of buying_power and day_pnl. Empty = unknown: nothing is
        /// converted and amounts are compared as they are (the legacy session path).
        QString account_currency;
        /// Currency the broker reported its funds in (buying_power), when it said;
        /// empty = account_currency.
        QString funds_currency;
        /// Today's P&L split by the currency each position reports it in (key ""
        /// = account_currency). Empty = all of day_pnl is in account_currency.
        /// Each part is converted on its own before the sum is compared with the
        /// daily loss limit — P&L in different currencies is never added raw.
        QHash<QString, double> day_pnl_by_currency;
        /// Rates for the conversions above. Empty = no rates: a needed conversion
        /// fails closed exactly like a missing rate.
        FxLookup fx;
        /// Amount formatter for messages (the app passes the user's money format).
        MoneyText money_text;
    };

    // Absurd-order guard (on by default): a quantity beyond any sane order size,
    // or a CASH-segment notional more than this multiple of available buying power
    // (no retail product leverages cash equity beyond ~5x, so >10x is a typo such as
    // an extra zero). Derivative segments are exempt from the buying-power test —
    // their notional/margin ratio is legitimately 10–50x — and rely on the exchange
    // freeze-quantity check in UnifiedTrading::place_order instead. The notional is
    // converted into the account's currency first (a pence or yen order against a
    // USD account was off by 100x+ before).
    static constexpr double kSanityMaxQuantity = 10'000'000.0;
    static constexpr double kSanityBuyingPowerMultiple = 10.0;

    /// PURE: evaluate one order against settings + context. Position-reducing orders
    /// are exempt from the configured limits (they lower risk); the sanity quantity
    /// ceiling applies to everything. A quantity that is not a finite number, or a
    /// day P&L that is not a finite number while a daily loss limit is set, is
    /// refused (fail closed) — NaN compares false against every limit.
    static ValidationResult check_risk(const UnifiedOrder& order, const RiskSettings& settings,
                                       const RiskContext& ctx);

    /// What the gate knows about one account (mirrored from the GUI-thread stream
    /// caches by note_funds / note_positions / note_quote).
    struct RiskSnapshot {
        double available = 0.0;
        bool funds_known = false;
        QString funds_currency; ///< currency of `available` as the broker reported it; empty = the account's
        QVector<BrokerPosition> positions;
        /// Parallel to `positions`: the currency each row's P&L is in (from what the
        /// adapter reports, e.g. the row's venue); an empty or missing entry = the
        /// funds' currency, else the account's (the documented fallback).
        QStringList position_currencies;
        bool positions_known = false;
        QHash<QString, double> ltp; // symbol (upper-cased) -> last price
    };

    /// PURE: the RiskContext for `order` from an account snapshot (nullptr = nothing
    /// known about the account). Reference price: LIMIT / SL-LIMIT price, else the
    /// SL trigger, else order.price, else the snapshot LTP, else the position LTP.
    /// The order "reduces" only when it is the closing side of an open position in
    /// the SAME symbol, exchange (when both are known) and product, and is no larger
    /// than that position. funds_currency is copied; day P&L is also split by each
    /// position's currency (day_pnl_by_currency) when any row reports one.
    static RiskContext build_risk_context(const UnifiedOrder& order, bool broker_routed,
                                          const RiskSnapshot* snapshot);

    /// PURE: settings-table rows ("risk.*" key -> stored text) <-> RiskSettings.
    /// Unknown keys are ignored; unparsable / non-finite / negative caps read as 0
    /// (off); the daily loss limit is stored as a positive amount; a limits
    /// currency that is not 3-5 letters reads as "" (each account's own).
    static RiskSettings parse_risk_settings(const QVector<QPair<QString, QString>>& rows);
    static QVector<QPair<QString, QString>> serialize_risk_settings(const RiskSettings& s);

    /// Settings I/O. load fails CLOSED: `ok_out` false when the settings could not be
    /// read, and pre_trade_risk() then refuses the order.
    static RiskSettings load_risk_settings(bool* ok_out = nullptr);
    static bool save_risk_settings(const RiskSettings& s);

    /// Full gate for one order on one account: loads settings, builds the context
    /// from the thread-safe snapshot below, adds the currencies (instrument: the
    /// order exchange's venue currency, a crypto pair's quote asset; account: the
    /// paper portfolio's for the local simulator, else the broker's) and the FX
    /// service's rates, runs check_risk(). Safe on any thread.
    static ValidationResult pre_trade_risk(const QString& account_id, const UnifiedOrder& order, bool broker_routed);

    /// True for derivative / commodity / currency segments (NFO, BFO, MCX, CDS, …).
    static bool is_derivative_exchange(const QString& exchange);

    // ── Thread-safe account snapshot (fed on the GUI thread by DataStreamManager) ──
    // The two-argument forms leave the currency unknown (= the account's).
    static void note_funds(const QString& account_id, double available_balance);
    /// `currency`: what the broker reported the funds in; empty = the account's.
    static void note_funds(const QString& account_id, double available_balance, const QString& currency);
    static void note_positions(const QString& account_id, const QVector<BrokerPosition>& positions);
    /// `pnl_currencies`: parallel to `positions`, the currency of each row's P&L
    /// (empty entries = the funds' / account's currency).
    static void note_positions(const QString& account_id, const QVector<BrokerPosition>& positions,
                               const QStringList& pnl_currencies);
    static void note_quote(const QString& account_id, const QString& symbol, double ltp);

  private:
    static bool requires_price(OrderType type);
    static bool requires_trigger_price(OrderType type);
    static const QSet<QString>& valid_exchanges();
};

} // namespace fincept::trading
