// src/algo_engine/ExecutionModel.h
#pragma once
// ExecutionModel — the cost and fill model shared by the backtester and the
// paper simulator, so a paper deployment and its backtest charge the same
// commission, taxes and slippage and size orders with the same lot rules.
//
// Pure computation, Qt Core only (unit-tested in tests/tst_algo_execution_model.cpp).
//
// Units
//   *_pct fields are PERCENT of traded value (0.03 = 0.03 %), slippage_bps is
//   basis points of the reference price, money fields are in the instrument's
//   quote currency. A Fill's price already includes slippage; its fees are on
//   top of price × quantity.
//
// Slippage is ADVERSE: a buy fills above the reference price, a sell below.
// Limit-style fills (take-profit) pass atr = 0 and slippage = false.
//
// Presets are APPROXIMATIONS of typical retail schedules per market, meant as
// editable starting points — they are labelled as such in the UI. They are not
// a broker's actual fee schedule.

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::algo {

enum class FillSide { Buy, Sell };

struct CostModel {
    QString preset = QStringLiteral("none"); ///< id of the preset it started from ("custom" once edited)

    // Commission per order
    double commission_pct = 0;       ///< % of traded value
    double commission_per_unit = 0;  ///< per share / contract
    double commission_per_order = 0; ///< flat per order
    double commission_min = 0;       ///< minimum per order (only when a commission is charged)
    double commission_max = 0;       ///< cap per order, 0 = none

    // Statutory charges (transaction taxes, stamp duty, exchange / regulatory fees)
    double buy_tax_pct = 0;  ///< % of value on buys
    double sell_tax_pct = 0; ///< % of value on sells

    // Slippage (adverse)
    double slippage_bps = 0;      ///< bps of the reference price
    double slippage_atr_frac = 0; ///< fraction of the bar ATR (0.05 = 5 % of ATR)

    // Quantity rules
    double lot_size = 1; ///< orders are multiples of this; < 1 allows fractional units (0.0001)

    // Shorting
    double borrow_pct_annual = 0; ///< stock-borrow fee on short notional, % per year (calendar days)

    /// True when every charge and slippage term is zero (a GROSS backtest).
    bool is_zero() const;
    QJsonObject to_json() const;
    static CostModel from_json(const QJsonObject& o);
    bool operator==(const CostModel& o) const;
    bool operator!=(const CostModel& o) const { return !(*this == o); }
};

struct FeeBreakdown {
    double commission = 0;
    double taxes = 0;
    double total() const { return commission + taxes; }
};

struct Fill {
    FillSide side = FillSide::Buy;
    double ref_price = 0;     ///< price before slippage (bar open, stop level, tick)
    double price = 0;         ///< executed price incl. slippage
    double qty = 0;
    double notional = 0;      ///< price × qty
    FeeBreakdown fees;
    double slippage_cost = 0; ///< |price - ref_price| × qty
    bool valid() const { return qty > 0 && price > 0; }
};

struct CostPreset {
    QString id;
    QString label;  ///< translated
    QString note;   ///< translated: what the approximation includes
    QString region; ///< MarketSessions country / pseudo-venue the preset fits ("US", "IN", "CRYPTO", ...)
    CostModel model;
};

namespace execution {

/// Reference price moved adversely by bps and ATR fraction. Never below 1e-9.
double slipped_price(const CostModel& m, FillSide side, double ref_price, double atr);

/// Commission + taxes of one order.
FeeBreakdown fees(const CostModel& m, FillSide side, double qty, double price);

/// `qty` floored to the lot / step (tolerant of floating error: 2.9999999 lots -> 3).
double round_quantity(const CostModel& m, double qty);

/// Largest lot-rounded quantity whose price × qty + fees fits in `cash`
/// (0 when not even one lot fits).
double max_affordable_qty(const CostModel& m, FillSide side, double cash, double price);

/// A complete fill. `apply_slippage` = false for limit-style fills.
Fill fill(const CostModel& m, FillSide side, double ref_price, double qty, double atr, bool apply_slippage = true);

/// Borrow fee for holding a short of `notional` for `days` calendar days.
double borrow_cost(const CostModel& m, double notional, double days);

/// Built-in presets ("none" first). Labels are translated at call time.
QVector<CostPreset> presets();
/// Preset by id; the "none" preset for an unknown id.
CostPreset preset(const QString& id);
/// Preset that best fits a MarketSessions venue id / country ("NSE", "IN", "LSE",
/// "CRYPTO", ...); "none" when there is no approximation for it.
QString default_preset_for(const QString& venue_or_country, bool intraday);

} // namespace execution
} // namespace fincept::algo
