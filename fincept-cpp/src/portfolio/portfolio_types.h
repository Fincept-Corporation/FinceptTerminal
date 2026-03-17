#pragma once
// Portfolio & Paper Trading types — shared between services and UI
// Mirrors Tauri/Rust types from portfolio_management.rs and paper_trading.rs

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <map>

namespace fincept::portfolio {

// ─── Investment Portfolio (long-term holding tracker) ───

struct Portfolio {
    std::string id;
    std::string name;
    std::string owner;
    std::string currency = "USD";
    std::optional<std::string> description;
    std::string created_at;
    std::string updated_at;
};

struct PortfolioAsset {
    std::string id;
    std::string portfolio_id;
    std::string symbol;
    double quantity = 0.0;
    double avg_buy_price = 0.0;
    std::string first_purchase_date;
    std::string last_updated;
};

struct Transaction {
    std::string id;
    std::string portfolio_id;
    std::string symbol;
    std::string transaction_type; // "BUY" | "SELL" | "DIVIDEND" | "SPLIT"
    double quantity = 0.0;
    double price = 0.0;
    double total_value = 0.0;
    std::string transaction_date;
    std::optional<std::string> notes;
};

// Computed at runtime (not stored in DB)
struct HoldingWithQuote {
    PortfolioAsset asset;
    double current_price = 0.0;
    double market_value = 0.0;
    double cost_basis = 0.0;
    double unrealized_pnl = 0.0;
    double unrealized_pnl_percent = 0.0;
    double day_change = 0.0;
    double day_change_percent = 0.0;
    double weight = 0.0; // % of total portfolio
};

struct PortfolioSummary {
    Portfolio portfolio;
    std::vector<HoldingWithQuote> holdings;
    double total_market_value = 0.0;
    double total_cost_basis = 0.0;
    double total_unrealized_pnl = 0.0;
    double total_unrealized_pnl_percent = 0.0;
    int total_positions = 0;
    double total_day_change = 0.0;
    double total_day_change_percent = 0.0;
};

} // namespace fincept::portfolio

// ─── Paper Trading (simulated exchange) ───

namespace fincept::trading {

struct PtPortfolio {
    std::string id;
    std::string name;
    double initial_balance = 0.0;
    double balance = 0.0;
    std::string currency = "USD";
    double leverage = 1.0;
    std::string margin_mode = "cross"; // "cross" | "isolated"
    double fee_rate = 0.001;
    std::string exchange;              // broker/exchange ID — isolates portfolios per exchange
    std::string created_at;
};

struct PtOrder {
    std::string id;
    std::string portfolio_id;
    std::string symbol;
    std::string side;        // "buy" | "sell"
    std::string order_type;  // "market" | "limit" | "stop" | "stop_limit"
    double quantity = 0.0;
    std::optional<double> price;
    std::optional<double> stop_price;
    double filled_qty = 0.0;
    std::optional<double> avg_price;
    std::string status = "pending"; // "pending" | "filled" | "partial" | "cancelled"
    bool reduce_only = false;
    std::string created_at;
    std::optional<std::string> filled_at;
};

struct PtPosition {
    std::string id;
    std::string portfolio_id;
    std::string symbol;
    std::string side;        // "long" | "short"
    double quantity = 0.0;
    double entry_price = 0.0;
    double current_price = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
    double leverage = 1.0;
    std::optional<double> liquidation_price;
    std::string opened_at;
};

struct PtTrade {
    std::string id;
    std::string portfolio_id;
    std::string order_id;
    std::string symbol;
    std::string side;
    double price = 0.0;
    double quantity = 0.0;
    double fee = 0.0;
    double pnl = 0.0;
    std::string timestamp;
};

struct PtStats {
    double total_pnl = 0.0;
    double win_rate = 0.0;
    int64_t total_trades = 0;
    int64_t winning_trades = 0;
    int64_t losing_trades = 0;
    double largest_win = 0.0;
    double largest_loss = 0.0;
};

// Price data for order matching
struct PriceData {
    double last = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    double high = 0.0;
    double low = 0.0;
    double volume = 0.0;
    double change = 0.0;
    double change_percent = 0.0;
    int64_t timestamp = 0;
};

} // namespace fincept::trading
