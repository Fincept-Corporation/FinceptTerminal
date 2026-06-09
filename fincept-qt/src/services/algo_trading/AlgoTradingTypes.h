// src/services/algo_trading/AlgoTradingTypes.h
#pragma once
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::algo {

// ── Kind & backend enums ────────────────────────────────────────────────────

/// Strategy kind. All strategies are now Dsl (indicator-rule based, persisted in
/// the algo_strategies table and evaluated by the C++ ConditionEvaluator). The Qc
/// value is retained only so historical deployment records still classify; the
/// Python/LEAN QC library was removed.
enum class StrategyKind { Dsl, Qc };

inline StrategyKind kind_from_id(const QString& id) {
    return id.startsWith(QStringLiteral("FCT-")) ? StrategyKind::Qc : StrategyKind::Dsl;
}

inline QString kind_to_string(StrategyKind k) {
    return k == StrategyKind::Qc ? QStringLiteral("qc") : QStringLiteral("dsl");
}

/// Routing target for an algo deployment's order signals.
///   Paper           → routed to PaperTrading.h's PtPortfolio
///   EquityBroker    → routed to BrokerRegistry::get(broker_id)->place_order
///   CryptoExchange  → routed to ExchangeService / native exchange client
enum class TradingBackend { Paper, EquityBroker, CryptoExchange };

inline QString backend_to_string(TradingBackend b) {
    switch (b) {
    case TradingBackend::Paper:
        return QStringLiteral("paper");
    case TradingBackend::EquityBroker:
        return QStringLiteral("equity_broker");
    case TradingBackend::CryptoExchange:
        return QStringLiteral("crypto_exchange");
    }
    return QStringLiteral("paper");
}

inline TradingBackend backend_from_string(const QString& s) {
    if (s == QStringLiteral("equity_broker"))
        return TradingBackend::EquityBroker;
    if (s == QStringLiteral("crypto_exchange"))
        return TradingBackend::CryptoExchange;
    return TradingBackend::Paper;
}

// ── Strategy ────────────────────────────────────────────────────────────────

struct AlgoStrategy {
    QString id;
    QString name;
    QString description;
    QString timeframe; // live, 1m, 5m, 15m, 1h, 4h, 1d
    QString instrument_type = "equity"; // equity | option | future
    QJsonArray entry_conditions;
    QJsonArray exit_conditions;
    QJsonArray legs; // F&O leg-rule definitions (see fno::fno_legs_to_json); empty for equity
    QString entry_logic = "AND"; // AND, OR
    QString exit_logic = "AND";
    double stop_loss = 0;
    double take_profit = 0;
    double trailing_stop = 0;
    double position_size_pct = 100.0; // in-memory only — % of capital allocated per backtest entry
    bool is_active = true;
    QString created_at;
    QString updated_at;
    QJsonObject last_backtest; // in-memory only — populated when backtest runs, not persisted

    StrategyKind kind() const { return kind_from_id(id); }
};

// ── Deployment ──────────────────────────────────────────────────────────────

struct AlgoDeployment {
    QString id;
    QString strategy_id;
    QString strategy_name;
    QString strategy_kind = "dsl";              // 'dsl' | 'qc' — cached from strategy_id prefix at deploy time
    QString symbol;
    QString exchange;                            // e.g. "NSE", "NASDAQ" — from broker profile
    QString instrument_type = "equity";          // equity | option | future
    QString underlying;                          // F&O underlying, e.g. "NIFTY" (option/future only)
    QString resolved_expiry;                     // concrete expiry chosen at entry, "DD-MMM-YY"
    QJsonArray resolved_legs;                    // concrete contracts placed at entry (restart reattach)
    QString product_type;                        // e.g. "MIS", "CNC" — broker-specific
    QString mode;                                // paper | live
    QString entry_side = "BUY";                 // BUY | SELL — direction of the entry signal
    QString backend = "paper";                   // paper | equity_broker | crypto_exchange
    QString broker_id;                           // BrokerRegistry id; empty for paper
    QString broker_account_id;                   // AccountManager id; empty for paper or single-account brokers
    QString paper_portfolio_id;                  // PtPortfolio id (paper backend only)
    QString status;                              // pending | starting | running | stopped | error | crashed
    QString timeframe;
    double quantity = 1.0;
    double max_order_value = 0;                  // 0 = no limit
    double max_daily_loss = 0;                   // 0 = no limit
    QString error_message;
    qint64 pid = 0;                              // OS pid of runner process; 0 if not running
    QString created_at;
    QString updated_at;

    // Live metrics
    double total_pnl = 0;
    double unrealized_pnl = 0;
    int total_trades = 0;
    double win_rate = 0;
    double max_drawdown = 0;
    double position_qty = 0;
    QString position_side;
    double position_entry = 0;
    double current_price = 0;

    StrategyKind kind() const { return kind_from_id(strategy_id); }
    TradingBackend backend_enum() const { return backend_from_string(backend); }
    bool is_live() const { return mode == QStringLiteral("live"); }
};

inline QColor deployment_status_color(const QString& status) {
    if (status == "running")
        return QColor("#00D66F");
    if (status == "starting")
        return QColor("#FFC400");
    if (status == "error")
        return QColor("#FF3B3B");
    if (status == "stopped")
        return QColor("#787878");
    return QColor("#787878"); // pending
}

// ── Condition ───────────────────────────────────────────────────────────────

struct ConditionDef {
    QString indicator;
    QJsonObject params;
    QString field;
    QString op; // >, <, >=, <=, ==, crosses_above, crosses_below, rising, falling, between
    double value = 0;
    double value2 = 0; // upper bound for the `between` operator (value = lower)
    QString compare_mode = "value"; // value, indicator
    QString compare_indicator;
    QJsonObject compare_params;
    QString compare_field;
    int offset = 0;         // bars-ago for the LHS operand (0 = current bar)
    int compare_offset = 0; // bars-ago for the RHS operand (indicator mode)
};

// ── Indicator categories ────────────────────────────────────────────────────

// A single tunable indicator parameter with a UI-ready range. Aggregate-init as
// {name, min, max, default, step, decimals}.
struct ParamSpec {
    QString name;
    double min = 1;
    double max = 500;
    double def = 14;
    double step = 1;
    int decimals = 0;
};

struct IndicatorDef {
    QString id;
    QString label;
    QString category; // stock, ma, momentum, trend, volatility, volume
    QVector<ParamSpec> params;
    QStringList fields; // named outputs; first is the default
};

inline QVector<IndicatorDef> algo_indicators() {
    return {
        // Stock attributes (no params)
        {"CLOSE", "Close Price", "stock", {}, {"value"}},
        {"OPEN", "Open Price", "stock", {}, {"value"}},
        {"HIGH", "High Price", "stock", {}, {"value"}},
        {"LOW", "Low Price", "stock", {}, {"value"}},
        {"VOLUME", "Volume", "stock", {}, {"value"}},
        {"VWAP", "VWAP", "stock", {}, {"value"}},
        // Moving averages
        {"SMA", "SMA", "ma", {{"period", 1, 500, 20, 1, 0}}, {"value"}},
        {"EMA", "EMA", "ma", {{"period", 1, 500, 20, 1, 0}}, {"value"}},
        {"WMA", "WMA", "ma", {{"period", 1, 500, 20, 1, 0}}, {"value"}},
        {"DEMA", "DEMA", "ma", {{"period", 1, 500, 20, 1, 0}}, {"value"}},
        {"TEMA", "TEMA", "ma", {{"period", 1, 500, 20, 1, 0}}, {"value"}},
        // Momentum
        {"RSI", "RSI", "momentum", {{"period", 2, 100, 14, 1, 0}}, {"value"}},
        {"MACD", "MACD", "momentum",
         {{"fast", 1, 100, 12, 1, 0}, {"slow", 1, 200, 26, 1, 0}, {"signal", 1, 100, 9, 1, 0}},
         {"line", "signal_line", "histogram"}},
        {"STOCHASTIC", "Stochastic", "momentum",
         {{"k_period", 1, 100, 14, 1, 0}, {"d_period", 1, 100, 3, 1, 0}}, {"k", "d"}},
        {"CCI", "CCI", "momentum", {{"period", 1, 100, 20, 1, 0}}, {"value"}},
        {"WILLIAMS_R", "Williams %R", "momentum", {{"period", 1, 100, 14, 1, 0}}, {"value"}},
        {"MFI", "MFI", "momentum", {{"period", 1, 100, 14, 1, 0}}, {"value"}},
        {"ROC", "Rate of Change", "momentum", {{"period", 1, 100, 12, 1, 0}}, {"value"}},
        // Trend
        {"ADX", "ADX", "trend", {{"period", 1, 100, 14, 1, 0}}, {"value", "plus_di", "minus_di"}},
        {"SUPERTREND", "SuperTrend", "trend",
         {{"period", 1, 100, 10, 1, 0}, {"multiplier", 0.5, 10, 3, 0.5, 1}}, {"value", "direction"}},
        {"AROON", "Aroon", "trend", {{"period", 1, 100, 14, 1, 0}}, {"up", "down"}},
        {"ICHIMOKU", "Ichimoku", "trend",
         {{"tenkan", 1, 100, 9, 1, 0}, {"kijun", 1, 100, 26, 1, 0}, {"senkou", 1, 200, 52, 1, 0}},
         {"tenkan_sen", "kijun_sen", "senkou_a", "senkou_b"}},
        // Volatility
        {"ATR", "ATR", "volatility", {{"period", 1, 100, 14, 1, 0}}, {"value"}},
        {"BOLLINGER", "Bollinger Bands", "volatility",
         {{"period", 1, 100, 20, 1, 0}, {"std_dev", 0.5, 5, 2, 0.5, 1}},
         {"upper", "middle", "lower", "width", "pct_b"}},
        {"KELTNER", "Keltner Channel", "volatility",
         {{"period", 1, 100, 20, 1, 0}, {"multiplier", 0.5, 10, 2, 0.5, 1}}, {"upper", "middle", "lower"}},
        {"DONCHIAN", "Donchian Channel", "volatility", {{"period", 1, 100, 20, 1, 0}}, {"upper", "lower"}},
        // Volume
        {"OBV", "On Balance Volume", "volume", {}, {"value"}},
        {"CMF", "Chaikin Money Flow", "volume", {{"period", 1, 100, 20, 1, 0}}, {"value"}},
        {"VOL_WIN_CHG", "Volume Window Δ%", "volume", {{"window", 2, 200, 10, 1, 0}}, {"value"}},
    };
}

inline QStringList algo_operators() {
    return {">",  "<",  ">=",            "<=",            "==",     "crosses_above",
            "crosses_below", "rising", "falling", "between"};
}

inline QStringList algo_timeframes() {
    return {"live", "1m", "3m", "5m", "10m", "15m", "30m", "1h", "4h", "1d"};
}

// Sensible default backtest lookback per timeframe. Daily needs years so long
// indicators (SMA200, EMA200) have valid history; intraday must stay under
// Yahoo's history caps (≈60 days for 5–30m, 7 for 1m).
inline int algo_default_lookback_days(const QString& tf) {
    if (tf == "1d") return 365 * 3;
    if (tf == "4h" || tf == "1h") return 365;
    if (tf == "1m") return 7;
    if (tf == "live") return 365;
    return 55; // 3m / 5m / 10m / 15m / 30m — just under Yahoo's ~60-day cap
}

// ── Strategy templates ────────────────────────────────────────────────────
// Ready-made, editable starting points (long-only, matching the backtest engine).

struct StrategyTemplate {
    QString name;
    QString description;
    QString timeframe;
    QString entry_logic = "AND";
    QString exit_logic = "AND";
    double stop_loss = 2.0;
    double take_profit = 5.0;
    QJsonArray entry;
    QJsonArray exit;
};

inline QVector<StrategyTemplate> algo_strategy_templates() {
    // value-comparison leaf
    auto cv = [](const QString& ind, const QJsonObject& p, const QString& field,
                 const QString& op, double value) {
        QJsonObject c;
        c["indicator"] = ind;
        c["params"] = p;
        c["field"] = field;
        c["operator"] = op;
        c["compare_mode"] = "value";
        c["value"] = value;
        return c;
    };
    // indicator-vs-indicator leaf
    auto ci = [](const QString& ind, const QJsonObject& p, const QString& field, const QString& op,
                 const QString& cmpInd, const QJsonObject& cmpP, const QString& cmpField) {
        QJsonObject c;
        c["indicator"] = ind;
        c["params"] = p;
        c["field"] = field;
        c["operator"] = op;
        c["compare_mode"] = "indicator";
        c["compare_indicator"] = cmpInd;
        c["compare_params"] = cmpP;
        c["compare_field"] = cmpField;
        return c;
    };

    QVector<StrategyTemplate> t;

    StrategyTemplate golden;
    golden.name = "Golden Cross";
    golden.description = "SMA(50) crosses above SMA(200)";
    golden.timeframe = "1d";
    golden.stop_loss = 5;
    golden.take_profit = 15;
    golden.entry = {ci("SMA", {{"period", 50}}, "value", "crosses_above", "SMA", {{"period", 200}}, "value")};
    golden.exit = {ci("SMA", {{"period", 50}}, "value", "crosses_below", "SMA", {{"period", 200}}, "value")};
    t.append(golden);

    StrategyTemplate rsi;
    rsi.name = "RSI Reversal";
    rsi.description = "Buy oversold RSI < 30, exit RSI > 60";
    rsi.timeframe = "1h";
    rsi.entry = {cv("RSI", {{"period", 14}}, "value", "<", 30)};
    rsi.exit = {cv("RSI", {{"period", 14}}, "value", ">", 60)};
    t.append(rsi);

    StrategyTemplate macd;
    macd.name = "MACD Cross";
    macd.description = "MACD line crosses above signal";
    macd.timeframe = "15m";
    macd.entry = {ci("MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "line", "crosses_above",
                     "MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "signal_line")};
    macd.exit = {ci("MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "line", "crosses_below",
                    "MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "signal_line")};
    t.append(macd);

    StrategyTemplate boll;
    boll.name = "Bollinger Breakout";
    boll.description = "Close breaks upper band, exit at middle band";
    boll.timeframe = "1h";
    boll.entry = {ci("CLOSE", {}, "value", "crosses_above", "BOLLINGER", {{"period", 20}, {"std_dev", 2}}, "upper")};
    boll.exit = {ci("CLOSE", {}, "value", "crosses_below", "BOLLINGER", {{"period", 20}, {"std_dev", 2}}, "middle")};
    t.append(boll);

    StrategyTemplate st;
    st.name = "SuperTrend Follow";
    st.description = "Close crosses SuperTrend(10, 3)";
    st.timeframe = "15m";
    st.entry = {ci("CLOSE", {}, "value", "crosses_above", "SUPERTREND", {{"period", 10}, {"multiplier", 3}}, "value")};
    st.exit = {ci("CLOSE", {}, "value", "crosses_below", "SUPERTREND", {{"period", 10}, {"multiplier", 3}}, "value")};
    t.append(st);

    return t;
}

// ── Scanner preset conditions ───────────────────────────────────────────────

struct ScannerPreset {
    QString name;
    QString category;
    QJsonArray conditions;
};

inline QVector<ScannerPreset> scanner_presets() {
    QVector<ScannerPreset> presets;

    auto make_cond = [](const QString& ind, const QJsonObject& params, const QString& field, const QString& op,
                        double val) {
        QJsonObject c;
        c["indicator"] = ind;
        c["params"] = params;
        c["field"] = field;
        c["operator"] = op;
        c["value"] = val;
        return c;
    };

    // RSI Oversold
    QJsonArray rsi_os;
    rsi_os.append(make_cond("RSI", {{"period", 14}}, "value", "<", 30));
    presets.append({"RSI Oversold", "momentum", rsi_os});

    // RSI Overbought
    QJsonArray rsi_ob;
    rsi_ob.append(make_cond("RSI", {{"period", 14}}, "value", ">", 70));
    presets.append({"RSI Overbought", "momentum", rsi_ob});

    // MACD Bullish
    QJsonArray macd_bull;
    macd_bull.append(make_cond("MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "histogram", ">", 0));
    presets.append({"MACD Bullish", "momentum", macd_bull});

    // Bollinger Squeeze
    QJsonArray bb_sq;
    bb_sq.append(make_cond("BOLLINGER", {{"period", 20}, {"std_dev", 2}}, "width", "<", 0.05));
    presets.append({"Bollinger Squeeze", "volatility", bb_sq});

    // Volume Breakout
    QJsonArray vol_bo;
    QJsonObject vol_params;
    vol_params["period"] = 20;
    vol_bo.append(make_cond("VOLUME", {}, "value", ">", 1000000));
    presets.append({"High Volume", "volume", vol_bo});

    return presets;
}

// ── Nifty watchlists ────────────────────────────────────────────────────────

inline QStringList nifty50_symbols() {
    return {"RELIANCE",   "TCS",   "HDFCBANK",  "INFY",       "ICICIBANK", "HINDUNILVR", "SBIN",
            "BHARTIARTL", "ITC",   "KOTAKBANK", "LT",         "AXISBANK",  "BAJFINANCE", "ASIANPAINT",
            "MARUTI",     "TITAN", "SUNPHARMA", "ULTRACEMCO", "NESTLEIND", "WIPRO"};
}

inline QStringList bank_nifty_symbols() {
    return {"HDFCBANK",   "ICICIBANK",  "KOTAKBANK", "AXISBANK",   "SBIN",       "INDUSINDBK",
            "BANDHANBNK", "FEDERALBNK", "PNB",       "BANKBARODA", "IDFCFIRSTB", "AUBANK"};
}

} // namespace fincept::services::algo
