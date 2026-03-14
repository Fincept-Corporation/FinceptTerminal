// Node Registry — Singleton implementation
// Registers all builtin node definitions on first init()

#include "node_registry.h"
#include "core/logger.h"
#include <cctype>

namespace fincept::node_editor {

NodeRegistry& NodeRegistry::instance() {
    static NodeRegistry reg;
    return reg;
}

void NodeRegistry::register_category(const std::string& name, ImU32 color) {
    if (cat_index_.count(name)) return;
    cat_index_[name] = categories_.size();
    categories_.push_back({name, color});
}

void NodeRegistry::ensure_category(const std::string& name, ImU32 color) {
    register_category(name, color);
}

void NodeRegistry::register_node(NodeDef def) {
    ensure_category(def.category, def.color);
    type_index_[def.type] = defs_.size();
    defs_.push_back(std::move(def));
}

const NodeDef* NodeRegistry::get_node(const std::string& type) const {
    auto it = type_index_.find(type);
    if (it == type_index_.end()) return nullptr;
    return &defs_[it->second];
}

std::vector<const NodeDef*> NodeRegistry::get_by_category(const std::string& category) const {
    std::vector<const NodeDef*> result;
    for (auto& d : defs_) {
        if (d.category == category) result.push_back(&d);
    }
    return result;
}

std::vector<const NodeDef*> NodeRegistry::search(const std::string& query) const {
    std::string q;
    for (char c : query) q += (char)std::tolower((unsigned char)c);

    std::vector<const NodeDef*> result;
    for (auto& d : defs_) {
        std::string label_lower;
        for (char c : d.label) label_lower += (char)std::tolower((unsigned char)c);
        std::string type_lower;
        for (char c : d.type) type_lower += (char)std::tolower((unsigned char)c);
        std::string desc_lower;
        for (char c : d.description) desc_lower += (char)std::tolower((unsigned char)c);

        if (label_lower.find(q) != std::string::npos ||
            type_lower.find(q) != std::string::npos ||
            desc_lower.find(q) != std::string::npos) {
            result.push_back(&d);
        }
    }
    return result;
}

// =============================================================================
// Helper to build a NodeDef inline
// =============================================================================
static NodeDef make_def(const std::string& type, const std::string& label,
                        const std::string& category, ImU32 color,
                        const std::string& desc, int inputs, int outputs,
                        std::vector<NodeParam> params = {}) {
    NodeDef d;
    d.type = type;
    d.label = label;
    d.category = category;
    d.color = color;
    d.description = desc;
    d.num_inputs = inputs;
    d.num_outputs = outputs;
    d.default_params = std::move(params);
    return d;
}

// =============================================================================
// Category colors
// =============================================================================
static constexpr ImU32 COL_INPUT      = IM_COL32(59, 130, 246, 255);   // blue
static constexpr ImU32 COL_PROCESSING = IM_COL32(16, 185, 129, 255);   // green
static constexpr ImU32 COL_AI         = IM_COL32(139, 92, 246, 255);   // purple
static constexpr ImU32 COL_OUTPUT     = IM_COL32(245, 158, 11, 255);   // amber
static constexpr ImU32 COL_ANALYTICS  = IM_COL32(6, 182, 212, 255);    // cyan
static constexpr ImU32 COL_RISK       = IM_COL32(239, 68, 68, 255);    // red
static constexpr ImU32 COL_TRADING    = IM_COL32(234, 179, 8, 255);    // yellow
static constexpr ImU32 COL_TRIGGERS   = IM_COL32(168, 85, 247, 255);   // purple-bright
static constexpr ImU32 COL_TRANSFORM  = IM_COL32(236, 72, 153, 255);   // pink
static constexpr ImU32 COL_CONTROL    = IM_COL32(250, 204, 21, 255);   // bright yellow
static constexpr ImU32 COL_SAFETY     = IM_COL32(220, 38, 38, 255);    // deep red
static constexpr ImU32 COL_NOTIFY     = IM_COL32(249, 115, 22, 255);   // orange
static constexpr ImU32 COL_UTILITY    = IM_COL32(156, 163, 175, 255);  // gray
static constexpr ImU32 COL_DATA       = IM_COL32(59, 130, 246, 255);   // blue
static constexpr ImU32 COL_FILES      = IM_COL32(107, 114, 128, 255);  // dark gray
static constexpr ImU32 COL_MARKET     = IM_COL32(34, 197, 94, 255);    // green-bright

// =============================================================================
// Register all 82 builtin nodes
// =============================================================================
void NodeRegistry::init() {
    if (initialized_) return;

    // Register categories in display order
    register_category("Triggers",     COL_TRIGGERS);
    register_category("Input",        COL_INPUT);
    register_category("Market Data",  COL_MARKET);
    register_category("Processing",   COL_PROCESSING);
    register_category("Transform",    COL_TRANSFORM);
    register_category("AI",           COL_AI);
    register_category("Analytics",    COL_ANALYTICS);
    register_category("Risk",         COL_RISK);
    register_category("Trading",      COL_TRADING);
    register_category("Control Flow", COL_CONTROL);
    register_category("Safety",       COL_SAFETY);
    register_category("Notifications",COL_NOTIFY);
    register_category("Output",       COL_OUTPUT);
    register_category("Utilities",    COL_UTILITY);
    register_category("Data",         COL_DATA);
    register_category("Files",        COL_FILES);
    register_category("Core",         COL_UTILITY);

    // =========================================================================
    // TRIGGERS (6)
    // =========================================================================
    register_node(make_def("manual-trigger", "Manual Trigger", "Triggers", COL_TRIGGERS,
        "Manually start workflow execution", 0, 1));

    register_node(make_def("schedule-trigger", "Schedule Trigger", "Triggers", COL_TRIGGERS,
        "Trigger on a cron schedule", 0, 1, {
            {"schedule", "Schedule", "string", std::string("0 9 * * MON-FRI"), {}, true, "Cron expression"},
            {"timezone", "Timezone", "options", std::string("UTC"),
             {"UTC", "US/Eastern", "US/Pacific", "Europe/London", "Asia/Tokyo"}, false, "Timezone"},
        }));

    register_node(make_def("price-alert-trigger", "Price Alert Trigger", "Triggers", COL_TRIGGERS,
        "Trigger when price crosses a threshold", 0, 1, {
            {"symbol", "Symbol", "string", std::string("AAPL"), {}, true, "Ticker symbol"},
            {"condition", "Condition", "options", std::string("above"),
             {"above", "below", "crosses_above", "crosses_below"}, true, "Alert condition"},
            {"price", "Price", "number", 150.0, {}, true, "Price threshold"},
        }));

    register_node(make_def("news-event-trigger", "News Event Trigger", "Triggers", COL_TRIGGERS,
        "Trigger on news events for a symbol", 0, 1, {
            {"symbol", "Symbol", "string", std::string("AAPL"), {}, true, "Ticker symbol"},
            {"keywords", "Keywords", "string", std::string("earnings,dividend"), {}, false, "Comma-separated keywords"},
        }));

    register_node(make_def("market-event-trigger", "Market Event Trigger", "Triggers", COL_TRIGGERS,
        "Trigger on market-level events", 0, 1, {
            {"event_type", "Event Type", "options", std::string("market_open"),
             {"market_open", "market_close", "circuit_breaker", "high_volatility"}, true, "Event type"},
            {"exchange", "Exchange", "options", std::string("NYSE"),
             {"NYSE", "NASDAQ", "LSE", "TSE", "SSE"}, false, "Exchange"},
        }));

    register_node(make_def("webhook-trigger", "Webhook Trigger", "Triggers", COL_TRIGGERS,
        "Trigger from external webhook", 0, 1, {
            {"path", "Path", "string", std::string("/webhook/workflow"), {}, true, "Webhook path"},
            {"method", "Method", "options", std::string("POST"), {"GET", "POST"}, false, "HTTP method"},
        }));

    // =========================================================================
    // INPUT (4 — existing)
    // =========================================================================
    register_node(make_def("data-source", "Data Source", "Input", COL_INPUT,
        "Connect to databases and fetch data", 0, 1, {
            {"source_type", "Source", "options", std::string("yahoo"),
             {"yahoo", "alpha_vantage", "csv", "database"}, true, "Data source provider"},
            {"symbol", "Symbol", "string", std::string("AAPL"), {}, true, "Ticker symbol"},
            {"timeframe", "Timeframe", "options", std::string("1d"),
             {"1m", "5m", "15m", "1h", "1d", "1wk", "1mo"}, false, "Data timeframe"},
            {"start_date", "Start Date", "string", std::string("2024-01-01"), {}, false, "Start date"},
            {"end_date", "End Date", "string", std::string("2024-12-31"), {}, false, "End date"},
        }));

    register_node(make_def("news-feed", "News Feed", "Input", COL_INPUT,
        "Fetch financial news for a symbol or topic", 0, 1, {
            {"query", "Query", "string", std::string("AAPL"), {}, true, "Search query or symbol"},
            {"source", "Source", "options", std::string("all"),
             {"all", "reuters", "bloomberg", "sec", "finviz"}, false, "News source"},
            {"max_articles", "Max Articles", "number", 50, {}, false, "Maximum articles to fetch"},
        }));

    register_node(make_def("screener", "Stock Screener", "Input", COL_INPUT,
        "Screen stocks by fundamental/technical criteria", 0, 1, {
            {"market", "Market", "options", std::string("US"),
             {"US", "EU", "APAC", "crypto", "forex"}, true, "Market to screen"},
            {"min_mcap", "Min Market Cap (M)", "number", 1000.0, {}, false, "Minimum market cap in millions"},
            {"min_volume", "Min Avg Volume", "number", 100000.0, {}, false, "Minimum average daily volume"},
            {"sector", "Sector", "options", std::string("all"),
             {"all", "technology", "healthcare", "financials", "energy", "consumer", "industrials"}, false, "Sector filter"},
        }));

    register_node(make_def("watchlist", "Watchlist", "Input", COL_INPUT,
        "Load symbols from a watchlist", 0, 1, {
            {"list_name", "Watchlist", "string", std::string("default"), {}, true, "Watchlist name"},
        }));

    // =========================================================================
    // MARKET DATA (7)
    // =========================================================================
    register_node(make_def("get-quote", "Get Quote", "Market Data", COL_MARKET,
        "Get real-time quote for a symbol", 0, 1, {
            {"symbol", "Symbol", "string", std::string("AAPL"), {}, true, "Ticker symbol"},
            {"exchange", "Exchange", "options", std::string("auto"),
             {"auto", "NYSE", "NASDAQ", "LSE", "TSE"}, false, "Exchange"},
        }));

    register_node(make_def("get-historical", "Get Historical Data", "Market Data", COL_MARKET,
        "Fetch OHLCV historical data", 0, 1, {
            {"symbol", "Symbol", "string", std::string("AAPL"), {}, true, "Ticker symbol"},
            {"period", "Period", "options", std::string("1y"),
             {"1d", "5d", "1mo", "3mo", "6mo", "1y", "2y", "5y", "max"}, false, "Data period"},
            {"interval", "Interval", "options", std::string("1d"),
             {"1m", "5m", "15m", "1h", "1d", "1wk", "1mo"}, false, "Bar interval"},
        }));

    register_node(make_def("get-fundamentals", "Get Fundamentals", "Market Data", COL_MARKET,
        "Fetch fundamental data (P/E, EPS, etc.)", 0, 1, {
            {"symbol", "Symbol", "string", std::string("AAPL"), {}, true, "Ticker symbol"},
            {"data_type", "Data Type", "options", std::string("overview"),
             {"overview", "income_statement", "balance_sheet", "cash_flow", "earnings"}, false, "Fundamental data type"},
        }));

    register_node(make_def("get-market-depth", "Get Market Depth", "Market Data", COL_MARKET,
        "Fetch order book / market depth", 0, 1, {
            {"symbol", "Symbol", "string", std::string("AAPL"), {}, true, "Ticker symbol"},
            {"depth", "Depth", "number", 10, {}, false, "Number of levels"},
        }));

    register_node(make_def("get-ticker-stats", "Get Ticker Stats", "Market Data", COL_MARKET,
        "Fetch key statistics for a symbol", 0, 1, {
            {"symbol", "Symbol", "string", std::string("AAPL"), {}, true, "Ticker symbol"},
        }));

    register_node(make_def("stream-quotes", "Stream Quotes", "Market Data", COL_MARKET,
        "Stream real-time quotes via WebSocket", 0, 1, {
            {"symbols", "Symbols", "string", std::string("AAPL,MSFT,GOOGL"), {}, true, "Comma-separated symbols"},
            {"feed", "Feed", "options", std::string("delayed"),
             {"real_time", "delayed"}, false, "Data feed type"},
        }));

    register_node(make_def("yfinance", "YFinance", "Market Data", COL_MARKET,
        "Fetch data via yfinance Python library", 0, 1, {
            {"symbol", "Symbol", "string", std::string("AAPL"), {}, true, "Ticker symbol"},
            {"period", "Period", "options", std::string("1y"),
             {"1d", "5d", "1mo", "3mo", "6mo", "1y", "2y", "5y", "10y", "max"}, false, "Data period"},
            {"interval", "Interval", "options", std::string("1d"),
             {"1m", "5m", "15m", "30m", "1h", "1d", "1wk", "1mo"}, false, "Data interval"},
        }));

    // =========================================================================
    // PROCESSING (3 — existing)
    // =========================================================================
    register_node(make_def("technical-indicator", "Technical Indicator", "Processing", COL_PROCESSING,
        "Calculate technical indicators on market data", 1, 1, {
            {"indicator", "Indicator", "options", std::string("SMA"),
             {"SMA", "EMA", "RSI", "MACD", "BB", "ATR", "VWAP", "OBV"}, true, "Indicator type"},
            {"period", "Period", "number", 14, {}, false, "Lookback period"},
            {"source_column", "Source Column", "options", std::string("close"),
             {"open", "high", "low", "close", "volume"}, false, "Price column to use"},
        }));

    register_node(make_def("data-filter", "Data Filter", "Processing", COL_PROCESSING,
        "Filter and transform data rows/columns", 1, 1, {
            {"filter_type", "Filter", "options", std::string("date_range"),
             {"date_range", "column_select", "dropna", "resample", "normalize", "pct_change"}, true, "Filter type"},
            {"column", "Column", "string", std::string("close"), {}, false, "Target column"},
            {"value", "Value", "string", std::string(""), {}, false, "Filter value / parameter"},
        }));

    register_node(make_def("data-merger", "Data Merger", "Processing", COL_PROCESSING,
        "Merge multiple data streams into one", 2, 1, {
            {"join_type", "Join", "options", std::string("inner"),
             {"inner", "outer", "left", "right"}, true, "Join type"},
            {"join_on", "Join On", "options", std::string("date"),
             {"date", "index", "symbol"}, false, "Join key"},
        }));

    // =========================================================================
    // TRANSFORM (8)
    // =========================================================================
    register_node(make_def("filter", "Filter", "Transform", COL_TRANSFORM,
        "Filter rows by condition", 1, 1, {
            {"column", "Column", "string", std::string("close"), {}, true, "Column to filter on"},
            {"operator", "Operator", "options", std::string(">"),
             {">", "<", ">=", "<=", "==", "!="}, true, "Comparison operator"},
            {"value", "Value", "string", std::string("100"), {}, true, "Filter value"},
        }));

    register_node(make_def("sort", "Sort", "Transform", COL_TRANSFORM,
        "Sort data by column", 1, 1, {
            {"column", "Column", "string", std::string("date"), {}, true, "Sort column"},
            {"direction", "Direction", "options", std::string("asc"),
             {"asc", "desc"}, false, "Sort direction"},
        }));

    register_node(make_def("map", "Map", "Transform", COL_TRANSFORM,
        "Apply expression to each row", 1, 1, {
            {"expression", "Expression", "string", std::string("value * 1.0"), {}, true, "Map expression"},
            {"output_column", "Output Column", "string", std::string("result"), {}, false, "Output column name"},
        }));

    register_node(make_def("aggregate", "Aggregate", "Transform", COL_TRANSFORM,
        "Aggregate data (sum, mean, etc.)", 1, 1, {
            {"operation", "Operation", "options", std::string("mean"),
             {"sum", "mean", "median", "min", "max", "count", "std"}, true, "Aggregation operation"},
            {"column", "Column", "string", std::string("close"), {}, false, "Column to aggregate"},
        }));

    register_node(make_def("deduplicate", "Deduplicate", "Transform", COL_TRANSFORM,
        "Remove duplicate rows", 1, 1, {
            {"key", "Key Column", "string", std::string("date"), {}, true, "Dedup key column"},
            {"keep", "Keep", "options", std::string("first"), {"first", "last"}, false, "Which duplicate to keep"},
        }));

    register_node(make_def("group-by", "Group By", "Transform", COL_TRANSFORM,
        "Group data by column", 1, 1, {
            {"key", "Key Column", "string", std::string("symbol"), {}, true, "Group key"},
            {"operation", "Operation", "options", std::string("mean"),
             {"sum", "mean", "median", "min", "max", "count"}, false, "Group operation"},
        }));

    register_node(make_def("join", "Join", "Transform", COL_TRANSFORM,
        "Join two datasets", 2, 1, {
            {"join_type", "Join Type", "options", std::string("inner"),
             {"inner", "outer", "left", "right"}, true, "Join type"},
            {"key", "Key", "string", std::string("date"), {}, true, "Join key column"},
        }));

    register_node(make_def("reshape", "Reshape", "Transform", COL_TRANSFORM,
        "Reshape data (pivot/melt)", 1, 1, {
            {"operation", "Operation", "options", std::string("pivot"),
             {"pivot", "melt", "transpose"}, true, "Reshape operation"},
            {"index", "Index", "string", std::string("date"), {}, false, "Index column"},
            {"columns", "Columns", "string", std::string("symbol"), {}, false, "Columns to reshape"},
        }));

    // =========================================================================
    // AI (3 — existing agent-mediator + 2 new)
    // =========================================================================
    register_node(make_def("agent-mediator", "Agent Mediator", "AI", COL_AI,
        "AI-powered data analysis and decision making", 1, 1, {
            {"model", "Model", "options", std::string("gpt-4"),
             {"gpt-4", "gpt-4o", "claude-sonnet", "claude-opus", "ollama-local"}, true, "AI model"},
            {"prompt", "Prompt", "string", std::string("Analyze the provided data"), {}, true, "Analysis prompt"},
            {"temperature", "Temperature", "number", 0.7, {}, false, "Model temperature (0-1)"},
        }));

    register_node(make_def("agent-node", "Agent Node", "AI", COL_AI,
        "Single LLM agent for analysis", 1, 1, {
            {"provider", "Provider", "options", std::string("openai"),
             {"openai", "anthropic", "ollama", "groq"}, true, "LLM provider"},
            {"model", "Model", "string", std::string("gpt-4o"), {}, true, "Model ID"},
            {"system_prompt", "System Prompt", "string", std::string("You are a financial analyst."), {}, false, "System prompt"},
            {"prompt", "Prompt", "string", std::string("Analyze the data"), {}, true, "User prompt"},
            {"temperature", "Temperature", "number", 0.7, {}, false, "Temperature"},
            {"max_tokens", "Max Tokens", "number", 2048, {}, false, "Max output tokens"},
        }));

    register_node(make_def("multi-agent", "Multi-Agent", "AI", COL_AI,
        "Multi-LLM consensus analysis", 1, 1, {
            {"agents", "Agent Count", "number", 3, {}, true, "Number of agents"},
            {"models", "Models", "string", std::string("gpt-4o,claude-sonnet,ollama-local"), {}, true, "Comma-separated model list"},
            {"prompt", "Prompt", "string", std::string("Analyze and provide recommendation"), {}, true, "Shared prompt"},
            {"consensus", "Consensus", "options", std::string("majority"),
             {"majority", "unanimous", "weighted"}, false, "Consensus method"},
        }));

    register_node(make_def("sentiment-analyzer", "Sentiment Analyzer", "AI", COL_AI,
        "Analyze sentiment from news/social media", 1, 1, {
            {"source", "Source", "options", std::string("news"),
             {"news", "twitter", "reddit", "sec_filings"}, true, "Data source for sentiment"},
            {"model", "Model", "options", std::string("finbert"),
             {"finbert", "vader", "gpt-4", "claude"}, false, "Sentiment model"},
            {"lookback_days", "Lookback (days)", "number", 7, {}, false, "Days of data to analyze"},
        }));

    // =========================================================================
    // ANALYTICS (7 — existing + enhanced)
    // =========================================================================
    register_node(make_def("backtest", "Backtest", "Analytics", COL_ANALYTICS,
        "Run backtests on trading strategies", 1, 1, {
            {"strategy", "Strategy", "string", std::string("SMA Crossover"), {}, true, "Strategy name"},
            {"initial_capital", "Initial Capital", "number", 100000.0, {}, false, "Starting capital"},
            {"commission", "Commission", "number", 0.001, {}, false, "Commission per trade"},
            {"start_date", "Start Date", "string", std::string("2024-01-01"), {}, false, "Backtest start"},
            {"end_date", "End Date", "string", std::string("2024-12-31"), {}, false, "Backtest end"},
        }));

    register_node(make_def("optimization", "Optimization", "Analytics", COL_ANALYTICS,
        "Optimize strategy parameters", 1, 1, {
            {"method", "Method", "options", std::string("grid"),
             {"grid", "genetic", "bayesian"}, true, "Optimization method"},
            {"objective", "Objective", "options", std::string("sharpe"),
             {"sharpe", "return", "max_drawdown", "sortino"}, false, "Optimization objective"},
            {"iterations", "Iterations", "number", 100, {}, false, "Max iterations"},
        }));

    register_node(make_def("portfolio-allocator", "Portfolio Allocator", "Analytics", COL_ANALYTICS,
        "Allocate weights across portfolio assets", 1, 1, {
            {"method", "Method", "options", std::string("equal_weight"),
             {"equal_weight", "mean_variance", "risk_parity", "black_litterman", "hrp"}, true, "Allocation method"},
            {"risk_free_rate", "Risk-Free Rate", "number", 0.05, {}, false, "Annual risk-free rate"},
            {"rebalance", "Rebalance Freq", "options", std::string("monthly"),
             {"daily", "weekly", "monthly", "quarterly"}, false, "Rebalance frequency"},
        }));

    register_node(make_def("correlation", "Correlation Matrix", "Analytics", COL_ANALYTICS,
        "Compute pairwise correlations between assets", 1, 1, {
            {"method", "Method", "options", std::string("pearson"),
             {"pearson", "spearman", "kendall"}, true, "Correlation method"},
            {"window", "Rolling Window", "number", 60, {}, false, "Rolling window (0 = full period)"},
        }));

    register_node(make_def("performance-metrics", "Performance Metrics", "Analytics", COL_ANALYTICS,
        "Calculate return, Sharpe, Sortino, etc.", 1, 1, {
            {"benchmark", "Benchmark", "string", std::string("SPY"), {}, false, "Benchmark symbol"},
            {"risk_free_rate", "Risk-Free Rate", "number", 0.05, {}, false, "Annual risk-free rate"},
        }));

    register_node(make_def("portfolio-optimization", "Portfolio Optimization", "Analytics", COL_ANALYTICS,
        "Optimize portfolio weights for best risk/return", 1, 1, {
            {"method", "Method", "options", std::string("mean_variance"),
             {"mean_variance", "min_volatility", "max_sharpe", "risk_parity", "hrp"}, true, "Optimization method"},
            {"constraints", "Constraints", "string", std::string("long_only"), {}, false, "Constraints"},
        }));

    register_node(make_def("technical-indicators", "Technical Indicators", "Analytics", COL_ANALYTICS,
        "Calculate multiple indicators at once", 1, 1, {
            {"indicators", "Indicators", "string", std::string("SMA,EMA,RSI,MACD"), {}, true, "Comma-separated indicator list"},
            {"period", "Period", "number", 14, {}, false, "Default period"},
        }));

    // =========================================================================
    // RISK (4 — existing + enhanced)
    // =========================================================================
    register_node(make_def("risk-metrics", "Risk Metrics", "Risk", COL_RISK,
        "Calculate VaR, CVaR, drawdown, and other risk measures", 1, 1, {
            {"metric", "Metric", "options", std::string("VaR"),
             {"VaR", "CVaR", "max_drawdown", "sharpe", "sortino", "calmar", "beta", "alpha"}, true, "Risk metric"},
            {"confidence", "Confidence", "number", 0.95, {}, false, "Confidence level (0-1)"},
            {"window", "Window", "number", 252, {}, false, "Lookback window (days)"},
        }));

    register_node(make_def("monte-carlo", "Monte Carlo Sim", "Risk", COL_RISK,
        "Run Monte Carlo simulations for risk estimation", 1, 1, {
            {"simulations", "Simulations", "number", 10000, {}, true, "Number of simulation paths"},
            {"horizon", "Horizon (days)", "number", 252, {}, false, "Forecast horizon"},
            {"distribution", "Distribution", "options", std::string("normal"),
             {"normal", "t_student", "historical"}, false, "Return distribution"},
        }));

    register_node(make_def("stress-test", "Stress Test", "Risk", COL_RISK,
        "Apply historical or hypothetical stress scenarios", 1, 1, {
            {"scenario", "Scenario", "options", std::string("2008_gfc"),
             {"2008_gfc", "2020_covid", "dot_com", "custom", "rate_shock_200bp", "fx_crash_10pct"}, true, "Stress scenario"},
            {"shock_pct", "Custom Shock %", "number", -20.0, {}, false, "Custom shock magnitude"},
            {"recovery_days", "Recovery (days)", "number", 60, {}, false, "Simulated recovery period"},
        }));

    register_node(make_def("risk-analysis", "Risk Analysis", "Risk", COL_RISK,
        "Comprehensive risk analysis report", 1, 1, {
            {"metrics", "Metrics", "string", std::string("VaR,CVaR,MaxDD,Sharpe"), {}, true, "Comma-separated metrics"},
            {"confidence", "Confidence", "number", 0.95, {}, false, "Confidence level"},
        }));

    // =========================================================================
    // TRADING (8 — existing signal/order + 6 new)
    // =========================================================================
    register_node(make_def("signal-generator", "Signal Generator", "Trading", COL_TRADING,
        "Generate buy/sell signals from indicators", 1, 1, {
            {"strategy", "Strategy", "options", std::string("crossover"),
             {"crossover", "threshold", "mean_reversion", "momentum", "breakout"}, true, "Signal strategy"},
            {"fast_period", "Fast Period", "number", 10, {}, false, "Fast MA period"},
            {"slow_period", "Slow Period", "number", 50, {}, false, "Slow MA period"},
            {"threshold", "Threshold", "number", 0.02, {}, false, "Signal threshold"},
        }));

    register_node(make_def("order-executor", "Order Executor", "Trading", COL_TRADING,
        "Execute paper/live orders based on signals", 1, 1, {
            {"mode", "Mode", "options", std::string("paper"),
             {"paper", "live"}, true, "Execution mode"},
            {"order_type", "Order Type", "options", std::string("market"),
             {"market", "limit", "stop", "stop_limit"}, false, "Order type"},
            {"position_size", "Position Size %", "number", 10.0, {}, false, "Position size"},
            {"slippage", "Slippage %", "number", 0.1, {}, false, "Expected slippage"},
        }));

    register_node(make_def("place-order", "Place Order", "Trading", COL_TRADING,
        "Place a new order", 1, 1, {
            {"symbol", "Symbol", "string", std::string("AAPL"), {}, true, "Ticker symbol"},
            {"side", "Side", "options", std::string("buy"), {"buy", "sell"}, true, "Order side"},
            {"qty", "Quantity", "number", 100, {}, true, "Order quantity"},
            {"order_type", "Order Type", "options", std::string("market"),
             {"market", "limit", "stop", "stop_limit"}, false, "Order type"},
            {"price", "Limit Price", "number", 0.0, {}, false, "Limit/stop price"},
        }));

    register_node(make_def("cancel-order", "Cancel Order", "Trading", COL_TRADING,
        "Cancel an existing order", 1, 1, {
            {"order_id", "Order ID", "string", std::string(""), {}, true, "Order ID to cancel"},
        }));

    register_node(make_def("modify-order", "Modify Order", "Trading", COL_TRADING,
        "Modify an existing order", 1, 1, {
            {"order_id", "Order ID", "string", std::string(""), {}, true, "Order ID"},
            {"qty", "New Quantity", "number", 0, {}, false, "New quantity"},
            {"price", "New Price", "number", 0.0, {}, false, "New price"},
        }));

    register_node(make_def("close-position", "Close Position", "Trading", COL_TRADING,
        "Close an open position", 1, 1, {
            {"symbol", "Symbol", "string", std::string("AAPL"), {}, true, "Position symbol"},
            {"pct", "Close %", "number", 100.0, {}, false, "Percentage to close"},
        }));

    register_node(make_def("get-balance", "Get Balance", "Trading", COL_TRADING,
        "Get account balance and buying power", 0, 1, {
            {"broker", "Broker", "options", std::string("paper"),
             {"paper", "kraken", "hyperliquid"}, false, "Broker"},
        }));

    register_node(make_def("get-positions", "Get Positions", "Trading", COL_TRADING,
        "Get current open positions", 0, 1, {
            {"broker", "Broker", "options", std::string("paper"),
             {"paper", "kraken", "hyperliquid"}, false, "Broker"},
        }));

    // =========================================================================
    // CONTROL FLOW (8)
    // =========================================================================
    register_node(make_def("if-else", "If/Else", "Control Flow", COL_CONTROL,
        "Branch based on condition", 1, 2, {
            {"condition", "Condition", "string", std::string("value > 0"), {}, true, "Boolean expression"},
        }));

    register_node(make_def("switch", "Switch", "Control Flow", COL_CONTROL,
        "Route to multiple outputs based on value", 1, 4, {
            {"field", "Field", "string", std::string("status"), {}, true, "Field to switch on"},
            {"cases", "Cases", "string", std::string("buy,sell,hold,error"), {}, true, "Comma-separated case values"},
        }));

    register_node(make_def("merge", "Merge", "Control Flow", COL_CONTROL,
        "Merge multiple inputs into one", 2, 1, {
            {"mode", "Mode", "options", std::string("wait_all"),
             {"wait_all", "first", "append"}, false, "Merge mode"},
        }));

    register_node(make_def("split", "Split", "Control Flow", COL_CONTROL,
        "Split data into multiple outputs", 1, 2, {
            {"split_on", "Split On", "string", std::string("symbol"), {}, true, "Column to split on"},
        }));

    register_node(make_def("loop", "Loop", "Control Flow", COL_CONTROL,
        "Iterate over items in input", 1, 1, {
            {"max_iterations", "Max Iterations", "number", 100, {}, false, "Max loop iterations"},
        }));

    register_node(make_def("wait", "Wait", "Control Flow", COL_CONTROL,
        "Wait for a duration before continuing", 1, 1, {
            {"duration_ms", "Duration (ms)", "number", 1000, {}, true, "Wait duration in milliseconds"},
        }));

    register_node(make_def("error-handler", "Error Handler", "Control Flow", COL_CONTROL,
        "Handle errors from upstream nodes", 1, 2, {
            {"action", "Action", "options", std::string("continue"),
             {"continue", "stop", "retry"}, true, "Error action"},
            {"max_retries", "Max Retries", "number", 3, {}, false, "Max retry attempts"},
        }));

    register_node(make_def("execute-workflow", "Execute Workflow", "Control Flow", COL_CONTROL,
        "Execute another workflow as a sub-workflow", 1, 1, {
            {"workflow_id", "Workflow ID", "string", std::string(""), {}, true, "Workflow ID to execute"},
        }));

    // =========================================================================
    // SAFETY (4)
    // =========================================================================
    register_node(make_def("risk-check", "Risk Check", "Safety", COL_SAFETY,
        "Validate trade against risk parameters", 1, 1, {
            {"max_loss_pct", "Max Loss %", "number", 2.0, {}, true, "Maximum loss percentage"},
            {"risk_threshold", "Risk Threshold", "number", 0.05, {}, false, "Risk threshold"},
        }));

    register_node(make_def("loss-limit", "Loss Limit", "Safety", COL_SAFETY,
        "Check if loss exceeds daily/total limits", 1, 1, {
            {"daily_limit", "Daily Limit %", "number", 5.0, {}, true, "Maximum daily loss %"},
            {"total_limit", "Total Limit %", "number", 15.0, {}, false, "Maximum total loss %"},
        }));

    register_node(make_def("position-size-limit", "Position Size Limit", "Safety", COL_SAFETY,
        "Validate position size against limits", 1, 1, {
            {"max_position_pct", "Max Position %", "number", 20.0, {}, true, "Max position as % of portfolio"},
            {"max_total_exposure", "Max Exposure %", "number", 100.0, {}, false, "Max total exposure"},
        }));

    register_node(make_def("trading-hours-check", "Trading Hours Check", "Safety", COL_SAFETY,
        "Verify within trading hours", 1, 1, {
            {"exchange", "Exchange", "options", std::string("NYSE"),
             {"NYSE", "NASDAQ", "LSE", "TSE", "24_7"}, true, "Exchange"},
            {"allow_premarket", "Allow Pre-Market", "boolean", false, {}, false, "Allow pre-market trading"},
            {"allow_afterhours", "Allow After-Hours", "boolean", false, {}, false, "Allow after-hours trading"},
        }));

    // =========================================================================
    // NOTIFICATIONS (6)
    // =========================================================================
    register_node(make_def("email-notify", "Email", "Notifications", COL_NOTIFY,
        "Send email notification", 1, 0, {
            {"recipient", "Recipient", "string", std::string(""), {}, true, "Email address"},
            {"subject", "Subject", "string", std::string("Workflow Alert"), {}, true, "Email subject"},
            {"message_template", "Message", "string", std::string("{{result}}"), {}, false, "Message template"},
        }));

    register_node(make_def("slack-notify", "Slack", "Notifications", COL_NOTIFY,
        "Send Slack message", 1, 0, {
            {"webhook_url", "Webhook URL", "string", std::string(""), {}, true, "Slack webhook URL"},
            {"channel", "Channel", "string", std::string("#alerts"), {}, false, "Slack channel"},
            {"message_template", "Message", "string", std::string("{{result}}"), {}, false, "Message template"},
        }));

    register_node(make_def("discord-notify", "Discord", "Notifications", COL_NOTIFY,
        "Send Discord message", 1, 0, {
            {"webhook_url", "Webhook URL", "string", std::string(""), {}, true, "Discord webhook URL"},
            {"message_template", "Message", "string", std::string("{{result}}"), {}, false, "Message template"},
        }));

    register_node(make_def("telegram-notify", "Telegram", "Notifications", COL_NOTIFY,
        "Send Telegram message", 1, 0, {
            {"bot_token", "Bot Token", "string", std::string(""), {}, true, "Telegram bot token"},
            {"chat_id", "Chat ID", "string", std::string(""), {}, true, "Telegram chat ID"},
            {"message_template", "Message", "string", std::string("{{result}}"), {}, false, "Message template"},
        }));

    register_node(make_def("sms-notify", "SMS", "Notifications", COL_NOTIFY,
        "Send SMS notification", 1, 0, {
            {"phone", "Phone Number", "string", std::string(""), {}, true, "Phone number"},
            {"message_template", "Message", "string", std::string("{{result}}"), {}, false, "Message template"},
        }));

    register_node(make_def("webhook-notify", "Webhook Notification", "Notifications", COL_NOTIFY,
        "Send data to a webhook URL", 1, 0, {
            {"url", "Webhook URL", "string", std::string(""), {}, true, "Target URL"},
            {"method", "Method", "options", std::string("POST"), {"GET", "POST", "PUT"}, false, "HTTP method"},
            {"headers", "Headers", "string", std::string("Content-Type: application/json"), {}, false, "Custom headers"},
        }));

    // =========================================================================
    // OUTPUT (3 — existing)
    // =========================================================================
    register_node(make_def("results-display", "Results Display", "Output", COL_OUTPUT,
        "Display workflow results with formatting", 1, 0, {
            {"format", "Format", "options", std::string("table"),
             {"table", "chart", "json", "text"}, false, "Output format"},
            {"title", "Title", "string", std::string("Results"), {}, false, "Display title"},
        }));

    register_node(make_def("chart-output", "Chart Output", "Output", COL_OUTPUT,
        "Render data as interactive chart", 1, 0, {
            {"chart_type", "Chart Type", "options", std::string("candlestick"),
             {"candlestick", "line", "bar", "scatter", "heatmap", "histogram"}, true, "Chart type"},
            {"title", "Title", "string", std::string("Chart"), {}, false, "Chart title"},
            {"show_volume", "Show Volume", "boolean", true, {}, false, "Show volume subplot"},
        }));

    register_node(make_def("csv-export", "CSV Export", "Output", COL_OUTPUT,
        "Export results to CSV file", 1, 0, {
            {"filename", "Filename", "string", std::string("output.csv"), {}, true, "Output filename"},
            {"delimiter", "Delimiter", "options", std::string(","),
             {",", ";", "\\t", "|"}, false, "Column separator"},
            {"include_header", "Include Header", "boolean", true, {}, false, "Include column headers"},
        }));

    // =========================================================================
    // UTILITIES (6)
    // =========================================================================
    register_node(make_def("http-request", "HTTP Request", "Utilities", COL_UTILITY,
        "Make arbitrary HTTP requests", 0, 1, {
            {"url", "URL", "string", std::string("https://api.example.com/data"), {}, true, "Request URL"},
            {"method", "Method", "options", std::string("GET"),
             {"GET", "POST", "PUT", "DELETE", "PATCH"}, false, "HTTP method"},
            {"body", "Body", "string", std::string(""), {}, false, "Request body (JSON)"},
            {"headers", "Headers", "string", std::string(""), {}, false, "Custom headers (key: value per line)"},
        }));

    register_node(make_def("date-time", "Date/Time", "Utilities", COL_UTILITY,
        "Get or manipulate dates and times", 0, 1, {
            {"operation", "Operation", "options", std::string("now"),
             {"now", "format", "add", "subtract", "diff"}, true, "Operation"},
            {"format", "Format", "string", std::string("%Y-%m-%d"), {}, false, "Date format string"},
            {"value", "Value", "string", std::string(""), {}, false, "Input date or duration"},
        }));

    register_node(make_def("crypto-util", "Crypto", "Utilities", COL_UTILITY,
        "Hash, encrypt, or encode data", 1, 1, {
            {"operation", "Operation", "options", std::string("sha256"),
             {"sha256", "md5", "base64_encode", "base64_decode", "hmac"}, true, "Crypto operation"},
            {"key", "Key", "string", std::string(""), {}, false, "Secret key (for HMAC)"},
        }));

    register_node(make_def("item-lists", "Item Lists", "Utilities", COL_UTILITY,
        "Manipulate lists/arrays of items", 1, 1, {
            {"operation", "Operation", "options", std::string("split"),
             {"split", "merge", "unique", "flatten", "chunk"}, true, "List operation"},
            {"chunk_size", "Chunk Size", "number", 10, {}, false, "Chunk size (for chunk op)"},
        }));

    register_node(make_def("limit", "Limit", "Utilities", COL_UTILITY,
        "Limit number of items passed through", 1, 1, {
            {"max_items", "Max Items", "number", 100, {}, true, "Maximum items to pass"},
        }));

    register_node(make_def("rss-read", "RSS Reader", "Utilities", COL_UTILITY,
        "Read and parse RSS/Atom feeds", 0, 1, {
            {"url", "Feed URL", "string", std::string(""), {}, true, "RSS feed URL"},
            {"max_items", "Max Items", "number", 20, {}, false, "Max items to read"},
        }));

    // =========================================================================
    // CORE (2)
    // =========================================================================
    register_node(make_def("code", "Code", "Core", COL_UTILITY,
        "Execute custom expression/code", 1, 1, {
            {"language", "Language", "options", std::string("javascript"),
             {"javascript", "python", "expression"}, true, "Code language"},
            {"code", "Code", "string", std::string("return input;"), {}, true, "Code to execute"},
        }));

    register_node(make_def("set-variable", "Set Variable", "Core", COL_UTILITY,
        "Set a variable value for the workflow", 0, 1, {
            {"name", "Name", "string", std::string("myVar"), {}, true, "Variable name"},
            {"value", "Value", "string", std::string(""), {}, true, "Variable value"},
        }));

    // =========================================================================
    // DATA (4)
    // =========================================================================
    register_node(make_def("json-node", "JSON", "Data", COL_DATA,
        "Parse, create, or transform JSON", 1, 1, {
            {"operation", "Operation", "options", std::string("parse"),
             {"parse", "stringify", "extract", "merge"}, true, "JSON operation"},
            {"path", "JSON Path", "string", std::string("$.data"), {}, false, "JSON path expression"},
        }));

    register_node(make_def("xml-node", "XML", "Data", COL_DATA,
        "Parse or transform XML data", 1, 1, {
            {"operation", "Operation", "options", std::string("parse"),
             {"parse", "to_json", "xpath"}, true, "XML operation"},
            {"xpath", "XPath", "string", std::string("//item"), {}, false, "XPath expression"},
        }));

    register_node(make_def("html-extract", "HTML Extract", "Data", COL_DATA,
        "Extract data from HTML pages", 1, 1, {
            {"selector", "CSS Selector", "string", std::string("table.data"), {}, true, "CSS selector"},
            {"attribute", "Attribute", "string", std::string("text"), {}, false, "Attribute to extract"},
        }));

    register_node(make_def("compare-datasets", "Compare Datasets", "Data", COL_DATA,
        "Compare two datasets and find differences", 2, 1, {
            {"key", "Key Column", "string", std::string("id"), {}, true, "Comparison key"},
            {"mode", "Mode", "options", std::string("diff"),
             {"diff", "intersect", "union"}, false, "Comparison mode"},
        }));

    // =========================================================================
    // FILES (5)
    // =========================================================================
    register_node(make_def("file-operations", "File Operations", "Files", COL_FILES,
        "Read, write, or manipulate files", 0, 1, {
            {"operation", "Operation", "options", std::string("read"),
             {"read", "write", "append", "delete", "list"}, true, "File operation"},
            {"path", "Path", "string", std::string(""), {}, true, "File path"},
            {"content", "Content", "string", std::string(""), {}, false, "Content to write"},
        }));

    register_node(make_def("spreadsheet-file", "Spreadsheet File", "Files", COL_FILES,
        "Read/write Excel/CSV files", 0, 1, {
            {"operation", "Operation", "options", std::string("read"),
             {"read", "write"}, true, "Operation"},
            {"path", "Path", "string", std::string("data.xlsx"), {}, true, "File path"},
            {"sheet", "Sheet", "string", std::string("Sheet1"), {}, false, "Sheet name"},
        }));

    register_node(make_def("binary-file", "Binary File", "Files", COL_FILES,
        "Read/write binary files", 0, 1, {
            {"operation", "Operation", "options", std::string("read"),
             {"read", "write"}, true, "Operation"},
            {"path", "Path", "string", std::string(""), {}, true, "File path"},
        }));

    register_node(make_def("compress", "Compress", "Files", COL_FILES,
        "Compress or decompress files", 1, 1, {
            {"operation", "Operation", "options", std::string("compress"),
             {"compress", "decompress"}, true, "Operation"},
            {"format", "Format", "options", std::string("zip"),
             {"zip", "gzip", "tar"}, false, "Compression format"},
        }));

    register_node(make_def("convert-to-file", "Convert to File", "Files", COL_FILES,
        "Convert data to a downloadable file", 1, 1, {
            {"format", "Format", "options", std::string("csv"),
             {"csv", "json", "xlsx", "pdf"}, true, "Output format"},
            {"filename", "Filename", "string", std::string("export"), {}, false, "Output filename"},
        }));

    initialized_ = true;
    LOG_INFO("NodeRegistry", "Registered %zu nodes in %zu categories",
             defs_.size(), categories_.size());
}

} // namespace fincept::node_editor
