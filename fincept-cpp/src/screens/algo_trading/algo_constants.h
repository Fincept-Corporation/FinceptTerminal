#pragma once
// Algo Trading Constants — indicator definitions, operators, timeframes
// Mirrors: fincept-terminal-desktop/src/components/tabs/algo-trading/constants/indicators.ts

#include <string>
#include <vector>
#include <utility>

namespace fincept::algo {

// ============================================================================
// Indicator definition
// ============================================================================

struct IndicatorDef {
    const char* id;
    const char* name;
    const char* category;
    bool has_period;
    int default_period;
    const char* description;
};

// ============================================================================
// Operator definition
// ============================================================================

struct OperatorDef {
    const char* id;
    const char* label;
    const char* description;
    bool needs_value2;     // "between" needs two values
};

// ============================================================================
// Timeframe definition
// ============================================================================

struct TimeframeDef {
    const char* id;
    const char* label;
    int seconds;           // duration in seconds (0 = live/tick)
};

// ============================================================================
// All indicators (6 categories, 60 total)
// ============================================================================

inline const std::vector<IndicatorDef>& get_indicators() {
    static const std::vector<IndicatorDef> indicators = {
        // --- Stock Attributes (16) ---
        {"close",       "Close Price",          "stock_attributes", false, 0, "Closing price"},
        {"open",        "Open Price",           "stock_attributes", false, 0, "Opening price"},
        {"high",        "High Price",           "stock_attributes", false, 0, "High price"},
        {"low",         "Low Price",            "stock_attributes", false, 0, "Low price"},
        {"volume",      "Volume",               "stock_attributes", false, 0, "Trading volume"},
        {"vwap",        "VWAP",                 "stock_attributes", false, 0, "Volume-weighted average price"},
        {"typical",     "Typical Price",        "stock_attributes", false, 0, "(H+L+C)/3"},
        {"median",      "Median Price",         "stock_attributes", false, 0, "(H+L)/2"},
        {"weighted",    "Weighted Close",       "stock_attributes", false, 0, "(H+L+2C)/4"},
        {"tr",          "True Range",           "stock_attributes", false, 0, "True range"},
        {"atr",         "ATR",                  "stock_attributes", true,  14, "Average true range"},
        {"gap",         "Gap",                  "stock_attributes", false, 0, "Open vs prev close"},
        {"gap_pct",     "Gap %",                "stock_attributes", false, 0, "Gap as percentage"},
        {"range",       "Range",                "stock_attributes", false, 0, "High - Low"},
        {"range_pct",   "Range %",              "stock_attributes", false, 0, "Range as pct of close"},
        {"body_pct",    "Body %",               "stock_attributes", false, 0, "|Open-Close|/Range"},

        // --- Moving Averages (10) ---
        {"sma",         "SMA",                  "moving_averages",  true,  20, "Simple moving average"},
        {"ema",         "EMA",                  "moving_averages",  true,  20, "Exponential moving average"},
        {"wma",         "WMA",                  "moving_averages",  true,  20, "Weighted moving average"},
        {"dema",        "DEMA",                 "moving_averages",  true,  20, "Double EMA"},
        {"tema",        "TEMA",                 "moving_averages",  true,  20, "Triple EMA"},
        {"kama",        "KAMA",                 "moving_averages",  true,  20, "Kaufman adaptive MA"},
        {"hma",         "HMA",                  "moving_averages",  true,  20, "Hull moving average"},
        {"vwma",        "VWMA",                 "moving_averages",  true,  20, "Volume-weighted MA"},
        {"zlema",       "ZLEMA",                "moving_averages",  true,  20, "Zero-lag EMA"},
        {"alma",        "ALMA",                 "moving_averages",  true,  20, "Arnaud Legoux MA"},

        // --- Momentum (10) ---
        {"rsi",         "RSI",                  "momentum",         true,  14, "Relative strength index"},
        {"stoch_k",     "Stochastic %K",        "momentum",         true,  14, "Stochastic oscillator %K"},
        {"stoch_d",     "Stochastic %D",        "momentum",         true,  3,  "Stochastic oscillator %D"},
        {"cci",         "CCI",                  "momentum",         true,  20, "Commodity channel index"},
        {"williams_r",  "Williams %R",          "momentum",         true,  14, "Williams percent range"},
        {"roc",         "ROC",                  "momentum",         true,  12, "Rate of change"},
        {"momentum",    "Momentum",             "momentum",         true,  10, "Price momentum"},
        {"tsi",         "TSI",                  "momentum",         true,  25, "True strength index"},
        {"uo",          "Ultimate Oscillator",  "momentum",         false, 0,  "Ultimate oscillator"},
        {"ao",          "Awesome Oscillator",   "momentum",         false, 0,  "Awesome oscillator"},

        // --- Trend (12) ---
        {"macd",        "MACD Line",            "trend",            true,  12, "MACD line (12,26)"},
        {"macd_signal", "MACD Signal",          "trend",            true,  9,  "MACD signal line"},
        {"macd_hist",   "MACD Histogram",       "trend",            false, 0,  "MACD histogram"},
        {"adx",         "ADX",                  "trend",            true,  14, "Average directional index"},
        {"plus_di",     "+DI",                  "trend",            true,  14, "Positive directional indicator"},
        {"minus_di",    "-DI",                  "trend",            true,  14, "Negative directional indicator"},
        {"aroon_up",    "Aroon Up",             "trend",            true,  25, "Aroon up"},
        {"aroon_down",  "Aroon Down",           "trend",            true,  25, "Aroon down"},
        {"psar",        "Parabolic SAR",        "trend",            false, 0,  "Parabolic SAR"},
        {"supertrend",  "Supertrend",           "trend",            true,  10, "Supertrend"},
        {"ichimoku_a",  "Ichimoku Span A",      "trend",            false, 0,  "Ichimoku cloud span A"},
        {"ichimoku_b",  "Ichimoku Span B",      "trend",            false, 0,  "Ichimoku cloud span B"},

        // --- Volatility (4) ---
        {"bb_upper",    "Bollinger Upper",      "volatility",       true,  20, "Upper Bollinger band"},
        {"bb_middle",   "Bollinger Middle",     "volatility",       true,  20, "Middle Bollinger band"},
        {"bb_lower",    "Bollinger Lower",      "volatility",       true,  20, "Lower Bollinger band"},
        {"kc_upper",    "Keltner Upper",        "volatility",       true,  20, "Upper Keltner channel"},

        // --- Volume (8) ---
        {"obv",         "OBV",                  "volume",           false, 0,  "On-balance volume"},
        {"ad",          "A/D Line",             "volume",           false, 0,  "Accumulation/distribution"},
        {"cmf",         "CMF",                  "volume",           true,  20, "Chaikin money flow"},
        {"mfi",         "MFI",                  "volume",           true,  14, "Money flow index"},
        {"vpt",         "VPT",                  "volume",           false, 0,  "Volume price trend"},
        {"eom",         "EOM",                  "volume",           true,  14, "Ease of movement"},
        {"fi",          "Force Index",          "volume",           true,  13, "Force index"},
        {"nvi",         "NVI",                  "volume",           false, 0,  "Negative volume index"},
    };
    return indicators;
}

// ============================================================================
// All operators (12)
// ============================================================================

inline const std::vector<OperatorDef>& get_operators() {
    static const std::vector<OperatorDef> operators = {
        {">",                    ">",                     "Greater than",                  false},
        {"<",                    "<",                     "Less than",                     false},
        {">=",                   ">=",                    "Greater than or equal",         false},
        {"<=",                   "<=",                    "Less than or equal",            false},
        {"==",                   "==",                    "Equal to",                      false},
        {"crosses_above",        "Crosses Above",         "Crosses above value/indicator", false},
        {"crosses_below",        "Crosses Below",         "Crosses below value/indicator", false},
        {"between",              "Between",               "Between two values",            true},
        {"crossed_above_within", "Crossed Above Within",  "Crossed above within N bars",   false},
        {"crossed_below_within", "Crossed Below Within",  "Crossed below within N bars",   false},
        {"rising",               "Rising",                "Value is rising over N bars",   false},
        {"falling",              "Falling",               "Value is falling over N bars",  false},
    };
    return operators;
}

// ============================================================================
// Timeframes (10)
// ============================================================================

inline const std::vector<TimeframeDef>& get_timeframes() {
    static const std::vector<TimeframeDef> timeframes = {
        {"live", "Live/Tick",   0},
        {"1m",   "1 Minute",    60},
        {"3m",   "3 Minutes",   180},
        {"5m",   "5 Minutes",   300},
        {"10m",  "10 Minutes",  600},
        {"15m",  "15 Minutes",  900},
        {"30m",  "30 Minutes",  1800},
        {"1h",   "1 Hour",      3600},
        {"4h",   "4 Hours",     14400},
        {"1d",   "1 Day",       86400},
    };
    return timeframes;
}

// ============================================================================
// Indicator categories (for UI grouping)
// ============================================================================

inline const std::vector<std::pair<const char*, const char*>>& get_indicator_categories() {
    static const std::vector<std::pair<const char*, const char*>> categories = {
        {"stock_attributes", "Stock Attributes"},
        {"moving_averages",  "Moving Averages"},
        {"momentum",         "Momentum"},
        {"trend",            "Trend"},
        {"volatility",       "Volatility"},
        {"volume",           "Volume"},
    };
    return categories;
}

// ============================================================================
// Broker providers (for strategy config)
// ============================================================================

inline const std::vector<std::pair<const char*, const char*>>& get_broker_providers() {
    static const std::vector<std::pair<const char*, const char*>> providers = {
        // Indian
        {"fyers",     "Fyers"},
        {"zerodha",   "Zerodha"},
        {"upstox",    "Upstox"},
        {"dhan",      "Dhan"},
        {"kotak",     "Kotak"},
        {"groww",     "Groww"},
        {"aliceblue", "AliceBlue"},
        {"angelone",  "AngelOne"},
        {"fivepaisa", "5Paisa"},
        {"iifl",      "IIFL"},
        {"motilal",   "Motilal Oswal"},
        {"shoonya",   "Shoonya"},
        // US
        {"alpaca",    "Alpaca"},
        {"ibkr",      "Interactive Brokers"},
        {"tradier",   "Tradier"},
        // EU
        {"saxobank",  "Saxo Bank"},
    };
    return providers;
}

} // namespace fincept::algo
