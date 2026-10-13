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

#include "trading/TradingTypes.h"

#include <QSet>
#include <QStringList>
#include <QVector>

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
    struct RiskSettings {
        double max_order_value = 0.0;
        double max_order_quantity = 0.0;
        QStringList blocked_symbols; // upper-cased
        double daily_loss_limit = 0.0;
        bool sanity_checks = true;
    };

    // What the gate knows about the account at order time (snapshot, may be partial).
    struct RiskContext {
        bool broker_routed = true;     // false = local paper simulator: configured limits skipped
        double ref_price = 0.0;        // limit / trigger / last price; 0 = unknown
        double buying_power = 0.0;     // available funds; <= 0 = unknown
        bool day_pnl_known = false;
        double day_pnl = 0.0;          // sum of today's position P&L
        bool reduces_position = false; // order only shrinks an existing same-product position
    };

    // Absurd-order guard (on by default): a quantity beyond any sane order size,
    // or a CASH-segment notional more than this multiple of available buying power
    // (no retail product leverages cash equity beyond ~5x, so >10x is a typo such as
    // an extra zero). Derivative segments are exempt from the buying-power test —
    // their notional/margin ratio is legitimately 10–50x — and rely on the exchange
    // freeze-quantity check in UnifiedTrading::place_order instead.
    static constexpr double kSanityMaxQuantity = 10'000'000.0;
    static constexpr double kSanityBuyingPowerMultiple = 10.0;

    /// PURE: evaluate one order against settings + context. Position-reducing orders
    /// are exempt from the configured limits (they lower risk); the sanity quantity
    /// ceiling applies to everything.
    static ValidationResult check_risk(const UnifiedOrder& order, const RiskSettings& settings,
                                       const RiskContext& ctx);

    /// Settings I/O. load fails CLOSED: `ok_out` false when the settings could not be
    /// read, and pre_trade_risk() then refuses the order.
    static RiskSettings load_risk_settings(bool* ok_out = nullptr);
    static bool save_risk_settings(const RiskSettings& s);

    /// Full gate for one order on one account: loads settings, builds the context
    /// from the thread-safe snapshot below, runs check_risk(). Safe on any thread.
    static ValidationResult pre_trade_risk(const QString& account_id, const UnifiedOrder& order, bool broker_routed);

    /// True for derivative / commodity / currency segments (NFO, BFO, MCX, CDS, …).
    static bool is_derivative_exchange(const QString& exchange);

    // ── Thread-safe account snapshot (fed on the GUI thread by DataStreamManager) ──
    static void note_funds(const QString& account_id, double available_balance);
    static void note_positions(const QString& account_id, const QVector<BrokerPosition>& positions);
    static void note_quote(const QString& account_id, const QString& symbol, double ltp);

  private:
    static bool requires_price(OrderType type);
    static bool requires_trigger_price(OrderType type);
    static const QSet<QString>& valid_exchanges();
};

} // namespace fincept::trading
