#pragma once
// Equity Trading Types — UI state types and constants for equity trading screen.

#include "trading/broker_types.h"
#include <string>
#include <vector>

namespace fincept::equity_trading {

// Bottom panel tabs
enum class BottomTab { positions, holdings, orders, history, brokers };

inline const char* bottom_tab_label(BottomTab t) {
    switch (t) {
        case BottomTab::positions: return "POSITIONS";
        case BottomTab::holdings:  return "HOLDINGS";
        case BottomTab::orders:    return "ORDERS";
        case BottomTab::history:   return "HISTORY";
        case BottomTab::brokers:   return "BROKERS";
    }
    return "POSITIONS";
}

// Left sidebar view
enum class SidebarView { watchlist, indices };

// Chart time frame
enum class TimeFrame {
    m1, m5, m15, m30, h1, h4, d1, w1
};

inline const char* timeframe_label(TimeFrame tf) {
    switch (tf) {
        case TimeFrame::m1:  return "1M";
        case TimeFrame::m5:  return "5M";
        case TimeFrame::m15: return "15M";
        case TimeFrame::m30: return "30M";
        case TimeFrame::h1:  return "1H";
        case TimeFrame::h4:  return "4H";
        case TimeFrame::d1:  return "1D";
        case TimeFrame::w1:  return "1W";
    }
    return "5M";
}

inline const char* timeframe_resolution(TimeFrame tf) {
    switch (tf) {
        case TimeFrame::m1:  return "1";
        case TimeFrame::m5:  return "5";
        case TimeFrame::m15: return "15";
        case TimeFrame::m30: return "30";
        case TimeFrame::h1:  return "60";
        case TimeFrame::h4:  return "240";
        case TimeFrame::d1:  return "D";
        case TimeFrame::w1:  return "W";
    }
    return "5";
}

// Watchlist entry with cached quote data
struct WatchlistEntry {
    std::string symbol;
    std::string exchange = "NSE";
    double ltp = 0.0;
    double change = 0.0;
    double change_pct = 0.0;
    double volume = 0.0;
};

// Order form state
struct OrderFormState {
    trading::OrderSide side = trading::OrderSide::Buy;
    trading::OrderType type = trading::OrderType::Market;
    trading::ProductType product = trading::ProductType::Intraday;
    char symbol_buf[64] = {};
    double quantity = 1;
    double price = 0.0;
    double stop_price = 0.0;
    double stop_loss = 0.0;
    double take_profit = 0.0;
};

// Stock exchange list
struct ExchangeDef {
    const char* code;
    const char* name;
    const char* currency;
    const char* region;
};

inline constexpr ExchangeDef EXCHANGES[] = {
    {"NSE",    "National Stock Exchange",   "INR", "India"},
    {"BSE",    "Bombay Stock Exchange",     "INR", "India"},
    {"NFO",    "NSE F&O",                  "INR", "India"},
    {"MCX",    "Multi Commodity Exchange",  "INR", "India"},
    {"NYSE",   "New York Stock Exchange",   "USD", "US"},
    {"NASDAQ", "NASDAQ",                    "USD", "US"},
    {"LSE",    "London Stock Exchange",     "GBP", "UK"},
    {"XETRA",  "Deutsche Borse",           "EUR", "EU"},
    {"TSE",    "Tokyo Stock Exchange",      "JPY", "Japan"},
    {"HKEX",   "Hong Kong Exchange",        "HKD", "HK"},
};
inline constexpr int EXCHANGE_COUNT = sizeof(EXCHANGES) / sizeof(EXCHANGES[0]);

// Default NIFTY 50 watchlist
inline const char* DEFAULT_WATCHLIST[] = {
    "HDFCBANK", "ICICIBANK", "SBIN", "KOTAKBANK", "AXISBANK",
    "BAJFINANCE", "BAJAJFINSV", "HDFCLIFE", "SBILIFE", "INDUSINDBK",
    "TCS", "INFY", "WIPRO", "HCLTECH", "TECHM",
    "RELIANCE", "ONGC", "NTPC", "POWERGRID", "ADANIPORTS",
    "MARUTI", "TATAMOTORS", "M&M", "BAJAJ-AUTO", "HEROMOTOCO",
    "HINDUNILVR", "ITC", "NESTLEIND", "BRITANNIA", "TATACONSUM",
    "TATASTEEL", "JSWSTEEL", "HINDALCO", "COALINDIA",
    "SUNPHARMA", "DRREDDY", "CIPLA", "APOLLOHOSP",
    "LT", "BHARTIARTL", "ULTRACEMCO", "GRASIM", "TITAN",
    "ASIANPAINT", "BPCL", "SHRIRAMFIN", "TRENT",
};
inline constexpr int DEFAULT_WATCHLIST_COUNT = 47;

// Market indices
struct IndexDef {
    const char* symbol;
    const char* exchange;
};

inline constexpr IndexDef MARKET_INDICES[] = {
    {"NIFTY 50",  "NSE"},
    {"SENSEX",    "BSE"},
    {"BANKNIFTY", "NSE"},
    {"NIFTYIT",   "NSE"},
};
inline constexpr int MARKET_INDICES_COUNT = 4;

} // namespace fincept::equity_trading
