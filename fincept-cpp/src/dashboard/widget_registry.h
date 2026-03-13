#pragma once
// Widget type system and layout persistence for the dashboard
// Supports adding/removing widgets, drag-resize, and SQLite persistence

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::dashboard {

// All available widget types
enum class WidgetType {
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
    MarketData,     // generic quote table
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
        case WidgetType::Performance:        return "Performance";
        case WidgetType::TopMovers:          return "Top Movers";
        case WidgetType::MarketData:         return "Market Data";
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
        default: return "unknown";
    }
}

inline WidgetType widget_type_from_id(const std::string& id) {
    if (id == "indices")      return WidgetType::GlobalIndices;
    if (id == "forex")        return WidgetType::Forex;
    if (id == "commodities")  return WidgetType::Commodities;
    if (id == "crypto")       return WidgetType::Crypto;
    if (id == "heatmap")      return WidgetType::SectorHeatmap;
    if (id == "news")         return WidgetType::News;
    if (id == "economic")     return WidgetType::EconomicIndicators;
    if (id == "geopolitics")  return WidgetType::GeopoliticalRisk;
    if (id == "performance")  return WidgetType::Performance;
    if (id == "topmovers")    return WidgetType::TopMovers;
    if (id == "marketdata")   return WidgetType::MarketData;
    return WidgetType::GlobalIndices;
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

    nlohmann::json to_json() const {
        return {
            {"id", id}, {"type", widget_type_id(type)}, {"title", title},
            {"col", col}, {"row", row}, {"col_span", col_span}, {"row_span", row_span},
            {"visible", visible}
        };
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
        if (w.title.empty()) w.title = widget_type_name(w.type);
        return w;
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

// =============================================================================
// Default templates
// =============================================================================
inline DashboardLayout default_layout() {
    DashboardLayout l;
    l.template_id = "default";
    l.grid_cols = 3;
    l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        {"w-idx",   WidgetType::GlobalIndices,      "GLOBAL INDICES",       0, 0, 1, 1, true},
        {"w-heat",  WidgetType::SectorHeatmap,       "SECTOR HEATMAP",       1, 0, 1, 1, true},
        {"w-perf",  WidgetType::Performance,         "PERFORMANCE",          2, 0, 1, 1, true},
        {"w-fx",    WidgetType::Forex,               "FOREX",                0, 1, 1, 1, true},
        {"w-cmd",   WidgetType::Commodities,         "COMMODITIES",          1, 1, 1, 1, true},
        {"w-cry",   WidgetType::Crypto,              "CRYPTO",               2, 1, 1, 1, true},
        {"w-news",  WidgetType::News,                "MARKET NEWS",          0, 2, 1, 1, true},
        {"w-eco",   WidgetType::EconomicIndicators,  "ECONOMIC INDICATORS",  1, 2, 1, 1, true},
        {"w-geo",   WidgetType::GeopoliticalRisk,    "GEOPOLITICAL RISK",    2, 2, 1, 1, true},
    };
    return l;
}

inline DashboardLayout crypto_trader_layout() {
    DashboardLayout l;
    l.template_id = "crypto-trader";
    l.grid_cols = 3;
    l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        {"w-cry",   WidgetType::Crypto,              "CRYPTO MARKETS",       0, 0, 2, 1, true},
        {"w-movers",WidgetType::TopMovers,           "TOP MOVERS",           2, 0, 1, 1, true},
        {"w-fx",    WidgetType::Forex,               "FOREX",                0, 1, 1, 1, true},
        {"w-cmd",   WidgetType::Commodities,         "COMMODITIES",          1, 1, 1, 1, true},
        {"w-idx",   WidgetType::GlobalIndices,       "INDICES",              2, 1, 1, 1, true},
        {"w-news",  WidgetType::News,                "CRYPTO NEWS",          0, 2, 2, 1, true},
        {"w-eco",   WidgetType::EconomicIndicators,  "ECONOMIC",             2, 2, 1, 1, true},
    };
    return l;
}

inline DashboardLayout macro_economist_layout() {
    DashboardLayout l;
    l.template_id = "macro-economist";
    l.grid_cols = 3;
    l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        {"w-eco",   WidgetType::EconomicIndicators,  "ECONOMIC INDICATORS",  0, 0, 1, 1, true},
        {"w-idx",   WidgetType::GlobalIndices,       "GLOBAL INDICES",       1, 0, 1, 1, true},
        {"w-fx",    WidgetType::Forex,               "FOREX MAJOR PAIRS",    2, 0, 1, 1, true},
        {"w-cmd",   WidgetType::Commodities,         "COMMODITIES",          0, 1, 1, 1, true},
        {"w-heat",  WidgetType::SectorHeatmap,       "SECTOR HEATMAP",       1, 1, 1, 1, true},
        {"w-geo",   WidgetType::GeopoliticalRisk,    "GEOPOLITICAL RISK",    2, 1, 1, 1, true},
        {"w-news",  WidgetType::News,                "ECONOMIC NEWS",        0, 2, 2, 1, true},
        {"w-cry",   WidgetType::Crypto,              "CRYPTO",               2, 2, 1, 1, true},
    };
    return l;
}

inline DashboardLayout equity_trader_layout() {
    DashboardLayout l;
    l.template_id = "equity-trader";
    l.grid_cols = 3;
    l.grid_rows = 3;
    l.market_pulse_open = true;
    l.widgets = {
        {"w-idx",   WidgetType::GlobalIndices,       "MARKET INDICES",       0, 0, 1, 1, true},
        {"w-movers",WidgetType::TopMovers,           "TOP MOVERS",           1, 0, 1, 1, true},
        {"w-heat",  WidgetType::SectorHeatmap,       "SECTOR HEATMAP",       2, 0, 1, 1, true},
        {"w-perf",  WidgetType::Performance,         "PERFORMANCE",          0, 1, 1, 1, true},
        {"w-fx",    WidgetType::Forex,               "FOREX",                1, 1, 1, 1, true},
        {"w-cmd",   WidgetType::Commodities,         "COMMODITIES",          2, 1, 1, 1, true},
        {"w-news",  WidgetType::News,                "MARKETS NEWS",         0, 2, 2, 1, true},
        {"w-eco",   WidgetType::EconomicIndicators,  "ECONOMIC",             2, 2, 1, 1, true},
    };
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
        {"default",        "Default Terminal",       "Bloomberg-style 3x3 grid with all major asset classes", "TERMINAL", default_layout},
        {"crypto-trader",  "Crypto Trader",          "Focus on crypto markets, top movers, and forex",        "CRYPTO",   crypto_trader_layout},
        {"macro-economist","Macro Economist",         "Economic indicators, indices, commodities, and geo risk","MACRO",   macro_economist_layout},
        {"equity-trader",  "Equity Trader",           "Stock indices, top movers, sectors, and performance",   "EQUITY",  equity_trader_layout},
    };
}

// Generate a unique widget ID
inline std::string generate_widget_id() {
    static int counter = 100;
    return "w-" + std::to_string(++counter);
}

} // namespace fincept::dashboard
