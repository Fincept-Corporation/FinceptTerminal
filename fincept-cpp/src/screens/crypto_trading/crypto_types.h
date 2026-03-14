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

// Order entry form state
struct OrderFormState {
    int side_idx = 0;           // 0=Buy, 1=Sell
    int type_idx = 0;           // 0=Market, 1=Limit, 2=Stop, 3=StopLimit
    char quantity_buf[32] = "";
    char price_buf[32] = "";
    char stop_price_buf[32] = "";
    bool reduce_only = false;
    std::string error;
    std::string success;
    float msg_timer = 0;
};

// Bottom panel tab
enum class BottomTab { Positions, Orders, History, Stats };

// Trading mode
enum class TradingMode { Paper, Live };

} // namespace fincept::crypto
