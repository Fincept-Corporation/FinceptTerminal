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

/// Strategy kind. Derived from the ID prefix — never persisted as a column.
///   FCT-XXXXXXXX → Qc (code-based, lives under scripts/strategies/<file>.py)
///   anything else → Dsl (indicator-rule-based, persisted in algo_strategies table)
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
    QJsonArray entry_conditions;
    QJsonArray exit_conditions;
    QString entry_logic = "AND"; // AND, OR
    QString exit_logic = "AND";
    double stop_loss = 0;
    double take_profit = 0;
    double trailing_stop = 0;
    bool is_active = true;
    QString created_at;
    QString updated_at;
    QJsonObject last_backtest; // in-memory only — populated when backtest runs, not persisted

    // For QC strategies, this is the .py file path relative to scripts/strategies/.
    // Empty for DSL strategies (their definition lives in entry_conditions/exit_conditions).
    QString script_path;

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
    QString product_type;                        // e.g. "MIS", "CNC" — broker-specific
    QString mode;                                // paper | live
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
    QString op; // >, <, >=, <=, ==, crosses_above, crosses_below
    double value = 0;
    QString compare_mode = "value"; // value, indicator
    QString compare_indicator;
    QJsonObject compare_params;
    QString compare_field;
};

// ── Indicator categories ────────────────────────────────────────────────────

struct IndicatorDef {
    QString id;
    QString label;
    QString category; // stock, ma, momentum, trend, volatility, volume
    QStringList params;
    QStringList fields;
};

inline QVector<IndicatorDef> algo_indicators() {
    return {
        // Stock attributes
        {"CLOSE", "Close Price", "stock", {}, {"value"}},
        {"OPEN", "Open Price", "stock", {}, {"value"}},
        {"HIGH", "High Price", "stock", {}, {"value"}},
        {"LOW", "Low Price", "stock", {}, {"value"}},
        {"VOLUME", "Volume", "stock", {}, {"value"}},
        {"VWAP", "VWAP", "stock", {}, {"value"}},
        // Moving averages
        {"SMA", "SMA", "ma", {"period"}, {"value"}},
        {"EMA", "EMA", "ma", {"period"}, {"value"}},
        {"WMA", "WMA", "ma", {"period"}, {"value"}},
        {"DEMA", "DEMA", "ma", {"period"}, {"value"}},
        {"TEMA", "TEMA", "ma", {"period"}, {"value"}},
        // Momentum
        {"RSI", "RSI", "momentum", {"period"}, {"value"}},
        {"MACD", "MACD", "momentum", {"fast", "slow", "signal"}, {"line", "signal_line", "histogram"}},
        {"STOCHASTIC", "Stochastic", "momentum", {"k_period", "d_period"}, {"k", "d"}},
        {"CCI", "CCI", "momentum", {"period"}, {"value"}},
        {"WILLIAMS_R", "Williams %R", "momentum", {"period"}, {"value"}},
        {"MFI", "MFI", "momentum", {"period"}, {"value"}},
        {"ROC", "Rate of Change", "momentum", {"period"}, {"value"}},
        // Trend
        {"ADX", "ADX", "trend", {"period"}, {"value", "plus_di", "minus_di"}},
        {"SUPERTREND", "SuperTrend", "trend", {"period", "multiplier"}, {"value", "direction"}},
        {"AROON", "Aroon", "trend", {"period"}, {"up", "down"}},
        {"ICHIMOKU",
         "Ichimoku",
         "trend",
         {"tenkan", "kijun", "senkou"},
         {"tenkan_sen", "kijun_sen", "senkou_a", "senkou_b"}},
        // Volatility
        {"ATR", "ATR", "volatility", {"period"}, {"value"}},
        {"BOLLINGER",
         "Bollinger Bands",
         "volatility",
         {"period", "std_dev"},
         {"upper", "middle", "lower", "width", "pct_b"}},
        {"KELTNER", "Keltner Channel", "volatility", {"period", "multiplier"}, {"upper", "middle", "lower"}},
        {"DONCHIAN", "Donchian Channel", "volatility", {"period"}, {"upper", "lower"}},
        // Volume
        {"OBV", "On Balance Volume", "volume", {}, {"value"}},
        {"CMF", "Chaikin Money Flow", "volume", {"period"}, {"value"}},
    };
}

inline QStringList algo_operators() {
    return {">", "<", ">=", "<=", "==", "crosses_above", "crosses_below", "rising", "falling"};
}

inline QStringList algo_timeframes() {
    return {"live", "1m", "3m", "5m", "10m", "15m", "30m", "1h", "4h", "1d"};
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
