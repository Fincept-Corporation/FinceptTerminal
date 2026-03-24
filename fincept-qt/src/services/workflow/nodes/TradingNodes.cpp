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
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "", {}, "Ticker symbol", true},
                {"side", "Side", "select", "buy", {"buy", "sell"}, "", true},
                {"order_type", "Order Type", "select", "market", {"market", "limit", "stop", "stop_limit"}, ""},
                {"quantity", "Quantity", "number", 1, {}, "Number of shares/contracts", true},
                {"price", "Limit Price", "number", 0, {}, "Price for limit orders"},
                {"broker", "Broker", "select", "paper", {"paper", "alpaca", "zerodha", "fyers", "ib"}, ""},
            },
        .execute = nullptr, // Wired via UnifiedTrading
    });

    registry.register_type({
        .type_id = "trading.cancel_order",
        .display_name = "Cancel Order",
        .category = "Trading",
        .description = "Cancel an existing order",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"order_id", "Order ID", "string", "", {}, "Order ID to cancel", true},
                {"broker", "Broker", "select", "paper", {"paper", "alpaca", "zerodha", "fyers", "ib"}, ""},
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
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
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
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"status", "Status", "select", "all", {"all", "open", "filled", "cancelled"}, ""},
                {"broker", "Broker", "select", "paper", {"paper", "alpaca", "zerodha", "fyers", "ib"}, ""},
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
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::PortfolioData}},
        .parameters =
            {
                {"broker", "Broker", "select", "paper", {"paper", "alpaca", "zerodha", "fyers", "ib"}, ""},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "trading.get_holdings",
        .display_name = "Get Holdings",
        .category = "Trading",
        .description = "Retrieve portfolio holdings",
        .icon_text = "T",
        .accent_color = "#16a34a",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::PortfolioData}},
        .parameters =
            {
                {"broker", "Broker", "select", "paper", {"paper", "alpaca", "zerodha", "fyers", "ib"}, ""},
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
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"broker", "Broker", "select", "paper", {"paper", "alpaca", "zerodha", "fyers", "ib"}, ""},
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
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "", {}, "Ticker symbol", true},
                {"quantity", "Quantity", "number", 0, {}, "0 = close all"},
                {"broker", "Broker", "select", "paper", {"paper", "alpaca", "zerodha", "fyers", "ib"}, ""},
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
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "", {}, "", true},
                {"side", "Side", "select", "buy", {"buy", "sell"}, "", true},
                {"quantity", "Quantity", "number", 1, {}, "", true},
                {"entry_price", "Entry Price", "number", 0, {}, "Limit price"},
                {"stop_loss", "Stop Loss", "number", 0, {}, "Stop price", true},
                {"take_profit", "Take Profit", "number", 0, {}, "Target price", true},
                {"broker", "Broker", "select", "paper", {"paper", "alpaca", "zerodha", "fyers", "ib"}, ""},
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
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "", {}, "", true},
                {"trail_type", "Trail Type", "select", "percent", {"percent", "fixed"}, ""},
                {"trail_value", "Trail Value", "number", 5.0, {}, "% or $ amount"},
                {"quantity", "Quantity", "number", 0, {}, "0 = all shares"},
                {"broker", "Broker", "select", "paper", {"paper", "alpaca", "zerodha", "fyers", "ib"}, ""},
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
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "", {}, "", true},
                {"side", "Side", "select", "buy", {"buy", "sell"}, ""},
                {"total_quantity", "Total Qty", "number", 100, {}, "", true},
                {"tranches", "Tranches", "number", 4, {}, "Number of orders"},
                {"interval_sec", "Interval (sec)", "number", 60, {}, "Seconds between orders"},
                {"broker", "Broker", "select", "paper", {"paper", "alpaca", "zerodha", "fyers", "ib"}, ""},
            },
        .execute = nullptr,
    });
}

} // namespace fincept::workflow
