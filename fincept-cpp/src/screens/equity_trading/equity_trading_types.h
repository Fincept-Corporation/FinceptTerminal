#pragma once
// Equity Trading Types — UI state types and constants for equity trading screen.

#include "trading/broker_types.h"
#include <string>
#include <vector>

namespace fincept::equity_trading {

// ============================================================================
// Bottom Panel Tabs
// ============================================================================

enum class BottomTab { positions, holdings, orders, history, funds, stats, brokers };

inline const char* bottom_tab_label(BottomTab t) {
    switch (t) {
        case BottomTab::positions: return "POSITIONS";
        case BottomTab::holdings:  return "HOLDINGS";
        case BottomTab::orders:    return "ORDERS";
        case BottomTab::history:   return "HISTORY";
        case BottomTab::funds:     return "FUNDS";
        case BottomTab::stats:     return "STATS";
        case BottomTab::brokers:   return "BROKERS";
    }
    return "POSITIONS";
}

// Left sidebar view
enum class SidebarView { watchlist, indices };

// ============================================================================
// Chart Time Frame
// ============================================================================

enum class TimeFrame { m1, m5, m15, m30, h1, h4, d1, w1 };

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

// ============================================================================
// Order Form Types
// ============================================================================

enum class OrderValidity { Day, IOC, GTC };

inline const char* validity_label(OrderValidity v) {
    switch (v) {
        case OrderValidity::Day: return "DAY";
        case OrderValidity::IOC: return "IOC";
        case OrderValidity::GTC: return "GTC";
    }
    return "DAY";
}

// Watchlist entry with cached quote data
struct WatchlistEntry {
    std::string symbol;
    std::string exchange = "NSE";
    double ltp        = 0.0;
    double change     = 0.0;
    double change_pct = 0.0;
    double volume     = 0.0;
};

// Order form state — all fields for the right-panel order entry form
struct OrderFormState {
    trading::OrderSide   side    = trading::OrderSide::Buy;
    trading::OrderType   type    = trading::OrderType::Market;
    trading::ProductType product = trading::ProductType::Intraday;
    OrderValidity        validity = OrderValidity::Day;
    char   symbol_buf[64] = {};
    double quantity        = 1;
    double price           = 0.0;
    double stop_price      = 0.0;
    double stop_loss       = 0.0;
    double take_profit     = 0.0;
    bool   is_amo          = false;   // After Market Order
    bool   show_bracket    = false;   // Bracket order expanded
    double square_off_pts  = 0.0;    // Bracket profit target in points
    double bracket_sl_pts  = 0.0;    // Bracket stop-loss in points
};

// ============================================================================
// Broker Auth Metadata
// ============================================================================

enum class BrokerAuthType { OAuth, TOTP, ApiKey };

struct BrokerMeta {
    trading::BrokerId id;
    const char* name;
    const char* region;
    BrokerAuthType auth_type;
    const char* api_key_label;
    const char* api_secret_label;
    const char* api_keys_url;
    bool needs_user_id;
    bool needs_totp;
    bool needs_password;
    const char* user_id_label;
    const char* password_label;
};

inline constexpr BrokerMeta BROKER_META[] = {
    // Indian — OAuth
    { trading::BrokerId::Fyers,    "Fyers",    "India", BrokerAuthType::OAuth,
      "APP ID", "SECRET KEY",
      "https://myapi.fyers.in/dashboard",
      false, false, false, "", "" },
    { trading::BrokerId::Zerodha,  "Zerodha",  "India", BrokerAuthType::OAuth,
      "API KEY", "API SECRET",
      "https://developers.kite.trade/apps",
      false, false, false, "", "" },
    { trading::BrokerId::Upstox,   "Upstox",   "India", BrokerAuthType::OAuth,
      "API KEY", "API SECRET",
      "https://developer.upstox.com/",
      false, false, false, "", "" },
    { trading::BrokerId::Dhan,     "Dhan",     "India", BrokerAuthType::OAuth,
      "CLIENT ID", "ACCESS TOKEN",
      "https://dhanhq.co/docs/api/",
      false, false, false, "", "" },
    { trading::BrokerId::IIFL,     "IIFL",     "India", BrokerAuthType::OAuth,
      "API KEY", "API SECRET",
      "https://ttblaze.iifl.com/",
      false, false, false, "", "" },
    { trading::BrokerId::Groww,    "Groww",    "India", BrokerAuthType::OAuth,
      "API KEY", "API SECRET",
      "https://groww.in/api-docs",
      false, false, false, "", "" },
    // Indian — TOTP
    { trading::BrokerId::AngelOne, "Angel One","India", BrokerAuthType::TOTP,
      "API KEY", "PIN",
      "https://smartapi.angelbroking.com/",
      true, true, false, "Client Code", "" },
    { trading::BrokerId::Kotak,    "Kotak",    "India", BrokerAuthType::TOTP,
      "UCC", "NEO-FIN-KEY",
      "https://neotradeapi.kotaksecurities.com/",
      false, false, true, "", "MPIN (6-digit)" },
    { trading::BrokerId::Shoonya,  "Shoonya",  "India", BrokerAuthType::TOTP,
      "USER ID", "PASSWORD",
      "https://api.shoonya.com/",
      true, true, false, "User ID", "" },
    // Indian — ApiKey
    { trading::BrokerId::AliceBlue,"AliceBlue","India", BrokerAuthType::ApiKey,
      "USER ID", "API SECRET",
      "https://ant.aliceblueonline.com/",
      false, false, true, "User ID", "Enc Key" },
    { trading::BrokerId::FivePaisa,"5Paisa",   "India", BrokerAuthType::ApiKey,
      "APP KEY", "CLIENT CODE",
      "https://www.5paisa.com/developerapi",
      false, false, false, "", "" },
    { trading::BrokerId::Motilal,  "Motilal",  "India", BrokerAuthType::ApiKey,
      "API KEY", "PASSWORD",
      "https://openapi.motilaloswal.com/",
      true, false, false, "Client Code", "" },
    // US
    { trading::BrokerId::Alpaca,   "Alpaca",   "US",    BrokerAuthType::ApiKey,
      "API KEY ID", "API SECRET KEY",
      "https://alpaca.markets/",
      false, false, false, "", "" },
    { trading::BrokerId::IBKR,     "IBKR",     "US",    BrokerAuthType::OAuth,
      "API KEY", "API SECRET",
      "https://www.interactivebrokers.com/",
      false, false, false, "", "" },
    { trading::BrokerId::Tradier,  "Tradier",  "US",    BrokerAuthType::OAuth,
      "ACCESS TOKEN", "",
      "https://developer.tradier.com/",
      false, false, false, "", "" },
    // EU
    { trading::BrokerId::SaxoBank, "Saxo Bank","EU",    BrokerAuthType::OAuth,
      "APP KEY", "APP SECRET",
      "https://www.developer.saxo/",
      false, false, false, "", "" },
};
inline constexpr int BROKER_META_COUNT = 16;

// ============================================================================
// Stock Exchange List
// ============================================================================

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
