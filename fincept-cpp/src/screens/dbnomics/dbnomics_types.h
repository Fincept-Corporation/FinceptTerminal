#pragma once
// DBnomics Tab — Data types, constants
// Port of dbnomics/types.ts + dbnomics/constants.ts

#include <string>
#include <vector>

namespace fincept::dbnomics {

// ============================================================================
// Core types
// ============================================================================

struct Provider {
    std::string code;
    std::string name;
};

struct Dataset {
    std::string code;
    std::string name;
};

struct Series {
    std::string code;
    std::string name;
    std::string full_id;  // provider/dataset/series_code
};

struct Observation {
    std::string period;
    double value = 0.0;
};

struct DataPoint {
    std::string series_id;
    std::string series_name;
    std::vector<Observation> observations;
    int color_index = 0;  // index into chart color palette
};

struct SearchResult {
    std::string provider_code;
    std::string provider_name;
    std::string dataset_code;
    std::string dataset_name;
    int nb_series = 0;
};

struct PaginationState {
    int offset = 0;
    int limit = 50;
    int total = 0;
    bool has_more = false;
};

// Chart type for visualization
enum class ChartType { Line, Bar, Area, Scatter };

inline const char* chart_type_label(ChartType t) {
    switch (t) {
        case ChartType::Line:    return "LINE";
        case ChartType::Bar:     return "BAR";
        case ChartType::Area:    return "AREA";
        case ChartType::Scatter: return "SCATTER";
        default:                 return "LINE";
    }
}

// ============================================================================
// Constants
// ============================================================================

constexpr const char* DBNOMICS_API_BASE = "https://api.db.nomics.world/v22";

constexpr int DATASETS_PAGE_SIZE = 50;
constexpr int SERIES_PAGE_SIZE   = 50;
constexpr int SEARCH_PAGE_SIZE   = 20;

// Chart colors (ImPlot-compatible, indexed)
constexpr int CHART_COLOR_COUNT = 6;

} // namespace fincept::dbnomics
