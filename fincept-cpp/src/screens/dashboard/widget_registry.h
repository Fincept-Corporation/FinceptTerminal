#pragma once
// Widget type system and layout persistence for the dashboard
// Supports adding/removing widgets, drag-resize, and SQLite persistence
// Mirrors React fincept-terminal-desktop widget types 1:1

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace fincept::dashboard {

// All available widget types — matches React WidgetType union exactly
enum class WidgetType {
    // === Original 12 ===
    GlobalIndices,
    Forex,
    Commodities,
    Crypto,
    SectorHeatmap,
    News,
    EconomicIndicators,
    GeopoliticalRisk,
    Performance,
    TopMovers,
    MarketData,
    VideoPlayer,        // was YouTubeStream, renamed to match React

    // === New 20 — migrated from React ===
    Watchlist,
    Portfolio,
    StockQuote,
    EconomicCalendar,
    MarketSentiment,
    QuickTrade,
    Notes,
    Screener,
    RiskMetrics,
    AlgoStatus,
    AlphaArena,
    BacktestSummary,
    WatchlistAlerts,
    LiveSignals,
    DBnomics,
    AkShare,
    Maritime,
    Polymarket,
    Forum,
    DataSource,

    Count
};

inline const char* widget_type_name(WidgetType t) {
    switch (t) {
        case WidgetType::GlobalIndices:      return "Global Indices";
        case WidgetType::Forex:              return "Forex";
        case WidgetType::Commodities:        return "Commodities";
        case WidgetType::Crypto:             return "Crypto";
        case WidgetType::SectorHeatmap:      return "Sector Heatmap";
        case WidgetType::News:               return "Market News";
        case WidgetType::EconomicIndicators: return "Economic Indicators";
        case WidgetType::GeopoliticalRisk:   return "Geopolitical Risk";
        case WidgetType::Performance:        return "Performance Tracker";
        case WidgetType::TopMovers:          return "Top Movers";
        case WidgetType::MarketData:         return "Market Data";
        case WidgetType::VideoPlayer:        return "Video Player";
        case WidgetType::Watchlist:          return "Watchlist";
        case WidgetType::Portfolio:          return "Portfolio Summary";
        case WidgetType::StockQuote:         return "Stock Quote";
        case WidgetType::EconomicCalendar:   return "Economic Calendar";
        case WidgetType::MarketSentiment:    return "Market Sentiment";
        case WidgetType::QuickTrade:         return "Quick Trade";
        case WidgetType::Notes:              return "Recent Notes";
        case WidgetType::Screener:           return "Stock Screener";
        case WidgetType::RiskMetrics:        return "Risk Metrics";
        case WidgetType::AlgoStatus:         return "Algo Status";
        case WidgetType::AlphaArena:         return "Alpha Arena";
        case WidgetType::BacktestSummary:    return "Backtest Summary";
        case WidgetType::WatchlistAlerts:    return "Watchlist Alerts";
        case WidgetType::LiveSignals:        return "Live AI Signals";
        case WidgetType::DBnomics:           return "DBnomics Data";
        case WidgetType::AkShare:            return "China Markets";
        case WidgetType::Maritime:           return "Maritime Sector";
        case WidgetType::Polymarket:         return "Polymarket";
        case WidgetType::Forum:              return "Forum Posts";
        case WidgetType::DataSource:         return "Data Source";
        default: return "Unknown";
    }
}

inline const char* widget_type_id(WidgetType t) {
    switch (t) {
        case WidgetType::GlobalIndices:      return "indices";
        case WidgetType::Forex:              return "forex";
        case WidgetType::Commodities:        return "commodities";
        case WidgetType::Crypto:             return "crypto";
        case WidgetType::SectorHeatmap:      return "heatmap";
        case WidgetType::News:               return "news";
        case WidgetType::EconomicIndicators: return "economic";
        case WidgetType::GeopoliticalRisk:   return "geopolitics";
        case WidgetType::Performance:        return "performance";
        case WidgetType::TopMovers:          return "topmovers";
        case WidgetType::MarketData:         return "marketdata";
        case WidgetType::VideoPlayer:        return "videoplayer";
        case WidgetType::Watchlist:          return "watchlist";
        case WidgetType::Portfolio:          return "portfolio";
        case WidgetType::StockQuote:         return "stockquote";
        case WidgetType::EconomicCalendar:   return "calendar";
        case WidgetType::MarketSentiment:    return "sentiment";
        case WidgetType::QuickTrade:         return "quicktrade";
        case WidgetType::Notes:              return "notes";
        case WidgetType::Screener:           return "screener";
        case WidgetType::RiskMetrics:        return "riskmetrics";
        case WidgetType::AlgoStatus:         return "algostatus";
        case WidgetType::AlphaArena:         return "alphaarena";
        case WidgetType::BacktestSummary:    return "backtestsummary";
        case WidgetType::WatchlistAlerts:    return "watchlistalerts";
        case WidgetType::LiveSignals:        return "livesignals";
        case WidgetType::DBnomics:           return "dbnomics";
        case WidgetType::AkShare:            return "akshare";
        case WidgetType::Maritime:           return "maritime";
        case WidgetType::Polymarket:         return "polymarket";
        case WidgetType::Forum:              return "forum";
        case WidgetType::DataSource:         return "datasource";
        default: return "unknown";
    }
}

inline WidgetType widget_type_from_id(const std::string& id) {
    static const std::unordered_map<std::string, WidgetType> lookup = {
        {"indices",         WidgetType::GlobalIndices},
        {"forex",           WidgetType::Forex},
        {"commodities",     WidgetType::Commodities},
        {"crypto",          WidgetType::Crypto},
        {"heatmap",         WidgetType::SectorHeatmap},
        {"news",            WidgetType::News},
        {"economic",        WidgetType::EconomicIndicators},
        {"geopolitics",     WidgetType::GeopoliticalRisk},
        {"performance",     WidgetType::Performance},
        {"topmovers",       WidgetType::TopMovers},
        {"marketdata",      WidgetType::MarketData},
        {"videoplayer",     WidgetType::VideoPlayer},
        {"youtube",         WidgetType::VideoPlayer},
        {"watchlist",       WidgetType::Watchlist},
        {"portfolio",       WidgetType::Portfolio},
        {"stockquote",      WidgetType::StockQuote},
        {"calendar",        WidgetType::EconomicCalendar},
        {"sentiment",       WidgetType::MarketSentiment},
        {"quicktrade",      WidgetType::QuickTrade},
        {"notes",           WidgetType::Notes},
        {"screener",        WidgetType::Screener},
        {"riskmetrics",     WidgetType::RiskMetrics},
        {"algostatus",      WidgetType::AlgoStatus},
        {"alphaarena",      WidgetType::AlphaArena},
        {"backtestsummary", WidgetType::BacktestSummary},
        {"watchlistalerts", WidgetType::WatchlistAlerts},
        {"livesignals",     WidgetType::LiveSignals},
        {"dbnomics",        WidgetType::DBnomics},
        {"akshare",         WidgetType::AkShare},
        {"maritime",        WidgetType::Maritime},
        {"polymarket",      WidgetType::Polymarket},
        {"forum",           WidgetType::Forum},
        {"datasource",      WidgetType::DataSource},
    };
    auto it = lookup.find(id);
    return (it != lookup.end()) ? it->second : WidgetType::GlobalIndices;
}

// Widget category — for the Add Widget modal grouping
enum class WidgetCategory {
    Markets,
    Trading,
    Portfolio_Risk,
    Research,
    DataFeeds,
    Tools,
};

inline const char* widget_category_name(WidgetCategory c) {
    switch (c) {
        case WidgetCategory::Markets:        return "Markets";
        case WidgetCategory::Trading:        return "Trading";
        case WidgetCategory::Portfolio_Risk:  return "Portfolio & Risk";
        case WidgetCategory::Research:        return "Research";
        case WidgetCategory::DataFeeds:       return "Data Feeds";
        case WidgetCategory::Tools:           return "Tools";
        default: return "Other";
    }
}

inline WidgetCategory widget_category_for(WidgetType t) {
    switch (t) {
        case WidgetType::GlobalIndices:
        case WidgetType::Forex:
        case WidgetType::Commodities:
        case WidgetType::Crypto:
        case WidgetType::SectorHeatmap:
        case WidgetType::TopMovers:
        case WidgetType::MarketData:
        case WidgetType::StockQuote:
        case WidgetType::MarketSentiment:
            return WidgetCategory::Markets;

        case WidgetType::QuickTrade:
        case WidgetType::AlgoStatus:
        case WidgetType::LiveSignals:
        case WidgetType::AlphaArena:
        case WidgetType::BacktestSummary:
            return WidgetCategory::Trading;

        case WidgetType::Portfolio:
        case WidgetType::Performance:
        case WidgetType::RiskMetrics:
        case WidgetType::Watchlist:
        case WidgetType::WatchlistAlerts:
            return WidgetCategory::Portfolio_Risk;

        case WidgetType::News:
        case WidgetType::Screener:
        case WidgetType::EconomicIndicators:
        case WidgetType::EconomicCalendar:
        case WidgetType::GeopoliticalRisk:
            return WidgetCategory::Research;

        case WidgetType::DBnomics:
        case WidgetType::AkShare:
        case WidgetType::Maritime:
        case WidgetType::Polymarket:
        case WidgetType::DataSource:
        case WidgetType::Forum:
            return WidgetCategory::DataFeeds;

        case WidgetType::Notes:
        case WidgetType::VideoPlayer:
            return WidgetCategory::Tools;

        default: return WidgetCategory::Markets;
    }
}

// A single widget instance in the grid
struct WidgetInstance {
    std::string id;       // unique instance id
    WidgetType type;
    std::string title;    // user-customizable title
    int col = 0;          // grid column (0-based)
    int row = 0;          // grid row (0-based)
    int col_span = 1;     // how many columns wide
    int row_span = 1;     // how many rows tall
    bool visible = true;
    nlohmann::json config; // per-widget config (symbol, preset, country, etc.)

    nlohmann::json to_json() const {
        nlohmann::json j = {
            {"id", id}, {"type", widget_type_id(type)}, {"title", title},
            {"col", col}, {"row", row}, {"col_span", col_span}, {"row_span", row_span},
            {"visible", visible}
        };
        if (!config.is_null() && !config.empty()) j["config"] = config;
        return j;
    }

    static WidgetInstance from_json(const nlohmann::json& j) {
        WidgetInstance w;
        w.id = j.value("id", "");
        w.type = widget_type_from_id(j.value("type", "indices"));
        w.title = j.value("title", "");
        w.col = j.value("col", 0);
        w.row = j.value("row", 0);
        w.col_span = j.value("col_span", 1);
        w.row_span = j.value("row_span", 1);
        w.visible = j.value("visible", true);
        if (j.contains("config")) w.config = j["config"];
        if (w.title.empty()) w.title = widget_type_name(w.type);
        return w;
    }

    // Config helpers
    std::string cfg_string(const char* key, const char* def = "") const {
        if (config.is_object() && config.contains(key) && config[key].is_string())
            return config[key].get<std::string>();
        return def;
    }
    int cfg_int(const char* key, int def = 0) const {
        if (config.is_object() && config.contains(key) && config[key].is_number_integer())
            return config[key].get<int>();
        return def;
    }
    double cfg_double(const char* key, double def = 0.0) const {
        if (config.is_object() && config.contains(key) && config[key].is_number())
            return config[key].get<double>();
        return def;
    }
};

// Complete dashboard layout
struct DashboardLayout {
    std::string template_id = "default";
    int grid_cols = 3;
    int grid_rows = 3;
    bool market_pulse_open = true;
    std::vector<WidgetInstance> widgets;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["template_id"] = template_id;
        j["grid_cols"] = grid_cols;
        j["grid_rows"] = grid_rows;
        j["market_pulse_open"] = market_pulse_open;
        j["widgets"] = nlohmann::json::array();
        for (auto& w : widgets) j["widgets"].push_back(w.to_json());
        return j;
    }

    static DashboardLayout from_json(const nlohmann::json& j) {
        DashboardLayout l;
        l.template_id = j.value("template_id", "default");
        l.grid_cols = j.value("grid_cols", 3);
        l.grid_rows = j.value("grid_rows", 3);
        l.market_pulse_open = j.value("market_pulse_open", true);
        if (j.contains("widgets") && j["widgets"].is_array()) {
            for (auto& wj : j["widgets"]) {
                l.widgets.push_back(WidgetInstance::from_json(wj));
            }
        }
        return l;
    }
};

// Helper to make a WidgetInstance with config
inline WidgetInstance make_widget(const std::string& id, WidgetType type,
                                  const std::string& title, int col, int row,
                                  int cs = 1, int rs = 1,
                                  nlohmann::json cfg = {}) {
    WidgetInstance w;
    w.id = id; w.type = type; w.title = title;
    w.col = col; w.row = row; w.col_span = cs; w.row_span = rs;
    w.visible = true; w.config = std::move(cfg);
    return w;
}

// =============================================================================
// Templates — matches React dashboardTemplates.ts
// =============================================================================

// Template 1: Default Terminal (original)
inline DashboardLayout default_layout() {
    DashboardLayout l;
    l.template_id = "default";
    l.grid_cols = 3; l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        make_widget("w-idx",  WidgetType::GlobalIndices,     "GLOBAL INDICES",      0,0),
        make_widget("w-heat", WidgetType::SectorHeatmap,     "SECTOR HEATMAP",      1,0),
        make_widget("w-perf", WidgetType::Performance,       "PERFORMANCE",         2,0),
        make_widget("w-fx",   WidgetType::Forex,             "FOREX",               0,1),
        make_widget("w-cmd",  WidgetType::Commodities,       "COMMODITIES",         1,1),
        make_widget("w-cry",  WidgetType::Crypto,            "CRYPTO",              2,1),
        make_widget("w-news", WidgetType::News,              "MARKET NEWS",         0,2),
        make_widget("w-eco",  WidgetType::EconomicIndicators,"ECONOMIC INDICATORS", 1,2),
        make_widget("w-geo",  WidgetType::GeopoliticalRisk,  "GEOPOLITICAL RISK",   2,2),
    };
    return l;
}

// Template 2: Portfolio Manager
inline DashboardLayout portfolio_manager_layout() {
    DashboardLayout l;
    l.template_id = "portfolio-manager";
    l.grid_cols = 3; l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        make_widget("w-perf",   WidgetType::Performance,       "PERFORMANCE TRACKER",   0,0),
        make_widget("w-port",   WidgetType::Portfolio,         "PORTFOLIO SUMMARY",     1,0),
        make_widget("w-risk",   WidgetType::RiskMetrics,       "RISK METRICS",          2,0),
        make_widget("w-idx",    WidgetType::GlobalIndices,     "GLOBAL INDICES",        0,1),
        make_widget("w-fx",     WidgetType::Forex,             "FOREX",                 1,1),
        make_widget("w-cal",    WidgetType::EconomicCalendar,  "ECONOMIC CALENDAR",     2,1, 1,1, {{"country","US"},{"limit",10}}),
        make_widget("w-news",   WidgetType::News,              "MARKET NEWS",           0,2),
        make_widget("w-eco",    WidgetType::EconomicIndicators,"ECONOMIC INDICATORS",   1,2),
        make_widget("w-alerts", WidgetType::WatchlistAlerts,   "WATCHLIST ALERTS",      2,2, 1,1, {{"alertThreshold",3}}),
    };
    return l;
}

// Template 3: Hedge Fund Manager
inline DashboardLayout hedge_fund_layout() {
    DashboardLayout l;
    l.template_id = "hedge-fund";
    l.grid_cols = 3; l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        make_widget("w-alpha",   WidgetType::AlphaArena,       "ALPHA ARENA",           0,0),
        make_widget("w-signals", WidgetType::LiveSignals,      "LIVE AI SIGNALS",       1,0),
        make_widget("w-risk",    WidgetType::RiskMetrics,      "RISK METRICS",          2,0),
        make_widget("w-bt",      WidgetType::BacktestSummary,  "BACKTEST SUMMARY",      0,1),
        make_widget("w-sent",    WidgetType::MarketSentiment,  "MARKET SENTIMENT",      1,1),
        make_widget("w-movers",  WidgetType::TopMovers,        "TOP MOVERS",            2,1),
        make_widget("w-news",    WidgetType::News,             "MARKETS NEWS",          0,2),
        make_widget("w-geo",     WidgetType::GeopoliticalRisk, "GEOPOLITICAL RISK",     1,2),
        make_widget("w-screen",  WidgetType::Screener,         "STOCK SCREENER",        2,2, 1,1, {{"screenerPreset","momentum"}}),
    };
    return l;
}

// Template 4: Crypto Trader
inline DashboardLayout crypto_trader_layout() {
    DashboardLayout l;
    l.template_id = "crypto-trader";
    l.grid_cols = 3; l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        make_widget("w-cry",   WidgetType::Crypto,           "CRYPTOCURRENCY MARKETS", 0,0, 2,1),
        make_widget("w-sent",  WidgetType::MarketSentiment,  "MARKET SENTIMENT",       2,0),
        make_widget("w-poly",  WidgetType::Polymarket,       "POLYMARKET PREDICTIONS", 0,1),
        make_widget("w-qt",    WidgetType::QuickTrade,       "QUICK TRADE",            1,1),
        make_widget("w-watch", WidgetType::Watchlist,        "MY WATCHLIST",           2,1, 1,1, {{"watchlistName","Default"}}),
        make_widget("w-news",  WidgetType::News,             "CRYPTO NEWS",            0,2, 2,1),
        make_widget("w-fx",    WidgetType::Forex,            "FOREX",                  2,2),
    };
    return l;
}

// Template 5: Equity Trader
inline DashboardLayout equity_trader_layout() {
    DashboardLayout l;
    l.template_id = "equity-trader";
    l.grid_cols = 3; l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        make_widget("w-quote",  WidgetType::StockQuote,      "STOCK QUOTE",          0,0, 1,1, {{"stockSymbol","AAPL"}}),
        make_widget("w-movers", WidgetType::TopMovers,       "TOP MOVERS",           1,0),
        make_widget("w-qt",     WidgetType::QuickTrade,      "QUICK TRADE",          2,0),
        make_widget("w-watch",  WidgetType::Watchlist,       "MY WATCHLIST",         0,1, 1,1, {{"watchlistName","Default"}}),
        make_widget("w-alerts", WidgetType::WatchlistAlerts, "WATCHLIST ALERTS",     1,1, 1,1, {{"alertThreshold",2}}),
        make_widget("w-screen", WidgetType::Screener,        "MOMENTUM SCREENER",    2,1, 1,1, {{"screenerPreset","momentum"}}),
        make_widget("w-idx",    WidgetType::GlobalIndices,   "MARKET INDICES",       0,2),
        make_widget("w-news",   WidgetType::News,            "MARKETS & EARNINGS",   1,2, 2,1),
    };
    return l;
}

// Template 6: Macro Economist
inline DashboardLayout macro_economist_layout() {
    DashboardLayout l;
    l.template_id = "macro-economist";
    l.grid_cols = 3; l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        make_widget("w-eco",   WidgetType::EconomicIndicators, "ECONOMIC INDICATORS",  0,0),
        make_widget("w-cal",   WidgetType::EconomicCalendar,   "ECONOMIC CALENDAR",    1,0, 1,1, {{"country","US"},{"limit",12}}),
        make_widget("w-dbn",   WidgetType::DBnomics,           "DBNOMICS - FED FUNDS", 2,0, 1,1, {{"dbnomicsSeriesId","FRED/FEDFUNDS"}}),
        make_widget("w-idx",   WidgetType::GlobalIndices,      "GLOBAL INDICES",       0,1),
        make_widget("w-fx",    WidgetType::Forex,              "FOREX MAJOR PAIRS",    1,1),
        make_widget("w-cmd",   WidgetType::Commodities,        "COMMODITIES",          2,1),
        make_widget("w-news",  WidgetType::News,               "ECONOMIC NEWS",        0,2),
        make_widget("w-geo",   WidgetType::GeopoliticalRisk,   "GEOPOLITICAL RISK",    1,2),
        make_widget("w-heat",  WidgetType::SectorHeatmap,      "SECTOR HEATMAP",       2,2),
    };
    return l;
}

// Template 7: Geopolitics Analyst
inline DashboardLayout geopolitics_analyst_layout() {
    DashboardLayout l;
    l.template_id = "geopolitics-analyst";
    l.grid_cols = 3; l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        make_widget("w-geo",      WidgetType::GeopoliticalRisk, "GEOPOLITICAL RISK",   0,0),
        make_widget("w-maritime", WidgetType::Maritime,         "MARITIME TRACKING",   1,0),
        make_widget("w-ak",      WidgetType::AkShare,          "CHINA MARKETS",       2,0),
        make_widget("w-idx",     WidgetType::GlobalIndices,     "GLOBAL INDICES",      0,1),
        make_widget("w-fx",      WidgetType::Forex,             "FOREX",               1,1),
        make_widget("w-poly",    WidgetType::Polymarket,        "POLYMARKET",          2,1),
        make_widget("w-news",    WidgetType::News,              "GEOPOLITICS NEWS",    0,2, 2,1),
        make_widget("w-cmd",     WidgetType::Commodities,       "COMMODITIES",         2,2),
    };
    return l;
}

// Template 8: Individual Investor — India
inline DashboardLayout india_investor_layout() {
    DashboardLayout l;
    l.template_id = "india-investor";
    l.grid_cols = 3; l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        make_widget("w-idx",   WidgetType::GlobalIndices,     "MARKET INDICES",       0,0),
        make_widget("w-perf",  WidgetType::Performance,       "MY PORTFOLIO",         1,0),
        make_widget("w-port",  WidgetType::Portfolio,         "PORTFOLIO SUMMARY",    2,0),
        make_widget("w-watch", WidgetType::Watchlist,         "MY WATCHLIST",         0,1, 1,1, {{"watchlistName","Default"}}),
        make_widget("w-quote", WidgetType::StockQuote,        "STOCK QUOTE",          1,1, 1,1, {{"stockSymbol","RELIANCE.NS"}}),
        make_widget("w-cal",   WidgetType::EconomicCalendar,  "ECONOMIC CALENDAR",    2,1, 1,1, {{"country","IN"},{"limit",10}}),
        make_widget("w-news",  WidgetType::News,              "MARKET NEWS",          0,2),
        make_widget("w-eco",   WidgetType::EconomicIndicators,"ECONOMIC INDICATORS",  1,2),
        make_widget("w-cmd",   WidgetType::Commodities,       "COMMODITIES",          2,2),
    };
    return l;
}

// Template 9: Individual Investor — USA
inline DashboardLayout us_investor_layout() {
    DashboardLayout l;
    l.template_id = "us-investor";
    l.grid_cols = 3; l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        make_widget("w-idx",    WidgetType::GlobalIndices,     "US INDICES",          0,0),
        make_widget("w-perf",   WidgetType::Performance,       "MY PORTFOLIO",        1,0),
        make_widget("w-screen", WidgetType::Screener,          "VALUE SCREENER",      2,0, 1,1, {{"screenerPreset","value"}}),
        make_widget("w-quote",  WidgetType::StockQuote,        "STOCK QUOTE",         0,1, 1,1, {{"stockSymbol","AAPL"}}),
        make_widget("w-watch",  WidgetType::Watchlist,         "MY WATCHLIST",        1,1, 1,1, {{"watchlistName","Default"}}),
        make_widget("w-cal",    WidgetType::EconomicCalendar,  "ECONOMIC CALENDAR",   2,1, 1,1, {{"country","US"},{"limit",10}}),
        make_widget("w-news",   WidgetType::News,              "MARKETS & EARNINGS",  0,2),
        make_widget("w-eco",    WidgetType::EconomicIndicators,"ECONOMIC INDICATORS", 1,2),
        make_widget("w-port",   WidgetType::Portfolio,         "PORTFOLIO SUMMARY",   2,2),
    };
    return l;
}

// Template 10: Individual Investor — Asia
inline DashboardLayout asia_investor_layout() {
    DashboardLayout l;
    l.template_id = "asia-investor";
    l.grid_cols = 3; l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        make_widget("w-ak",   WidgetType::AkShare,            "CHINA MARKETS",       0,0),
        make_widget("w-idx",  WidgetType::GlobalIndices,       "GLOBAL INDICES",      1,0),
        make_widget("w-perf", WidgetType::Performance,         "MY PORTFOLIO",        2,0),
        make_widget("w-fx",   WidgetType::Forex,               "FOREX",               0,1),
        make_widget("w-geo",  WidgetType::GeopoliticalRisk,    "GEOPOLITICAL RISK",   1,1),
        make_widget("w-cal",  WidgetType::EconomicCalendar,    "ECONOMIC CALENDAR",   2,1, 1,1, {{"country","CN"},{"limit",10}}),
        make_widget("w-eco",  WidgetType::EconomicIndicators,  "ECONOMIC INDICATORS", 0,2),
        make_widget("w-news", WidgetType::News,                "MARKET NEWS",         1,2),
        make_widget("w-cmd",  WidgetType::Commodities,         "COMMODITIES",         2,2),
    };
    return l;
}

// Template 11: Custom / Blank
inline DashboardLayout blank_layout() {
    DashboardLayout l;
    l.template_id = "custom";
    l.grid_cols = 3; l.grid_rows = 3;
    l.market_pulse_open = false;
    l.widgets = {};
    return l;
}

// Template registry
struct TemplateInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string tag;
    DashboardLayout (*create)();
};

inline std::vector<TemplateInfo> get_all_templates() {
    return {
        {"default",             "Default Terminal",            "Fincept-style 3x3 grid with all major asset classes",              "TERMINAL", default_layout},
        {"portfolio-manager",   "Portfolio Manager",           "Track performance, risk, macro data and regulatory news",          "BUY-SIDE", portfolio_manager_layout},
        {"hedge-fund",          "Hedge Fund Manager",          "Alpha generation, signals, backtesting, sentiment and geo risk",   "ALPHA",    hedge_fund_layout},
        {"crypto-trader",       "Crypto Trader",               "Real-time crypto, Polymarket, sentiment and breaking news",        "CRYPTO",   crypto_trader_layout},
        {"equity-trader",       "Equity Trader",               "Intraday moves, alerts, quick trade, top movers and earnings",     "EQUITY",   equity_trader_layout},
        {"macro-economist",     "Macro Economist",             "Central bank feeds, economic indicators, DBnomics and forex",      "MACRO",    macro_economist_layout},
        {"geopolitics-analyst", "Geopolitics Analyst",         "Country risk, maritime, China markets and intelligence feeds",      "INTEL",    geopolitics_analyst_layout},
        {"india-investor",      "Individual Investor - India", "NSE/BSE indices, Indian market news, portfolio tracker",           "INDIA",    india_investor_layout},
        {"us-investor",         "Individual Investor - USA",   "S&P 500, NASDAQ, Dow, earnings, screener and portfolio",           "USA",      us_investor_layout},
        {"asia-investor",       "Individual Investor - Asia",  "China A-shares, Asian indices, AkShare, forex and geo risk",       "ASIA",     asia_investor_layout},
        {"custom",              "Custom / Blank",              "Start empty and build your own layout from scratch",               "CUSTOM",   blank_layout},
    };
}

// Generate a unique widget ID
inline std::string generate_widget_id() {
    static int counter = 100;
    return "w-" + std::to_string(++counter);
}

} // namespace fincept::dashboard
