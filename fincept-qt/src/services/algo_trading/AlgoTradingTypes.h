// src/services/algo_trading/AlgoTradingTypes.h
#pragma once
#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::algo {

// ── Strategy ────────────────────────────────────────────────────────────────

struct AlgoStrategy {
    QString id;
    QString name;
    QString description;
    QString timeframe;        // live, 1m, 5m, 15m, 1h, 4h, 1d
    QJsonArray entry_conditions;
    QJsonArray exit_conditions;
    QString entry_logic = "AND";  // AND, OR
    QString exit_logic = "AND";
    double stop_loss = 0;
    double take_profit = 0;
    double trailing_stop = 0;
    bool is_active = true;
    QString created_at;
    QString updated_at;
};

// ── Deployment ──────────────────────────────────────────────────────────────

struct AlgoDeployment {
    QString id;
    QString strategy_id;
    QString strategy_name;
    QString symbol;
    QString mode;             // paper, live
    QString status;           // pending, starting, running, stopped, error
    QString timeframe;
    double quantity = 1.0;
    QString error_message;
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
};

inline QColor deployment_status_color(const QString& status) {
    if (status == "running")  return QColor("#00D66F");
    if (status == "starting") return QColor("#FFC400");
    if (status == "error")    return QColor("#FF3B3B");
    if (status == "stopped")  return QColor("#787878");
    return QColor("#787878"); // pending
}

// ── Trade ───────────────────────────────────────────────────────────────────

struct AlgoTrade {
    QString id;
    QString deployment_id;
    QString symbol;
    QString side;             // BUY, SELL
    double quantity = 0;
    double price = 0;
    double pnl = 0;
    QString signal_reason;
    QString timestamp;
};

// ── Condition ───────────────────────────────────────────────────────────────

struct ConditionDef {
    QString indicator;
    QJsonObject params;
    QString field;
    QString op;               // >, <, >=, <=, ==, crosses_above, crosses_below
    double value = 0;
    QString compare_mode = "value";  // value, indicator
    QString compare_indicator;
    QJsonObject compare_params;
    QString compare_field;
};

// ── Indicator categories ────────────────────────────────────────────────────

struct IndicatorDef {
    QString id;
    QString label;
    QString category;       // stock, ma, momentum, trend, volatility, volume
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
        {"MACD", "MACD", "momentum", {"fast", "slow", "signal"},
         {"line", "signal_line", "histogram"}},
        {"STOCHASTIC", "Stochastic", "momentum", {"k_period", "d_period"}, {"k", "d"}},
        {"CCI", "CCI", "momentum", {"period"}, {"value"}},
        {"WILLIAMS_R", "Williams %R", "momentum", {"period"}, {"value"}},
        {"MFI", "MFI", "momentum", {"period"}, {"value"}},
        {"ROC", "Rate of Change", "momentum", {"period"}, {"value"}},
        // Trend
        {"ADX", "ADX", "trend", {"period"}, {"value", "plus_di", "minus_di"}},
        {"SUPERTREND", "SuperTrend", "trend", {"period", "multiplier"}, {"value", "direction"}},
        {"AROON", "Aroon", "trend", {"period"}, {"up", "down"}},
        {"ICHIMOKU", "Ichimoku", "trend", {"tenkan", "kijun", "senkou"},
         {"tenkan_sen", "kijun_sen", "senkou_a", "senkou_b"}},
        // Volatility
        {"ATR", "ATR", "volatility", {"period"}, {"value"}},
        {"BOLLINGER", "Bollinger Bands", "volatility", {"period", "std_dev"},
         {"upper", "middle", "lower", "width", "pct_b"}},
        {"KELTNER", "Keltner Channel", "volatility", {"period", "multiplier"},
         {"upper", "middle", "lower"}},
        {"DONCHIAN", "Donchian Channel", "volatility", {"period"},
         {"upper", "lower"}},
        // Volume
        {"OBV", "On Balance Volume", "volume", {}, {"value"}},
        {"CMF", "Chaikin Money Flow", "volume", {"period"}, {"value"}},
    };
}

inline QStringList algo_operators() {
    return {">", "<", ">=", "<=", "==",
            "crosses_above", "crosses_below",
            "rising", "falling"};
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

    auto make_cond = [](const QString& ind, const QJsonObject& params,
                        const QString& field, const QString& op, double val) {
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
    macd_bull.append(make_cond("MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}},
                               "histogram", ">", 0));
    presets.append({"MACD Bullish", "momentum", macd_bull});

    // Bollinger Squeeze
    QJsonArray bb_sq;
    bb_sq.append(make_cond("BOLLINGER", {{"period", 20}, {"std_dev", 2}},
                            "width", "<", 0.05));
    presets.append({"Bollinger Squeeze", "volatility", bb_sq});

    // Volume Breakout
    QJsonArray vol_bo;
    QJsonObject vol_params; vol_params["period"] = 20;
    vol_bo.append(make_cond("VOLUME", {}, "value", ">", 1000000));
    presets.append({"High Volume", "volume", vol_bo});

    return presets;
}

// ── Nifty watchlists ────────────────────────────────────────────────────────

inline QStringList nifty50_symbols() {
    return {"RELIANCE", "TCS", "HDFCBANK", "INFY", "ICICIBANK",
            "HINDUNILVR", "SBIN", "BHARTIARTL", "ITC", "KOTAKBANK",
            "LT", "AXISBANK", "BAJFINANCE", "ASIANPAINT", "MARUTI",
            "TITAN", "SUNPHARMA", "ULTRACEMCO", "NESTLEIND", "WIPRO"};
}

inline QStringList bank_nifty_symbols() {
    return {"HDFCBANK", "ICICIBANK", "KOTAKBANK", "AXISBANK", "SBIN",
            "INDUSINDBK", "BANDHANBNK", "FEDERALBNK", "PNB", "BANKBARODA",
            "IDFCFIRSTB", "AUBANK"};
}

} // namespace fincept::services::algo
