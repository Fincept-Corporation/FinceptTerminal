#pragma once
// Crypto Trading types — watchlist, order book, UI state

#include "trading/exchange_service.h"
#include "portfolio/portfolio_types.h"
#include <string>
#include <vector>

namespace fincept::crypto {

// Watchlist entry with live price
struct WatchlistEntry {
    std::string symbol;
    double price = 0.0;
    double change = 0.0;
    double change_pct = 0.0;
    double volume = 0.0;
    bool has_data = false;
};

// Order book level
struct OBLevel {
    double price = 0.0;
    double amount = 0.0;
    double cumulative = 0.0; // running total for depth bar
};

// Order book view mode (ES.45: named enum, not magic ints)
enum class ObViewMode { Book, Volume, Imbalance, Signals };

// Tick snapshot — captured once per second from WS orderbook callback.
// Stores the top-3 bid/ask quantities needed for weighted imbalance,
// plus a 60-second price rise ratio for signal detection.
struct TickSnapshot {
    int64_t timestamp     = 0;
    double  best_bid      = 0.0;
    double  best_ask      = 0.0;
    double  bid_qty[3]    = {};  // top-3 bid quantities (50/30/20 weight)
    double  ask_qty[3]    = {};  // top-3 ask quantities (50/30/20 weight)
    double  imbalance     = 0.0; // (wBid-wAsk)/(wBid+wAsk), range [-1, +1]
    double  rise_ratio_60 = 0.0; // % price change over last 60 seconds
};

// Order entry form state
struct OrderFormState {
    int  side_idx = 0;              // 0=Buy, 1=Sell
    int  type_idx = 0;              // 0=Market, 1=Limit, 2=Stop, 3=StopLimit
    char quantity_buf[32]   = "";
    char price_buf[32]      = "";
    char stop_price_buf[32] = "";
    char sl_price_buf[32]   = "";   // Stop Loss  (optional, non-Market orders)
    char tp_price_buf[32]   = "";   // Take Profit (optional, non-Market orders)
    bool reduce_only = false;
    std::string error;
    std::string success;
    float msg_timer = 0;
};

// Trade entry for Time & Sales feed
struct TradeEntry {
    std::string id;
    std::string side;     // "buy" | "sell"
    double price = 0.0;
    double amount = 0.0;
    int64_t timestamp = 0;
};

// Market info (funding rate, open interest, etc.)
struct MarketInfoData {
    double funding_rate = 0.0;
    double mark_price = 0.0;
    double index_price = 0.0;
    double open_interest = 0.0;
    double open_interest_value = 0.0;
    double maker_fee = 0.0;
    double taker_fee = 0.0;
    int64_t next_funding_time = 0;
    bool has_data = false;
};

// Bottom panel tab
enum class BottomTab { Positions, Orders, History, Trades, MarketInfo, Stats };

// Trading mode
enum class TradingMode { Paper, Live };

} // namespace fincept::crypto
