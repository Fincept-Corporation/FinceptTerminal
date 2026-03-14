#pragma once
// Geopolitics data types — mirrors Tauri GeopoliticsTab state and API types

#include <string>
#include <vector>
#include <optional>
#include <imgui.h>

namespace fincept::geopolitics {

// ============================================================================
// News Events (from api.fincept.in/research/news-events)
// ============================================================================
struct GeoEvent {
    std::string url;
    std::string domain;
    std::string event_category;
    std::string matched_keywords;
    std::string city;
    std::string country;
    double latitude = 0;
    double longitude = 0;
    std::string extracted_date;
    std::string created_at;
};

struct UniqueCountry {
    std::string country;
    int event_count = 0;
};

struct UniqueCategory {
    std::string event_category;
    int event_count = 0;
};

struct UniqueCity {
    std::string city;
    std::string country;
};

// ============================================================================
// Category color mapping
// ============================================================================
inline ImU32 category_color(const std::string& cat) {
    if (cat.find("armed_conflict") != std::string::npos) return IM_COL32(255, 0, 0, 220);
    if (cat.find("terrorism") != std::string::npos)       return IM_COL32(255, 69, 0, 220);
    if (cat.find("protests") != std::string::npos)        return IM_COL32(255, 215, 0, 200);
    if (cat.find("civilian") != std::string::npos)        return IM_COL32(255, 100, 100, 200);
    if (cat.find("riots") != std::string::npos)           return IM_COL32(255, 165, 0, 200);
    if (cat.find("political") != std::string::npos)       return IM_COL32(147, 51, 234, 200);
    if (cat.find("crisis") != std::string::npos)          return IM_COL32(0, 229, 255, 200);
    if (cat.find("explosions") != std::string::npos)      return IM_COL32(255, 20, 147, 220);
    if (cat.find("strategic") != std::string::npos)       return IM_COL32(100, 149, 237, 200);
    return IM_COL32(136, 136, 136, 200);
}

inline ImVec4 category_color_vec(const std::string& cat) {
    ImU32 c = category_color(cat);
    return ImVec4(
        ((c >> 0) & 0xFF) / 255.0f,
        ((c >> 8) & 0xFF) / 255.0f,
        ((c >> 16) & 0xFF) / 255.0f,
        ((c >> 24) & 0xFF) / 255.0f
    );
}

// ============================================================================
// WTO Data Types
// ============================================================================
struct WTOTimeseriesPoint {
    std::string reporting_economy;
    std::string partner_economy;
    std::string indicator;
    std::string period;
    double value = 0;
};

struct WTONotification {
    std::string title;
    std::string member;
    std::string date;
    std::string type;
    std::string description;
};

} // namespace fincept::geopolitics
