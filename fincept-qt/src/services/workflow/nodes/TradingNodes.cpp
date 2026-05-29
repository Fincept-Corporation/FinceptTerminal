#include "services/workflow/nodes/TradingNodes.h"

#include "services/workflow/NodeRegistry.h"

namespace fincept::workflow {

void register_trading_nodes(NodeRegistry& registry) {
    registry.register_type({
        .type_id = "trading.place_order",
        .display_name = "Place Order",
        .category = "Trading",
        .description = "Submit a buy or sell order",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"mode", "Mode", "select", "paper", {"paper", "live"}, "Paper = simulated, Live = real broker"},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, "Broker (live mode only)"},
                {"symbol", "Symbol", "string", "", {}, "Ticker symbol", true},
                {"exchange", "Exchange", "select", "NSE", {"NSE", "BSE", "NFO", "MCX", "CDS", "NASDAQ", "NYSE"}, ""},
                {"side", "Side", "select", "buy", {"buy", "sell"}, "", true},
                {"product_type", "Product Type", "select", "delivery", {"delivery", "intraday", "margin", "mtf"}, "Delivery = CNC/Holdings, Intraday = MIS"},
                {"order_type", "Order Type", "select", "market", {"market", "limit", "stop", "stop_limit"}, ""},
                {"quantity", "Quantity", "number", 1, {}, "Number of shares/contracts", true},
                {"price", "Limit Price", "number", 0, {}, "Price for limit orders"},
                {"validity", "Validity", "select", "DAY", {"DAY", "IOC"}, ""},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "trading.cancel_order",
        .display_name = "Cancel Order",
        .category = "Trading",
        .description = "Cancel an existing order",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"mode", "Mode", "select", "paper", {"paper", "live"}, ""},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
                {"order_id", "Order ID", "string", "", {}, "Order ID to cancel", true},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "trading.modify_order",
        .display_name = "Modify Order",
        .category = "Trading",
        .description = "Modify an existing order",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"mode", "Mode", "select", "paper", {"paper", "live"}, ""},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
                {"order_id", "Order ID", "string", "", {}, "Order ID to modify", true},
                {"quantity", "New Quantity", "number", 0, {}, ""},
                {"price", "New Price", "number", 0, {}, ""},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "trading.get_orders",
        .display_name = "Get Orders",
        .category = "Trading",
        .description = "Retrieve current orders",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"mode", "Mode", "select", "paper", {"paper", "live"}, ""},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
                {"status", "Status", "select", "all", {"all", "open", "filled", "cancelled"}, ""},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "trading.get_positions",
        .display_name = "Get Positions",
        .category = "Trading",
        .description = "Retrieve open positions",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::PortfolioData}},
        .parameters =
            {
                {"mode", "Mode", "select", "paper", {"paper", "live"}, ""},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "trading.get_holdings",
        .display_name = "Get Holdings",
        .category = "Trading",
        .description = "Retrieve portfolio holdings (live mode only)",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::PortfolioData}},
        .parameters =
            {
                {"mode", "Mode", "select", "live", {"paper", "live"}, ""},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "trading.get_balance",
        .display_name = "Get Balance",
        .category = "Trading",
        .description = "Retrieve account balance and buying power",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"mode", "Mode", "select", "paper", {"paper", "live"}, ""},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "trading.close_position",
        .display_name = "Close Position",
        .category = "Trading",
        .description = "Close an open position",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"mode", "Mode", "select", "paper", {"paper", "live"}, ""},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
                {"symbol", "Symbol", "string", "", {}, "Ticker symbol", true},
                {"exchange", "Exchange", "select", "NSE", {"NSE", "BSE", "NFO", "MCX", "CDS", "NASDAQ", "NYSE"}, ""},
                {"product_type", "Product Type", "select", "delivery", {"delivery", "intraday", "margin", "mtf"}, ""},
                {"quantity", "Quantity", "number", 0, {}, "0 = close all"},
            },
        .execute = nullptr,
    });

    // ── Tier 1 additions ───────────────────────────────────────────

    registry.register_type({
        .type_id = "trading.bracket_order",
        .display_name = "Bracket Order",
        .category = "Trading",
        .description = "OCO bracket: entry + stop loss + take profit",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"mode", "Mode", "select", "paper", {"paper", "live"}, ""},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
                {"symbol", "Symbol", "string", "", {}, "", true},
                {"exchange", "Exchange", "select", "NSE", {"NSE", "BSE", "NFO", "MCX", "CDS", "NASDAQ", "NYSE"}, ""},
                {"side", "Side", "select", "buy", {"buy", "sell"}, "", true},
                {"quantity", "Quantity", "number", 1, {}, "", true},
                {"entry_price", "Entry Price", "number", 0, {}, "Limit price"},
                {"stop_loss", "Stop Loss", "number", 0, {}, "Stop price", true},
                {"take_profit", "Take Profit", "number", 0, {}, "Target price", true},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "trading.trailing_stop",
        .display_name = "Trailing Stop",
        .category = "Trading",
        .description = "Trailing stop loss order",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"mode", "Mode", "select", "paper", {"paper", "live"}, ""},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
                {"symbol", "Symbol", "string", "", {}, "", true},
                {"exchange", "Exchange", "select", "NSE", {"NSE", "BSE", "NFO", "MCX", "CDS", "NASDAQ", "NYSE"}, ""},
                {"product_type", "Product Type", "select", "intraday", {"delivery", "intraday", "margin", "mtf"}, ""},
                {"trail_type", "Trail Type", "select", "percent", {"percent", "fixed"}, ""},
                {"trail_value", "Trail Value", "number", 5.0, {}, "% or $ amount"},
                {"quantity", "Quantity", "number", 0, {}, "0 = all shares"},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "trading.scale_in",
        .display_name = "Scale In",
        .category = "Trading",
        .description = "Gradually build position in tranches",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"mode", "Mode", "select", "paper", {"paper", "live"}, ""},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
                {"symbol", "Symbol", "string", "", {}, "", true},
                {"exchange", "Exchange", "select", "NSE", {"NSE", "BSE", "NFO", "MCX", "CDS", "NASDAQ", "NYSE"}, ""},
                {"product_type", "Product Type", "select", "delivery", {"delivery", "intraday", "margin", "mtf"}, ""},
                {"side", "Side", "select", "buy", {"buy", "sell"}, ""},
                {"total_quantity", "Total Qty", "number", 100, {}, "", true},
                {"tranches", "Tranches", "number", 4, {}, "Number of orders"},
                {"interval_sec", "Interval (sec)", "number", 60, {}, "Seconds between orders"},
            },
        .execute = nullptr,
    });

    // ── Phase 3 §6: OpenAlgo-parity live trading nodes ─────────────
    // Definitions only — execution is wired in ServiceBridges.cpp
    // (wire_trading_bridges) against UnifiedTrading / the active broker.

    // Data: Get Quote — live LTP/bid/ask/volume/OI for a single symbol.
    registry.register_type({
        .type_id = "trading.get_quote",
        .display_name = "Get Quote",
        .category = "Trading",
        .description = "Fetch a live broker quote (LTP, bid, ask, volume, OI) for a symbol",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::MarketData}},
        .parameters =
            {
                {"account_id", "Account ID", "string", "", {}, "Leave blank to use active account for broker"},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
                {"symbol", "Symbol", "string", "", {}, "Ticker symbol", true},
                {"exchange", "Exchange", "select", "NSE", {"NSE", "BSE", "NFO", "MCX", "CDS", "NASDAQ", "NYSE"}, ""},
            },
        .execute = nullptr,
    });

    // Data: Get Position — quantity/avg_price/pnl for a single symbol.
    registry.register_type({
        .type_id = "trading.get_position",
        .display_name = "Get Position",
        .category = "Trading",
        .description = "Fetch the open position (qty, avg price, P&L) for a single symbol",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::PortfolioData}},
        .parameters =
            {
                {"account_id", "Account ID", "string", "", {}, "Leave blank to use active account for broker"},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
                {"symbol", "Symbol", "string", "", {}, "Ticker symbol", true},
                {"exchange", "Exchange", "select", "NSE", {"NSE", "BSE", "NFO", "MCX", "CDS", "NASDAQ", "NYSE"}, ""},
                {"product", "Product", "select", "", {"", "MIS", "CNC", "NRML"}, "Optional product filter"},
            },
        .execute = nullptr,
    });

    // Order: Smart Order — position-aware order (targets a net position size).
    registry.register_type({
        .type_id = "trading.smart_order",
        .display_name = "Smart Order",
        .category = "Trading",
        .description = "Position-aware order — buys/sells the delta needed to reach a target position size",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"account_id", "Account ID", "string", "", {}, "Leave blank to use active account for broker"},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
                {"symbol", "Symbol", "string", "", {}, "Ticker symbol", true},
                {"exchange", "Exchange", "select", "NSE", {"NSE", "BSE", "NFO", "MCX", "CDS", "NASDAQ", "NYSE"}, ""},
                {"position_size", "Position Size", "number", 0, {}, "Target net position (+long / -short / 0=flatten)", true},
                {"order_type", "Order Type", "select", "market", {"market", "limit"}, ""},
                {"price", "Limit Price", "number", 0, {}, "Price for limit orders"},
                {"product", "Product", "select", "intraday", {"delivery", "intraday", "margin", "mtf"}, ""},
            },
        .execute = nullptr,
    });

    // Order: Cancel All — cancel every open order on the account.
    registry.register_type({
        .type_id = "trading.cancel_all",
        .display_name = "Cancel All Orders",
        .category = "Trading",
        .description = "Cancel all open orders on the account",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"account_id", "Account ID", "string", "", {}, "Leave blank to use active account for broker"},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
            },
        .execute = nullptr,
    });

    // Order: Close All — square off every open position on the account.
    registry.register_type({
        .type_id = "trading.close_all",
        .display_name = "Close All Positions",
        .category = "Trading",
        .description = "Square off all open positions on the account",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"account_id", "Account ID", "string", "", {}, "Leave blank to use active account for broker"},
                {"broker", "Broker", "select", "fyers", {"fyers", "zerodha", "alpaca", "ibkr", "upstox", "dhan"}, ""},
            },
        .execute = nullptr,
    });

    // Alert: Trading Alert — fire a workflow notification (no outputs).
    registry.register_type({
        .type_id = "trading.alert",
        .display_name = "Trading Alert",
        .category = "Trading",
        .description = "Send a trading alert via the notification system or log",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {},
        .parameters =
            {
                {"message", "Message", "string", "", {}, "Alert message", true},
                {"channel", "Channel", "select", "notification", {"notification", "log"}, "Where to deliver the alert"},
            },
        .execute = nullptr,
    });
}

} // namespace fincept::workflow
