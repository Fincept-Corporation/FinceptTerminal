#pragma once
// Watchlist tab — data types and utility functions
// Mirrors utils.ts from the Tauri watchlist component

#include <imgui.h>
#include <string>
#include <vector>
#include <cstdio>
#include <optional>

namespace fincept::watchlist {

// Stock quote data (fetched via python/yfinance)
struct StockQuote {
    double price          = 0;
    double change         = 0;
    double change_percent = 0;
    double high           = 0;
    double low            = 0;
    double open           = 0;
    double previous_close = 0;
    double volume         = 0;
    int64_t timestamp     = 0;
    bool valid            = false;
};

// Stock entry with optional quote data
struct WatchlistStockEntry {
    std::string id;
    std::string watchlist_id;
    std::string symbol;
    std::string added_at;
    std::string notes;
    StockQuote quote;
};

// Watchlist with stock count
struct WatchlistWithCount {
    std::string id;
    std::string name;
    std::string description;
    std::string color;
    std::string created_at;
    std::string updated_at;
    int stock_count = 0;
};

// Market mover entry (for sidebar panels)
struct MarketMover {
    std::string symbol;
    double change_percent = 0;
    double volume         = 0;
};

// Sort criteria
enum class SortCriteria { Ticker, Change, Volume, Price };

inline const char* sort_label(SortCriteria s) {
    switch (s) {
        case SortCriteria::Ticker: return "TICKER";
        case SortCriteria::Change: return "CHANGE";
        case SortCriteria::Volume: return "VOLUME";
        case SortCriteria::Price:  return "PRICE";
    }
    return "CHANGE";
}

// Watchlist color palette (matches Tauri WATCHLIST_COLORS)
inline const std::vector<const char*>& watchlist_colors() {
    static const std::vector<const char*> colors = {
        "#FF8800", "#0088FF", "#00D66F", "#00E5FF",
        "#9D4EDD", "#FFD700", "#FF3B3B", "#FFFFFF"
    };
    return colors;
}

// Convert hex color string to ImVec4 (e.g. "#FF8800" -> orange)
inline ImVec4 hex_to_color(const std::string& hex) {
    if (hex.size() < 7 || hex[0] != '#') return ImVec4(1, 1, 1, 1);
    unsigned int r = 0, g = 0, b = 0;
    std::sscanf(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

// Format helpers
inline std::string format_currency(double val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "$%.2f", val);
    return buf;
}

inline std::string format_percent(double val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s%.2f%%", val >= 0 ? "+" : "", val);
    return buf;
}

inline std::string format_volume(double val) {
    char buf[32];
    if (val >= 1e12)      std::snprintf(buf, sizeof(buf), "%.2fT", val / 1e12);
    else if (val >= 1e9)  std::snprintf(buf, sizeof(buf), "%.2fB", val / 1e9);
    else if (val >= 1e6)  std::snprintf(buf, sizeof(buf), "%.1fM", val / 1e6);
    else if (val >= 1e3)  std::snprintf(buf, sizeof(buf), "%.1fK", val / 1e3);
    else                  std::snprintf(buf, sizeof(buf), "%.0f", val);
    return buf;
}

} // namespace fincept::watchlist
