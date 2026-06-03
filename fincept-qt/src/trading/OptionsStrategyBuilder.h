#pragma once
// Options Strategy Builder — generates option legs for common multi-leg
// strategies (straddle, strangle, iron condor, vertical spreads) and converts
// them into a BasketOrderRequest for execution.
//
// Spec: docs/OPENALGO_BRIDGE_PHASE2.md §13 and §15.
// OpenAlgo reference: services/flow_executor_service.py (_generate_strategy_legs).

#include "TradingTypes.h"

#include <QString>
#include <QVector>

namespace fincept::trading {

// ----------------------------------------------------------------------------
// A single option leg within a strategy.
// ----------------------------------------------------------------------------
struct OptionsLeg {
    QString symbol;       // e.g. "NIFTY28MAR25500CE" (placeholder — see build_option_symbol)
    QString exchange;     // "NFO" (NSE F&O) / "BFO" (BSE F&O)
    OrderSide side = OrderSide::Buy;
    double quantity = 0;  // already multiplied by lot size
    double strike = 0;
    QString option_type;  // "CE" (call) or "PE" (put)
    QString expiry;       // "2025-03-28"
    QString underlying;   // e.g. "NIFTY" — root symbol, used to resolve the real tradingsymbol
};

// ----------------------------------------------------------------------------
// A complete options strategy: a named bundle of legs plus payoff metrics.
// ----------------------------------------------------------------------------
struct OptionsStrategy {
    QString name;
    QVector<OptionsLeg> legs;
    // Payoff metrics. Computed where they follow directly from strike geometry
    // and lot size (e.g. vertical spreads). Left at 0 where option premium data
    // is required — see notes in OptionsStrategyBuilder.cpp.
    double max_profit = 0;
    double max_loss = 0;
    double breakeven_upper = 0;
    double breakeven_lower = 0;
};

// ----------------------------------------------------------------------------
// Static factory for common option strategies + basket-order conversion.
// ----------------------------------------------------------------------------
class OptionsStrategyBuilder {
public:
    // Long straddle (direction = Buy) or short straddle (direction = Sell):
    // ATM CE + ATM PE, both on the same side.
    static OptionsStrategy straddle(const QString& underlying, const QString& expiry,
                                    double atm_strike, double lot_size,
                                    OrderSide direction);

    // Long strangle (direction = Buy) or short strangle (direction = Sell):
    // OTM CE at (atm + width) + OTM PE at (atm - width), both on the same side.
    static OptionsStrategy strangle(const QString& underlying, const QString& expiry,
                                    double atm_strike, double width, double lot_size,
                                    OrderSide direction);

    // Iron condor (always a net credit, short-vol strategy):
    // Sell near CE (atm + near_width), buy far CE (atm + far_width),
    // Sell near PE (atm - near_width), buy far PE (atm - far_width).
    static OptionsStrategy iron_condor(const QString& underlying, const QString& expiry,
                                       double atm_strike, double near_width, double far_width,
                                       double lot_size);

    // Bull call spread (debit): buy lower-strike CE, sell upper-strike CE.
    static OptionsStrategy bull_call_spread(const QString& underlying, const QString& expiry,
                                            double lower_strike, double upper_strike,
                                            double lot_size);

    // Bear put spread (debit): buy upper-strike PE, sell lower-strike PE.
    static OptionsStrategy bear_put_spread(const QString& underlying, const QString& expiry,
                                           double upper_strike, double lower_strike,
                                           double lot_size);

    // Convert a strategy's legs into a BasketOrderRequest. SELL legs are placed
    // first (collect premium / reduce margin) via std::stable_partition, matching
    // OpenAlgo's options_multiorder_service ordering. Each leg becomes a Market
    // UnifiedOrder with the supplied product type. The strategy name is copied to
    // BasketOrderRequest::strategy_name.
    //
    // When broker_id is supplied and the instrument master is loaded, each leg's
    // tradingsymbol is resolved from the master to the real, broker-tradable
    // symbol. If a leg can't be resolved it falls back to the build_option_symbol()
    // placeholder so behaviour never regresses (a warning is logged).
    static BasketOrderRequest to_basket_order(const OptionsStrategy& strategy,
                                              ProductType product,
                                              const QString& broker_id = {});

    // Build a readable placeholder option symbol of the form
    // "<UNDERLYING><EXPIRY-COMPACT><STRIKE><CE|PE>", e.g. "NIFTY28MAR25500CE".
    // This is a human-readable label for previews and a last-resort fallback —
    // the real, broker-tradable symbol is resolved from the instrument master in
    // to_basket_order() (the path that actually reaches a broker).
    static QString build_option_symbol(const QString& underlying, const QString& expiry,
                                       double strike, const QString& opt_type);
};

} // namespace fincept::trading
