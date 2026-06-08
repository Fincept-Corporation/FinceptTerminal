// src/algo_engine/AlgoEngineTypes.h
#pragma once
#include <QHash>
#include <QMetaType>
#include <QString>
#include <QVector>

#include <cstdint>
#include <optional>

namespace fincept::algo {

// ── OHLCV Candle ────────────────────────────────────────────────────────────

struct OhlcvCandle {
    int64_t open_time = 0;
    int64_t close_time = 0;
    double open = 0;
    double high = 0;
    double low = 0;
    double close = 0;
    double volume = 0;
    bool is_closed = false;
};

// ── Timeframe ───────────────────────────────────────────────────────────────

enum class Timeframe { M1, M3, M5, M15, M30, H1, H4, D1 };

inline int timeframe_seconds(Timeframe tf) {
    switch (tf) {
    case Timeframe::M1:  return 60;
    case Timeframe::M3:  return 180;
    case Timeframe::M5:  return 300;
    case Timeframe::M15: return 900;
    case Timeframe::M30: return 1800;
    case Timeframe::H1:  return 3600;
    case Timeframe::H4:  return 14400;
    case Timeframe::D1:  return 86400;
    }
    return 60;
}

inline Timeframe timeframe_from_string(const QString& s) {
    if (s == "1m")  return Timeframe::M1;
    if (s == "3m")  return Timeframe::M3;
    if (s == "5m")  return Timeframe::M5;
    if (s == "15m") return Timeframe::M15;
    if (s == "30m") return Timeframe::M30;
    if (s == "1h")  return Timeframe::H1;
    if (s == "4h")  return Timeframe::H4;
    if (s == "1d")  return Timeframe::D1;
    return Timeframe::M5;
}

inline QString timeframe_to_string(Timeframe tf) {
    switch (tf) {
    case Timeframe::M1:  return QStringLiteral("1m");
    case Timeframe::M3:  return QStringLiteral("3m");
    case Timeframe::M5:  return QStringLiteral("5m");
    case Timeframe::M15: return QStringLiteral("15m");
    case Timeframe::M30: return QStringLiteral("30m");
    case Timeframe::H1:  return QStringLiteral("1h");
    case Timeframe::H4:  return QStringLiteral("4h");
    case Timeframe::D1:  return QStringLiteral("1d");
    }
    return QStringLiteral("5m");
}

// ── Indicator result ────────────────────────────────────────────────────────

struct IndicatorResult {
    QHash<QString, double> current;
    QHash<QString, double> previous;
    bool valid = false;
    QString error;
};

// ── Condition evaluation ────────────────────────────────────────────────────

struct ConditionResult {
    bool met = false;
    QString indicator;
    QString field;
    double computed_value = 0;
    double target_value = 0;
    QString op;
    QString error;
};

struct GroupEvalResult {
    bool triggered = false;
    QVector<ConditionResult> details;
    QString logic;
};

// ── Position ────────────────────────────────────────────────────────────────

enum class PositionSide { None, Long, Short };

inline QString position_side_to_string(PositionSide s) {
    switch (s) {
    case PositionSide::Long:  return QStringLiteral("LONG");
    case PositionSide::Short: return QStringLiteral("SHORT");
    default: return QStringLiteral("NONE");
    }
}

struct AlgoPosition {
    PositionSide side = PositionSide::None;
    double quantity = 0;
    double entry_price = 0;
    int64_t entry_time = 0;
    double unrealized_pnl = 0;
    double highest_since_entry = 0;
    double lowest_since_entry = 0;
};

// One leg of an open multi-leg F&O basket position (P3).
struct AlgoLegPosition {
    QString symbol;            // broker-native option symbol
    qint64  instrument_token = 0;
    bool    is_call = true;
    double  strike = 0;
    int     side_sign = 1;     // +1 long/BUY, -1 short/SELL
    double  quantity = 0;      // contracts (lots × lot_size)
    double  entry_price = 0;   // premium per contract at entry
    double  current_price = 0; // live mark
    double  unrealized_pnl = 0;
};

// ── Risk ────────────────────────────────────────────────────────────────────

struct RiskState {
    double daily_pnl = 0;
    double max_drawdown = 0;
    double peak_equity = 0;
    bool paused_by_loss_limit = false;
    int64_t day_start_epoch = 0;
};

// ── Trade record ────────────────────────────────────────────────────────────

struct AlgoTradeRecord {
    QString id;
    QString deployment_id;
    QString symbol;
    QString side;
    double quantity = 0;
    double price = 0;
    double pnl = 0;
    QString reason;
    int64_t timestamp = 0;
    QString broker_order_id;
    int64_t latency_ms = 0;
    // Multi-leg F&O basket fills (P3.4): one trade row per leg. leg_symbol is the
    // broker-native contract; leg_index is its position in the basket. Empty/-1 for
    // the single-symbol equity path.
    QString leg_symbol;
    int     leg_index = -1;
};

// ── Metrics ─────────────────────────────────────────────────────────────────

struct AlgoMetrics {
    double total_pnl = 0;
    double unrealized_pnl = 0;
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
    double win_rate = 0;
    double max_drawdown = 0;
    double current_price = 0;
    double current_position_qty = 0;
    QString current_position_side;
    double current_position_entry = 0;
    int64_t last_signal_time = 0;
    int64_t last_trade_time = 0;
};

// ── Order signal ────────────────────────────────────────────────────────────

// One leg of a multi-leg F&O order. Empty AlgoOrderSignal.legs means the
// single-symbol equity path (symbol/quantity/side fields) is used instead.
struct AlgoOrderLeg {
    QString symbol;             // broker-native option/future symbol
    qint64  instrument_token = 0;
    QString side;               // BUY | SELL
    double  quantity = 0;
    double  price = 0;          // limit price; 0 = market
};

struct AlgoOrderSignal {
    QString deployment_id;
    QString account_id;
    QString symbol;
    QString exchange;
    QString product_type;
    QString side;
    double quantity = 0;
    QString order_type = "MARKET";
    double price = 0;
    double trigger_price = 0;
    QString reason;
    QString mode = "paper"; // paper | live — paper simulates the fill, live routes to the broker
    QString paper_portfolio_id; // PtPortfolio id for the paper basket path (stamped by the runner)
    QVector<AlgoOrderLeg> legs; // multi-leg F&O orders; empty = single-symbol equity path
};

// ── Live dashboard snapshot ───────────────────────────────────────────────────
// Pushed from the runner to the Dashboard on every tick (throttled) so the UI can
// show real-time price, P&L, position, and per-condition status without re-reading
// the DB. Each AlgoConditionStatus is one entry/exit leaf with its live computed
// value vs its target, so the user can see exactly why a rule is or isn't firing.

struct AlgoConditionStatus {
    QString label;       // human label, e.g. "CLOSE crosses above 1205"
    QString op;          // raw operator (>, crosses_above, …) — UI derives hints from it
    QString section;     // "entry" | "exit"
    double computed = 0; // current value of the LHS operand
    double target = 0;   // the threshold / RHS value
    bool met = false;    // did this leaf evaluate true this tick
};

struct AlgoLiveSnapshot {
    QString deployment_id;
    double current_price = 0;
    int64_t last_update_ms = 0;
    AlgoMetrics metrics;
    QVector<AlgoConditionStatus> conditions; // entry rows then exit rows
    QString note;                            // short activity line, e.g. "entry not met"
};

} // namespace fincept::algo

Q_DECLARE_METATYPE(fincept::algo::AlgoMetrics)
Q_DECLARE_METATYPE(fincept::algo::AlgoTradeRecord)
Q_DECLARE_METATYPE(fincept::algo::OhlcvCandle)
Q_DECLARE_METATYPE(fincept::algo::AlgoConditionStatus)
Q_DECLARE_METATYPE(fincept::algo::AlgoLiveSnapshot)
