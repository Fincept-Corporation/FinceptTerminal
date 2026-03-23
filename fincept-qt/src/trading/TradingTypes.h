#pragma once
// Trading Types — shared data types for paper trading, brokers, and exchange services.
// Qt-adapted port from the ImGui codebase (portfolio_types.h + broker_types.h + exchange_service types).

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace fincept::trading {

// ============================================================================
// Paper Trading Types
// ============================================================================

struct PtPortfolio {
    QString id;
    QString name;
    double initial_balance = 0.0;
    double balance = 0.0;
    QString currency = "USD";
    double leverage = 1.0;
    QString margin_mode = "cross";
    double fee_rate = 0.001;
    QString exchange;
    QString created_at;
};

struct PtOrder {
    QString id;
    QString portfolio_id;
    QString symbol;
    QString side;       // "buy" | "sell"
    QString order_type; // "market" | "limit" | "stop" | "stop_limit"
    double quantity = 0.0;
    std::optional<double> price;
    std::optional<double> stop_price;
    double filled_qty = 0.0;
    std::optional<double> avg_price;
    QString status = "pending";
    bool reduce_only = false;
    QString created_at;
    std::optional<QString> filled_at;
};

struct PtPosition {
    QString id;
    QString portfolio_id;
    QString symbol;
    QString side; // "long" | "short"
    double quantity = 0.0;
    double entry_price = 0.0;
    double current_price = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
    double leverage = 1.0;
    std::optional<double> liquidation_price;
    QString opened_at;
};

struct PtTrade {
    QString id;
    QString portfolio_id;
    QString order_id;
    QString symbol;
    QString side;
    double price = 0.0;
    double quantity = 0.0;
    double fee = 0.0;
    double pnl = 0.0;
    QString timestamp;
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

// ============================================================================
// Broker Types
// ============================================================================

template <typename T>
struct ApiResponse {
    bool success = false;
    std::optional<T> data;
    QString error;
    int64_t timestamp = 0;
};

struct TokenExchangeResponse {
    bool success = false;
    QString access_token;
    QString user_id;
    QString additional_data;
    QString error;
};

struct OrderPlaceResponse {
    bool success = false;
    QString order_id;
    QString error;
};

struct BrokerCredentials {
    QString broker_id;
    QString api_key;
    QString api_secret;
    QString access_token;
    QString refresh_token;
    QString user_id;
    QString additional_data;
};

enum class OrderSide { Buy, Sell };
enum class OrderType { Market, Limit, StopLoss, StopLossLimit };
enum class ProductType { Intraday, Delivery, Margin, CoverOrder, BracketOrder };

inline const char* order_side_str(OrderSide s) {
    return s == OrderSide::Buy ? "buy" : "sell";
}

inline const char* order_type_str(OrderType t) {
    switch (t) {
        case OrderType::Market:
            return "market";
        case OrderType::Limit:
            return "limit";
        case OrderType::StopLoss:
            return "stop_loss";
        case OrderType::StopLossLimit:
            return "stop_loss_limit";
    }
    return "market";
}

inline const char* product_type_str(ProductType p) {
    switch (p) {
        case ProductType::Intraday:
            return "intraday";
        case ProductType::Delivery:
            return "delivery";
        case ProductType::Margin:
            return "margin";
        case ProductType::CoverOrder:
            return "cover_order";
        case ProductType::BracketOrder:
            return "bracket_order";
    }
    return "intraday";
}

struct UnifiedOrder {
    QString symbol;
    QString exchange;
    OrderSide side = OrderSide::Buy;
    OrderType order_type = OrderType::Market;
    double quantity = 0;
    double price = 0;
    double stop_price = 0;
    ProductType product_type = ProductType::Intraday;
    QString validity = "DAY";
    double stop_loss = 0;
    double take_profit = 0;
    bool amo = false;
};

struct BrokerPosition {
    QString symbol;
    QString exchange;
    QString product_type;
    double quantity = 0;
    double avg_price = 0;
    double ltp = 0;
    double pnl = 0;
    double day_pnl = 0;
    QString side;
};

struct BrokerHolding {
    QString symbol;
    QString exchange;
    double quantity = 0;
    double avg_price = 0;
    double ltp = 0;
    double pnl = 0;
    double pnl_pct = 0;
    double invested_value = 0;
    double current_value = 0;
};

struct BrokerOrderInfo {
    QString order_id;
    QString symbol;
    QString exchange;
    QString side;
    QString order_type;
    QString product_type;
    double quantity = 0;
    double price = 0;
    double trigger_price = 0;
    double filled_qty = 0;
    double avg_price = 0;
    QString status;
    QString timestamp;
    QString message;
};

struct BrokerQuote {
    QString symbol;
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

struct BrokerFunds {
    double available_balance = 0;
    double used_margin = 0;
    double total_balance = 0;
    double collateral = 0;
    QJsonObject raw_data;
};

enum class BrokerId {
    Fyers,
    Zerodha,
    Upstox,
    Dhan,
    Kotak,
    Groww,
    AliceBlue,
    AngelOne,
    FivePaisa,
    IIFL,
    Motilal,
    Shoonya,
    Alpaca,
    IBKR,
    Tradier,
    SaxoBank
};

inline const char* broker_id_str(BrokerId id) {
    switch (id) {
        case BrokerId::Fyers:
            return "fyers";
        case BrokerId::Zerodha:
            return "zerodha";
        case BrokerId::Upstox:
            return "upstox";
        case BrokerId::Dhan:
            return "dhan";
        case BrokerId::Kotak:
            return "kotak";
        case BrokerId::Groww:
            return "groww";
        case BrokerId::AliceBlue:
            return "aliceblue";
        case BrokerId::AngelOne:
            return "angelone";
        case BrokerId::FivePaisa:
            return "fivepaisa";
        case BrokerId::IIFL:
            return "iifl";
        case BrokerId::Motilal:
            return "motilal";
        case BrokerId::Shoonya:
            return "shoonya";
        case BrokerId::Alpaca:
            return "alpaca";
        case BrokerId::IBKR:
            return "ibkr";
        case BrokerId::Tradier:
            return "tradier";
        case BrokerId::SaxoBank:
            return "saxobank";
    }
    return "unknown";
}

inline std::optional<BrokerId> parse_broker_id(const QString& s) {
    if (s == "fyers")
        return BrokerId::Fyers;
    if (s == "zerodha")
        return BrokerId::Zerodha;
    if (s == "upstox")
        return BrokerId::Upstox;
    if (s == "dhan")
        return BrokerId::Dhan;
    if (s == "kotak")
        return BrokerId::Kotak;
    if (s == "groww")
        return BrokerId::Groww;
    if (s == "aliceblue")
        return BrokerId::AliceBlue;
    if (s == "angelone")
        return BrokerId::AngelOne;
    if (s == "fivepaisa")
        return BrokerId::FivePaisa;
    if (s == "iifl")
        return BrokerId::IIFL;
    if (s == "motilal")
        return BrokerId::Motilal;
    if (s == "shoonya")
        return BrokerId::Shoonya;
    if (s == "alpaca")
        return BrokerId::Alpaca;
    if (s == "ibkr")
        return BrokerId::IBKR;
    if (s == "tradier")
        return BrokerId::Tradier;
    if (s == "saxobank")
        return BrokerId::SaxoBank;
    return std::nullopt;
}

// ============================================================================
// Exchange Service Types (crypto/ccxt)
// ============================================================================

struct TickerData {
    QString symbol;
    double last = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    double high = 0.0;
    double low = 0.0;
    double open = 0.0;
    double close = 0.0;
    double change = 0.0;
    double percentage = 0.0;
    double base_volume = 0.0;
    double quote_volume = 0.0;
    int64_t timestamp = 0;
};

struct Candle {
    int64_t timestamp = 0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
};

struct OrderBookData {
    QString symbol;
    QVector<QPair<double, double>> bids;
    QVector<QPair<double, double>> asks;
    double best_bid = 0.0;
    double best_ask = 0.0;
    double spread = 0.0;
    double spread_pct = 0.0;
};

struct MarketInfo {
    QString symbol;
    QString base;
    QString quote;
    QString type;
    bool active = true;
    double maker_fee = 0.0;
    double taker_fee = 0.0;
    double min_amount = 0.0;
    double min_cost = 0.0;
};

struct ExchangeInfo {
    QString id;
    QString name;
    bool has_fetch_ticker = false;
    bool has_fetch_order_book = false;
    bool has_fetch_ohlcv = false;
    bool has_create_order = false;
    bool has_fetch_balance = false;
    bool has_fetch_positions = false;
    bool has_set_leverage = false;
};

struct TradeData {
    QString id;
    QString symbol;
    QString side;
    double price = 0.0;
    double amount = 0.0;
    double cost = 0.0;
    int64_t timestamp = 0;
};

struct FundingRateData {
    QString symbol;
    double funding_rate = 0.0;
    double mark_price = 0.0;
    double index_price = 0.0;
    int64_t funding_timestamp = 0;
    int64_t next_funding_timestamp = 0;
};

struct OpenInterestData {
    QString symbol;
    double open_interest = 0.0;
    double open_interest_value = 0.0;
    int64_t timestamp = 0;
};

struct ExchangeCredentials {
    QString api_key;
    QString secret;
    QString password;
};

// ============================================================================
// Equity Stream Types
// ============================================================================

struct EquityTick {
    QString symbol;
    QString exchange;
    double ltp = 0.0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    int64_t timestamp = 0;
};

struct EquitySymbol {
    QString symbol;
    QString exchange;
};

// ============================================================================
// Unified Trading Types
// ============================================================================

struct TradingSession {
    QString broker;
    QString mode; // "live" or "paper"
    QString paper_portfolio_id;
    bool is_connected = false;
};

struct UnifiedOrderResponse {
    bool success = false;
    QString order_id;
    QString message;
    QString mode;
};

} // namespace fincept::trading
