#pragma once
// Screener Types — FRED Economic Data series, search results, categories.

#include <string>
#include <vector>
#include <optional>

namespace fincept::screener {

// A single observation (date + value)
struct Observation {
    std::string date;
    double value = 0.0;
};

// A FRED time series with metadata + observations
struct FREDSeries {
    std::string series_id;
    std::string title;
    std::string units;
    std::string frequency;
    std::string seasonal_adjustment;
    std::string last_updated;
    std::vector<Observation> observations;
    int observation_count = 0;
    std::string error;
};

// Search result from FRED series search
struct SearchResult {
    std::string id;
    std::string title;
    std::string frequency;
    std::string units;
    std::string seasonal_adjustment;
    std::string last_updated;
    int popularity = 0;
};

// FRED category
struct Category {
    int id = 0;
    std::string name;
    int parent_id = 0;
};

// Breadcrumb path element
struct CategoryPath {
    int id = 0;
    std::string name;
};

// Popular preset series for quick-add
struct PopularSeries {
    const char* id;
    const char* name;
    const char* category;
};

// Popular preset categories
struct PopularCategory {
    int id;
    const char* name;
    const char* desc;
};

// Chart display mode
enum class ChartType { line, area };

// ============================================================================
// Constants
// ============================================================================

inline constexpr PopularSeries POPULAR_SERIES[] = {
    {"GDP",               "Gross Domestic Product",   "Economic"},
    {"UNRATE",            "Unemployment Rate",        "Labor"},
    {"CPIAUCSL",          "Consumer Price Index",     "Inflation"},
    {"FEDFUNDS",          "Federal Funds Rate",       "Interest Rates"},
    {"DGS10",             "10-Year Treasury Rate",    "Bonds"},
    {"DEXUSEU",           "USD/EUR Exchange Rate",    "Forex"},
    {"DCOILWTICO",        "WTI Crude Oil Price",      "Commodities"},
    {"GOLDAMGBD228NLBM",  "Gold Price",               "Commodities"},
};
inline constexpr int POPULAR_SERIES_COUNT = sizeof(POPULAR_SERIES) / sizeof(POPULAR_SERIES[0]);

inline constexpr PopularCategory POPULAR_CATEGORIES[] = {
    {32991, "Money, Banking, & Finance", "Money supply, interest rates, banking data"},
    {10,    "Population, Employment, Labor", "Employment, unemployment, labor statistics"},
    {32455, "Prices",                    "Inflation, CPI, PPI, commodity prices"},
    {1,     "Production & Business",     "Industrial production, capacity utilization"},
    {32263, "International Data",        "Trade, exchange rates, foreign markets"},
    {33060, "U.S. Regional Data",        "State and metro area statistics"},
};
inline constexpr int POPULAR_CATEGORIES_COUNT = sizeof(POPULAR_CATEGORIES) / sizeof(POPULAR_CATEGORIES[0]);

// Chart colors (matching desktop)
inline constexpr unsigned int CHART_COLORS[] = {
    0xFF0C58EA, // #ea580c orange (ABGR for ImGui)
    0xFF55C522, // #22c55e green
    0xFFF6823B, // #3b82f6 blue
    0xFF08B3EA, // #eab308 yellow
    0xFF4444EF, // #ef4444 red
};
inline constexpr int CHART_COLORS_COUNT = sizeof(CHART_COLORS) / sizeof(CHART_COLORS[0]);

} // namespace fincept::screener
