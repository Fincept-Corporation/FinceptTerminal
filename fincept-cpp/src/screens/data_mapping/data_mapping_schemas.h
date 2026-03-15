#pragma once
// Unified Financial Data Schemas — standardized formats for all data sources
// Mirrors the Tauri desktop schemas/index.ts

#include "data_mapping_types.h"
#include <vector>
#include <string>

namespace fincept::data_mapping {

// ============================================================================
// Get all predefined unified schemas
// ============================================================================
inline std::vector<UnifiedSchema> get_all_schemas() {
    return {
        // --- OHLCV ---
        {
            "OHLCV (Candlestick Data)", SchemaCategory::market_data,
            "Open, High, Low, Close, Volume - Standard candlestick/bar data",
            {
                {"symbol",    FieldType::string_type,   true,  "Trading symbol",                {}, {}, {}},
                {"timestamp", FieldType::datetime_type,  true,  "Bar timestamp (start time)",     {}, {}, {}},
                {"open",      FieldType::number_type,    true,  "Opening price",                  {}, {}, {0.0, {}, {}}},
                {"high",      FieldType::number_type,    true,  "Highest price in the period",    {}, {}, {0.0, {}, {}}},
                {"low",       FieldType::number_type,    true,  "Lowest price in the period",     {}, {}, {0.0, {}, {}}},
                {"close",     FieldType::number_type,    true,  "Closing price",                  {}, {}, {0.0, {}, {}}},
                {"volume",    FieldType::number_type,    true,  "Trading volume",                 {}, {}, {0.0, {}, {}}},
                {"vwap",      FieldType::number_type,    false, "Volume weighted average price",  {}, {}, {0.0, {}, {}}},
                {"trades",    FieldType::number_type,    false, "Number of trades",               {}, {}, {0.0, {}, {}}},
            }
        },
        // --- QUOTE ---
        {
            "Real-time Quote", SchemaCategory::market_data,
            "Real-time or delayed quote with bid/ask spreads",
            {
                {"symbol",        FieldType::string_type,   true,  "Trading symbol",       {}, {}, {}},
                {"timestamp",     FieldType::datetime_type,  true,  "Quote timestamp",      {}, {}, {}},
                {"price",         FieldType::number_type,    true,  "Last traded price",    {}, {}, {0.0, {}, {}}},
                {"bid",           FieldType::number_type,    false, "Best bid price",       {}, {}, {0.0, {}, {}}},
                {"ask",           FieldType::number_type,    false, "Best ask price",       {}, {}, {0.0, {}, {}}},
                {"bidSize",       FieldType::number_type,    false, "Bid quantity",         {}, {}, {0.0, {}, {}}},
                {"askSize",       FieldType::number_type,    false, "Ask quantity",         {}, {}, {0.0, {}, {}}},
                {"volume",        FieldType::number_type,    false, "Total volume",         {}, {}, {0.0, {}, {}}},
                {"change",        FieldType::number_type,    false, "Change from prev close", {}, {}, {}},
                {"changePercent", FieldType::number_type,    false, "Percentage change",    {}, {}, {}},
                {"open",          FieldType::number_type,    false, "Day open price",       {}, {}, {0.0, {}, {}}},
                {"high",          FieldType::number_type,    false, "Day high price",       {}, {}, {0.0, {}, {}}},
                {"low",           FieldType::number_type,    false, "Day low price",        {}, {}, {0.0, {}, {}}},
                {"previousClose", FieldType::number_type,    false, "Previous day close",   {}, {}, {0.0, {}, {}}},
            }
        },
        // --- TICK ---
        {
            "Tick Data", SchemaCategory::market_data,
            "Individual trade ticks",
            {
                {"symbol",    FieldType::string_type,   true,  "Trading symbol",    {}, {}, {}},
                {"timestamp", FieldType::datetime_type,  true,  "Trade timestamp",   {}, {}, {}},
                {"price",     FieldType::number_type,    true,  "Trade price",       {}, {}, {0.0, {}, {}}},
                {"quantity",  FieldType::number_type,    true,  "Trade quantity",    {}, {}, {0.0, {}, {}}},
                {"side",      FieldType::enum_type,      false, "Buy or Sell",       {"BUY", "SELL", "UNKNOWN"}, {}, {}},
                {"tradeId",   FieldType::string_type,    false, "Unique trade ID",   {}, {}, {}},
            }
        },
        // --- ORDER ---
        {
            "Order", SchemaCategory::trading,
            "Trading order (placed, pending, filled, cancelled)",
            {
                {"orderId",        FieldType::string_type,   true,  "Unique order ID",       {}, {}, {}},
                {"symbol",         FieldType::string_type,   true,  "Trading symbol",        {}, {}, {}},
                {"side",           FieldType::enum_type,      true,  "Buy or Sell",           {"BUY", "SELL"}, {}, {}},
                {"type",           FieldType::enum_type,      true,  "Order type",            {"MARKET", "LIMIT", "STOP", "STOP_LIMIT"}, {}, {}},
                {"quantity",       FieldType::number_type,    true,  "Order quantity",        {}, {}, {0.0, {}, {}}},
                {"filledQuantity", FieldType::number_type,    false, "Filled quantity",       {}, {}, {0.0, {}, {}}},
                {"price",          FieldType::number_type,    false, "Limit price",           {}, {}, {0.0, {}, {}}},
                {"stopPrice",      FieldType::number_type,    false, "Stop price",            {}, {}, {0.0, {}, {}}},
                {"averagePrice",   FieldType::number_type,    false, "Average fill price",    {}, {}, {0.0, {}, {}}},
                {"status",         FieldType::enum_type,      true,  "Order status",          {"PENDING", "OPEN", "PARTIALLY_FILLED", "FILLED", "CANCELLED", "REJECTED"}, {}, {}},
                {"timestamp",      FieldType::datetime_type,  true,  "Placement timestamp",   {}, {}, {}},
                {"updatedAt",      FieldType::datetime_type,  false, "Last update timestamp", {}, {}, {}},
                {"timeInForce",    FieldType::enum_type,      false, "Time in force",         {"DAY", "GTC", "IOC", "FOK"}, {}, {}},
                {"exchange",       FieldType::string_type,    false, "Exchange name",         {}, {}, {}},
            }
        },
        // --- POSITION ---
        {
            "Position", SchemaCategory::portfolio_cat,
            "Open position in portfolio",
            {
                {"symbol",       FieldType::string_type,  true,  "Trading symbol",              {}, {}, {}},
                {"quantity",     FieldType::number_type,   true,  "Position quantity",           {}, {}, {}},
                {"averagePrice", FieldType::number_type,   true,  "Average entry price",         {}, {}, {0.0, {}, {}}},
                {"currentPrice", FieldType::number_type,   true,  "Current market price",        {}, {}, {0.0, {}, {}}},
                {"marketValue",  FieldType::number_type,   true,  "Current market value",        {}, {}, {}},
                {"costBasis",    FieldType::number_type,   true,  "Total cost basis",            {}, {}, {}},
                {"pnl",          FieldType::number_type,   true,  "Unrealized P&L",              {}, {}, {}},
                {"pnlPercent",   FieldType::number_type,   true,  "Unrealized P&L %",            {}, {}, {}},
                {"realizedPnl",  FieldType::number_type,   false, "Realized P&L",                {}, {}, {}},
                {"exchange",     FieldType::string_type,   false, "Exchange name",               {}, {}, {}},
                {"productType",  FieldType::enum_type,     false, "Product type",                {"DELIVERY", "INTRADAY", "MARGIN"}, {}, {}},
            }
        },
        // --- PORTFOLIO ---
        {
            "Portfolio Summary", SchemaCategory::portfolio_cat,
            "Overall portfolio summary",
            {
                {"totalValue",  FieldType::number_type,   true,  "Total portfolio value",       {}, {}, {}},
                {"cash",        FieldType::number_type,   true,  "Available cash balance",      {}, {}, {}},
                {"invested",    FieldType::number_type,   true,  "Total invested amount",       {}, {}, {}},
                {"marketValue", FieldType::number_type,   true,  "Total market value",          {}, {}, {}},
                {"pnl",         FieldType::number_type,   true,  "Total unrealized P&L",        {}, {}, {}},
                {"pnlPercent",  FieldType::number_type,   true,  "Total P&L %",                 {}, {}, {}},
                {"realizedPnl", FieldType::number_type,   false, "Total realized P&L",          {}, {}, {}},
                {"dayPnl",      FieldType::number_type,   false, "Today's P&L",                 {}, {}, {}},
                {"buyingPower",  FieldType::number_type,   false, "Available buying power",     {}, {}, {}},
                {"marginUsed",  FieldType::number_type,   false, "Margin used",                 {}, {}, {}},
                {"timestamp",   FieldType::datetime_type,  true,  "Snapshot timestamp",          {}, {}, {}},
            }
        },
        // --- INSTRUMENT ---
        {
            "Instrument", SchemaCategory::reference,
            "Security/instrument master data",
            {
                {"symbol",         FieldType::string_type,  true,  "Trading symbol",     {}, {}, {}},
                {"name",           FieldType::string_type,  true,  "Instrument name",    {}, {}, {}},
                {"exchange",       FieldType::string_type,  true,  "Exchange",           {}, {}, {}},
                {"instrumentType", FieldType::enum_type,    true,  "Type of instrument", {"EQUITY", "FUTURES", "OPTIONS", "CURRENCY", "COMMODITY", "INDEX"}, {}, {}},
                {"isin",           FieldType::string_type,  false, "ISIN code",          {}, {}, {}},
                {"lotSize",        FieldType::number_type,  false, "Lot size",           {}, {}, {1.0, {}, {}}},
                {"tickSize",       FieldType::number_type,  false, "Min price movement", {}, {}, {0.0, {}, {}}},
                {"expiry",         FieldType::datetime_type, false, "Expiry date",       {}, {}, {}},
                {"strike",         FieldType::number_type,  false, "Strike price",       {}, {}, {0.0, {}, {}}},
                {"optionType",     FieldType::enum_type,    false, "Call or Put",        {"CALL", "PUT"}, {}, {}},
            }
        },
    };
}

// Schema lookup by SchemaType enum
inline const UnifiedSchema* get_schema_for_type(SchemaType type) {
    static const auto schemas = get_all_schemas();
    const auto idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(schemas.size())) {
        return &schemas[static_cast<size_t>(idx)];
    }
    return nullptr;
}

// Get list of available schema type names
inline std::vector<std::string> get_schema_type_names() {
    return {"OHLCV", "QUOTE", "TICK", "ORDER", "POSITION", "PORTFOLIO", "INSTRUMENT"};
}

} // namespace fincept::data_mapping
