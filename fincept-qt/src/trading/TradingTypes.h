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

/// Per-product leverage rules for paper-trading margin calculation.
/// Mirrors OpenAlgo's sandbox leverage config (fund_manager.py::_get_leverage).
/// All fields have sane defaults so existing portfolios keep working unchanged.
struct PtLeverageConfig {
    double equity_mis = 5.0;   // intraday equity (NSE/BSE MIS)
    double equity_cnc = 1.0;   // delivery equity (NSE/BSE CNC/NRML)
    double futures = 10.0;     // futures (FUT suffix on NFO/BFO/MCX/CDS)
    double options_buy = 1.0;  // option buy (premium only)
    double options_sell = 1.0; // option sell (margin estimate)
};

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

    // NEW (Phase 3 §4): per-product leverage rules + market-hours enforcement.
    // These have defaults so existing portfolios (and the existing DB schema,
    // which has no columns for them) keep working. The leverage config and the
    // enforce flag are managed by an in-memory per-portfolio config map in
    // PaperTrading.cpp (see pt_get/set_leverage_config / pt_set_enforce_market_hours).
    PtLeverageConfig leverage_config{};
    bool enforce_market_hours = false; // opt-in; FALSE preserves existing behavior
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

    // NEW (Phase 3 §4): margin blocked from available balance when this order
    // was placed (released on fill/cancel). Default 0 = no margin blocked
    // (reduce-only / position-reducing orders, or pre-margin-engine orders).
    // Persisted via the pt_margin_blocks table, not a pt_orders column, so the
    // existing pt_orders schema and row mapper are unchanged.
    double margin_blocked = 0.0;

    // NEW (Phase 3 §4): product type used for leverage selection
    // ("MIS"/"CNC"/"NRML"); empty = use portfolio default leverage. Not
    // persisted (transient hint for the margin engine); existing call sites
    // that don't set it default to the portfolio-level leverage.
    QString product;
    QString exchange; // exchange for market-hours / instrument-type detection
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

    // NEW (v040): broker product type ("MIS"/"CNC"/"NRML") so the engine can tell
    // intraday from delivery (15:30 MIS auto-square, MIS->CNC convert), and the
    // margin locked from available balance while this position is open (released on
    // close). Persisted in pt_positions. Defaults preserve pre-v040 behavior.
    //
    // Storage model: ALL open exposure lives in pt_positions tagged by product.
    // The Equity screen renders CNC/delivery positions in the Holdings tab and
    // MIS/NRML in the Positions tab (single source of truth, one fill path).
    QString product = "MIS";
    double held_margin = 0.0;
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
    double total_pnl = 0.0; // realized P&L from closed trades
    double win_rate = 0.0;  // winning_trades / total_trades (0..1)
    int64_t total_trades = 0;
    int64_t winning_trades = 0;
    int64_t losing_trades = 0;
    double largest_win = 0.0;
    double largest_loss = 0.0;

    // NEW: richer trade-derived aggregates (all from pt_trades).
    double gross_profit = 0.0;  // sum of positive trade P&L
    double gross_loss = 0.0;    // sum of negative trade P&L (<= 0)
    double avg_win = 0.0;       // gross_profit / winning_trades
    double avg_loss = 0.0;      // gross_loss / losing_trades (<= 0)
    double profit_factor = 0.0; // gross_profit / |gross_loss|
    double total_fees = 0.0;    // sum of trade fees / charges
    double turnover = 0.0;      // sum of price * quantity across trades
    double today_pnl = 0.0;     // realized P&L from trades dated today (local)
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
    QString refresh_token;
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
enum class ProductType { Intraday, Delivery, Margin, CoverOrder, BracketOrder, MTF };

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
        case ProductType::MTF:
            return "mtf";
    }
    return "intraday";
}

/// Indian-broker product mnemonic (MIS/CNC/NRML/...) for a ProductType. Used by
/// the paper engine to tag positions/orders so intraday (MIS) can be told apart
/// from delivery (CNC) for auto-square-off, product conversion and leverage.
inline const char* product_to_broker_str(ProductType p) {
    switch (p) {
        case ProductType::Intraday:
            return "MIS";
        case ProductType::Delivery:
            return "CNC";
        case ProductType::Margin:
            return "NRML";
        case ProductType::CoverOrder:
            return "CO";
        case ProductType::BracketOrder:
            return "BO";
        case ProductType::MTF:
            return "MTF";
    }
    return "MIS";
}

/// True when a broker product string denotes an intraday position that must be
/// auto-squared at session close. Treats MIS / INTRADAY as intraday; everything
/// else (CNC / NRML / delivery / margin / blank) is carry-forward.
inline bool product_is_intraday(const QString& product) {
    return product.compare("MIS", Qt::CaseInsensitive) == 0 ||
           product.compare("intraday", Qt::CaseInsensitive) == 0;
}

/// True when a broker product string denotes a delivery holding (CNC / delivery).
inline bool product_is_delivery(const QString& product) {
    return product.compare("CNC", Qt::CaseInsensitive) == 0 ||
           product.compare("delivery", Qt::CaseInsensitive) == 0;
}

/// Parse a broker product mnemonic (MIS/CNC/NRML/MTF) back to a ProductType.
/// Inverse of product_to_broker_str; unknown/blank defaults to Intraday (MIS).
inline ProductType product_from_broker_str(const QString& s) {
    if (s.compare("CNC", Qt::CaseInsensitive) == 0 || s.compare("delivery", Qt::CaseInsensitive) == 0)
        return ProductType::Delivery;
    if (s.compare("NRML", Qt::CaseInsensitive) == 0 || s.compare("margin", Qt::CaseInsensitive) == 0)
        return ProductType::Margin;
    if (s.compare("MTF", Qt::CaseInsensitive) == 0)
        return ProductType::MTF;
    return ProductType::Intraday;
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
    QString instrument_token; // broker-specific numeric token (e.g. Zerodha/AliceBlue/Dhan)
};

struct BrokerPosition {
    QString symbol;
    QString exchange;
    QString product_type;
    double quantity = 0;
    double avg_price = 0;
    double ltp = 0;
    double pnl = 0;
    double pnl_pct = 0;
    double day_pnl = 0;
    QString side;
};

struct BrokerHolding {
    QString symbol;
    QString exchange;
    double quantity = 0;
    double avg_price = 0;
    double ltp = 0;
    double pnl = 0;          // overall P&L (current_value - invested_value)
    double pnl_pct = 0;
    double invested_value = 0;
    double current_value = 0;
    // Previous-day close, for "Today's P&L" = quantity * (ltp - prev_close).
    // Populated generically from the live quote feed (BrokerQuote.close) so it
    // works for every broker without per-broker holdings-parser changes; 0 until
    // the first tick (Today's P&L then shows 0 rather than a wrong number).
    double prev_close = 0;
};

struct BrokerOrderInfo {
    QString order_id;
    QString exchange_order_id;
    QString symbol;
    QString exchange;
    QString side;
    QString order_type;
    QString product_type;
    double quantity = 0;
    double price = 0;
    double trigger_price = 0;
    double stop_price = 0;
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
    double bid_size = 0;
    double ask_size = 0;
    int64_t timestamp = 0;
    // F&O fields — populated when the symbol is a derivative (CE/PE/FUT)
    // and the broker quote response carries OI. Default zero is safe for
    // equity / spot quotes where OI is not applicable.
    qint64 oi = 0;
    double oi_change_pct = 0;
};

struct BrokerCandle {
    int64_t timestamp = 0;
    double open = 0;
    double high = 0;
    double low = 0;
    double close = 0;
    double volume = 0;
    // Open Interest — populated when the broker historical endpoint returns it
    // (Zerodha c[6] when oi=1 is requested; F&O contracts only). Defaults to 0
    // so existing 6-arg brace-init call sites remain source-compatible.
    double oi = 0;
};

struct BrokerFunds {
    double available_balance = 0;
    double used_margin = 0;
    double total_balance = 0;
    double collateral = 0;
    QJsonObject raw_data;
};

struct MarketCalendarDay {
    QString date;          // "YYYY-MM-DD"
    QString open;          // "09:30"
    QString close;         // "16:00"
    QString session_open;  // pre-market open if present
    QString session_close; // after-hours close if present
};

struct MarketClock {
    QString timestamp; // ISO8601 current time
    bool is_open = false;
    QString next_open;  // ISO8601
    QString next_close; // ISO8601
};

// Individual trade print (time & sales)
struct BrokerTrade {
    QString symbol;
    double price = 0;
    double size = 0;
    QString exchange;
    QString timestamp; // ISO8601
    QStringList conditions;
    QString tape; // "A"/"B"/"C"
};

// Auction print (opening/closing)
struct BrokerAuction {
    QString symbol;
    QString date; // "YYYY-MM-DD"
    // Each auction entry (can be multiple per day: opening + closing)
    struct Entry {
        QString timestamp;
        QString auction_type; // "O" (opening) or "C" (closing)
        double price = 0;
        double size = 0;
        QString exchange;
        QStringList conditions;
    };
    QVector<Entry> entries;
};

// Exchange/condition metadata
struct BrokerMetaEntry {
    QString code;
    QString description;
    QString exchange; // for condition codes: which tape/exchange it applies to
};

// ── Margin Calculator ─────────────────────────────────────────────────────────

/// Per-order margin breakdown returned by the broker's pre-trade margin API.
struct OrderMargin {
    QString symbol;
    QString exchange;
    QString side; // "BUY" / "SELL"
    double quantity = 0.0;
    double price = 0.0;

    // Margin components (all in account currency)
    double total = 0.0;      // total margin required
    double var_margin = 0.0; // VaR margin (equity)
    double elm = 0.0;        // Extreme Loss Margin
    double additional = 0.0; // additional margin (futures/options)
    double bo_margin = 0.0;  // bracket order margin
    double cash = 0.0;       // cash component
    double pnl = 0.0;        // unrealised P&L used as collateral
    double leverage = 0.0;   // implied leverage ratio (total / (price * qty))
    QString error;           // per-order error if margin could not be calculated
};

/// Aggregate margin for a basket of orders (POST /margins/basket).
struct BasketMargin {
    double initial_margin = 0.0; // total margin needed to open
    double final_margin = 0.0;   // total margin needed after netting
    QVector<OrderMargin> orders; // per-order breakdown
};

// ── GTT (Good Till Triggered) Orders — Zerodha-specific ──────────────────────

enum class GttOrderType {
    Single, // fires when price hits one trigger
    OCO,    // one-cancels-other: two triggers (e.g. stop-loss + target)
};

struct GttTrigger {
    double trigger_price = 0.0; // the price level that fires this leg
    double limit_price = 0.0;   // limit price for the resulting order (0 = market)
    double quantity = 0.0;
    OrderSide side = OrderSide::Buy;
    OrderType order_type = OrderType::Limit;
    ProductType product = ProductType::Delivery;
};

struct GttOrder {
    QString gtt_id;   // broker-assigned GTT ID
    QString symbol;   // normalised symbol, e.g. "RELIANCE"
    QString exchange; // "NSE" / "BSE"
    GttOrderType type = GttOrderType::Single;
    double last_price = 0.0; // LTP at the time of GTT creation
    QString status;          // "active" / "triggered" / "cancelled" / "expired"
    QString created_at;
    QString updated_at;
    QVector<GttTrigger> triggers; // 1 for Single, 2 for OCO
};

struct GttPlaceResponse {
    bool success = false;
    QString gtt_id;
    QString error;
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
    Samco,
    Flattrade,
    Paytm,
    Tradejini,
    IciciDirect,
    Alpaca,
    IBKR,
    Tradier,
    SaxoBank,
    MetaTrader4
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
        case BrokerId::Samco:
            return "samco";
        case BrokerId::Flattrade:
            return "flattrade";
        case BrokerId::Paytm:
            return "paytm";
        case BrokerId::Tradejini:
            return "tradejini";
        case BrokerId::IciciDirect:
            return "icicidirect";
        case BrokerId::Alpaca:
            return "alpaca";
        case BrokerId::IBKR:
            return "ibkr";
        case BrokerId::Tradier:
            return "tradier";
        case BrokerId::SaxoBank:
            return "saxobank";
        case BrokerId::MetaTrader4:
            return "metatrader4";
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
    if (s == "samco")
        return BrokerId::Samco;
    if (s == "flattrade")
        return BrokerId::Flattrade;
    if (s == "paytm")
        return BrokerId::Paytm;
    if (s == "tradejini")
        return BrokerId::Tradejini;
    if (s == "icicidirect")
        return BrokerId::IciciDirect;
    if (s == "alpaca")
        return BrokerId::Alpaca;
    if (s == "ibkr")
        return BrokerId::IBKR;
    if (s == "tradier")
        return BrokerId::Tradier;
    if (s == "saxobank")
        return BrokerId::SaxoBank;
    if (s == "metatrader4")
        return BrokerId::MetaTrader4;
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

struct MarkPriceData {
    QString symbol;
    double mark_price = 0.0;
    double index_price = 0.0;
    int64_t timestamp = 0;
};

struct ExchangeCredentials {
    QString api_key;
    QString secret;
    QString password;
    QString wallet_address;
    QString private_key;
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

// ---------------------------------------------------------------------------
// Phase 1: Core trading operations (OpenAlgo bridge)
// ---------------------------------------------------------------------------

struct CancelAllResult {
    QStringList canceled_order_ids;
    QVector<QPair<QString, QString>> failed; // {order_id, error}
    int total_attempted = 0;
};

struct CloseAllResult {
    QStringList closed_symbols;
    QVector<QPair<QString, QString>> failed; // {symbol, error}
    int total_positions = 0;
};

struct SmartOrder {
    QString symbol;
    QString exchange;
    OrderSide action = OrderSide::Buy;
    double quantity = 0;
    double position_size = 0; // target position (positive=long, negative=short, 0=flatten)
    OrderType order_type = OrderType::Market;
    double price = 0;
    double trigger_price = 0;
    ProductType product_type = ProductType::Intraday;
};

struct SmartOrderResult {
    bool action_taken = false;
    QString order_id;
    OrderSide executed_action = OrderSide::Buy;
    double executed_quantity = 0;
    QString message;
};

struct DepthLevel {
    double price = 0;
    int quantity = 0;
    int orders = 0;
};

struct MarketDepth {
    QString symbol;
    QString exchange;
    QVector<DepthLevel> bids;
    QVector<DepthLevel> asks;
    double ltp = 0;
    double volume = 0;
    double oi = 0;
};

struct OptionChainEntry {
    double strike_price = 0;
    QString ce_symbol;
    BrokerQuote ce_quote;
    QString pe_symbol;
    BrokerQuote pe_quote;
    QString label; // "ATM", "ITM1", "OTM1", etc.
};

// ============================================================================
// Synthetic Future (Put-Call Parity)
// ============================================================================

/// Calculate the synthetic future price from ATM option quotes using put-call
/// parity: Synthetic_Future = ATM_Call_LTP - ATM_Put_LTP + ATM_Strike.
/// Useful for deriving the implied fair-value of the underlying from option
/// premiums when the spot/futures price is stale or unavailable.
inline double calculate_synthetic_future(double atm_call_ltp, double atm_put_ltp, double atm_strike)
{
    return atm_call_ltp - atm_put_ltp + atm_strike;
}

// ============================================================================
// Basket & Split Orders
// ============================================================================

struct BasketOrderRequest {
    QVector<UnifiedOrder> orders;
    QString strategy_name;
    bool pre_fetch_quotes = true;
};

struct BasketOrderResult {
    struct OrderResult {
        QString symbol;
        QString exchange;
        bool success = false;
        QString order_id;
        QString error;
    };
    QVector<OrderResult> results;
    int successful = 0;
    int failed = 0;
    int total = 0;
};

struct SplitOrderRequest {
    UnifiedOrder base_order;
    int split_size = 0;
    int delay_between_ms = 100;
};

struct SplitOrderResult {
    QVector<BasketOrderResult::OrderResult> results;
    int total_quantity_placed = 0;
    int chunks_successful = 0;
    int chunks_failed = 0;
};

} // namespace fincept::trading

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::trading::BrokerPosition)
Q_DECLARE_METATYPE(fincept::trading::BrokerHolding)
Q_DECLARE_METATYPE(fincept::trading::BrokerOrderInfo)
Q_DECLARE_METATYPE(fincept::trading::BrokerQuote)
Q_DECLARE_METATYPE(fincept::trading::BrokerFunds)
Q_DECLARE_METATYPE(QVector<fincept::trading::BrokerPosition>)
Q_DECLARE_METATYPE(QVector<fincept::trading::BrokerHolding>)
Q_DECLARE_METATYPE(QVector<fincept::trading::BrokerOrderInfo>)
Q_DECLARE_METATYPE(QVector<fincept::trading::BrokerQuote>)
