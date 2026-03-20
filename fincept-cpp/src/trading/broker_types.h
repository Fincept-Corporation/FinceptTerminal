#pragma once
// Broker Types — common types for all broker integrations
// Mirrors Tauri brokers/common.rs

#include <string>
#include <optional>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace fincept::trading {

using json = nlohmann::json;

// ============================================================================
// Common Response Types
// ============================================================================

template<typename T>
struct ApiResponse {
    bool success = false;
    std::optional<T> data;
    std::string error;
    int64_t timestamp = 0;
};

struct TokenExchangeResponse {
    bool success = false;
    std::string access_token;
    std::string user_id;
    std::string additional_data;  // JSON: feed_token, refresh_token, etc.
    std::string error;
};

struct OrderPlaceResponse {
    bool success = false;
    std::string order_id;
    std::string error;
};

// ============================================================================
// Broker Credentials (stored in DB via existing Credential system)
// ============================================================================

struct BrokerCredentials {
    std::string broker_id;
    std::string api_key;
    std::string api_secret;
    std::string access_token;
    std::string refresh_token;
    std::string user_id;
    std::string additional_data;   // JSON string for broker-specific extras
};

// ============================================================================
// Order Types (unified across brokers)
// ============================================================================

enum class OrderSide { Buy, Sell };
enum class OrderType { Market, Limit, StopLoss, StopLossLimit };
enum class ProductType { Intraday, Delivery, Margin, CoverOrder, BracketOrder };

inline const char* order_side_str(OrderSide s) {
    return s == OrderSide::Buy ? "buy" : "sell";
}

inline const char* order_type_str(OrderType t) {
    switch (t) {
        case OrderType::Market:        return "market";
        case OrderType::Limit:         return "limit";
        case OrderType::StopLoss:      return "stop_loss";
        case OrderType::StopLossLimit: return "stop_loss_limit";
    }
    return "market";
}

inline const char* product_type_str(ProductType p) {
    switch (p) {
        case ProductType::Intraday:      return "intraday";
        case ProductType::Delivery:      return "delivery";
        case ProductType::Margin:        return "margin";
        case ProductType::CoverOrder:    return "cover_order";
        case ProductType::BracketOrder:  return "bracket_order";
    }
    return "intraday";
}

// ============================================================================
// Unified Order (broker-agnostic)
// ============================================================================

struct UnifiedOrder {
    std::string symbol;
    std::string exchange;
    OrderSide side = OrderSide::Buy;
    OrderType order_type = OrderType::Market;
    double quantity = 0;
    double price = 0;          // for limit orders
    double stop_price = 0;     // for stop-loss orders
    ProductType product_type = ProductType::Intraday;
    std::string validity = "DAY";
    double stop_loss = 0;
    double take_profit = 0;
    bool amo = false;
};

// ============================================================================
// Position / Holding (returned from brokers)
// ============================================================================

struct BrokerPosition {
    std::string symbol;
    std::string exchange;
    std::string product_type;
    double quantity = 0;
    double avg_price = 0;
    double ltp = 0;            // last traded price
    double pnl = 0;
    double day_pnl = 0;
    std::string side;          // "buy" or "sell"
};

struct BrokerHolding {
    std::string symbol;
    std::string exchange;
    double quantity = 0;
    double avg_price = 0;
    double ltp = 0;
    double pnl = 0;
    double pnl_pct = 0;
    double invested_value = 0;
    double current_value = 0;
};

// ============================================================================
// Order Info (returned from broker order book)
// ============================================================================

struct BrokerOrderInfo {
    std::string order_id;
    std::string symbol;
    std::string exchange;
    std::string side;
    std::string order_type;
    std::string product_type;
    double quantity = 0;
    double price = 0;
    double trigger_price = 0;
    double filled_qty = 0;
    double avg_price = 0;
    std::string status;        // "pending", "filled", "cancelled", "rejected"
    std::string timestamp;
    std::string message;
};

// ============================================================================
// Quote / Market Data
// ============================================================================

struct BrokerQuote {
    std::string symbol;
    double ltp = 0;
    double open = 0;
    double high = 0;
    double low = 0;
    double close = 0;
    double volume = 0;
    double change = 0;
    double change_pct = 0;
    double bid = 0;
    double ask = 0;
    int64_t timestamp = 0;
};

struct BrokerCandle {
    int64_t timestamp = 0;
    double open = 0;
    double high = 0;
    double low = 0;
    double close = 0;
    double volume = 0;
};

// ============================================================================
// Funds / Margin
// ============================================================================

struct BrokerFunds {
    double available_balance = 0;
    double used_margin = 0;
    double total_balance = 0;
    double collateral = 0;
    json raw_data;             // broker-specific raw response
};

// ============================================================================
// Broker ID enum
// ============================================================================

enum class BrokerId {
    Fyers, Zerodha, Upstox, Dhan, Kotak, Groww, AliceBlue, AngelOne,
    FivePaisa, IIFL, Motilal, Shoonya,  // Indian
    Alpaca, IBKR, Tradier,               // US
    SaxoBank                             // EU
};

inline const char* broker_id_str(BrokerId id) {
    switch (id) {
        case BrokerId::Fyers:     return "fyers";
        case BrokerId::Zerodha:   return "zerodha";
        case BrokerId::Upstox:    return "upstox";
        case BrokerId::Dhan:      return "dhan";
        case BrokerId::Kotak:     return "kotak";
        case BrokerId::Groww:     return "groww";
        case BrokerId::AliceBlue: return "aliceblue";
        case BrokerId::AngelOne:  return "angelone";
        case BrokerId::FivePaisa: return "fivepaisa";
        case BrokerId::IIFL:      return "iifl";
        case BrokerId::Motilal:   return "motilal";
        case BrokerId::Shoonya:   return "shoonya";
        case BrokerId::Alpaca:    return "alpaca";
        case BrokerId::IBKR:      return "ibkr";
        case BrokerId::Tradier:   return "tradier";
        case BrokerId::SaxoBank:  return "saxobank";
    }
    return "unknown";
}

inline std::optional<BrokerId> parse_broker_id(const std::string& s) {
    if (s == "fyers")     return BrokerId::Fyers;
    if (s == "zerodha")   return BrokerId::Zerodha;
    if (s == "upstox")    return BrokerId::Upstox;
    if (s == "dhan")      return BrokerId::Dhan;
    if (s == "kotak")     return BrokerId::Kotak;
    if (s == "groww")     return BrokerId::Groww;
    if (s == "aliceblue") return BrokerId::AliceBlue;
    if (s == "angelone")  return BrokerId::AngelOne;
    if (s == "fivepaisa") return BrokerId::FivePaisa;
    if (s == "iifl")      return BrokerId::IIFL;
    if (s == "motilal")   return BrokerId::Motilal;
    if (s == "shoonya")   return BrokerId::Shoonya;
    if (s == "alpaca")    return BrokerId::Alpaca;
    if (s == "ibkr")      return BrokerId::IBKR;
    if (s == "tradier")   return BrokerId::Tradier;
    if (s == "saxobank")  return BrokerId::SaxoBank;
    return std::nullopt;
}

} // namespace fincept::trading
