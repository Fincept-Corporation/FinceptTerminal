#pragma once
// Notes tab types, constants, and helpers — mirrors Tauri constants.ts

#include <string>
#include <array>
#include "imgui.h"

namespace fincept::notes {

// Categories
struct CategoryDef {
    const char* id;
    const char* label;
    const char* icon;   // text icon for ImGui
};

static constexpr std::array<CategoryDef, 8> CATEGORIES = {{
    {"ALL",              "All Notes",        "[*]"},
    {"TRADE_IDEA",       "Trade Ideas",      "[$]"},
    {"RESEARCH",         "Research",         "[R]"},
    {"MARKET_ANALYSIS",  "Market Analysis",  "[M]"},
    {"EARNINGS",         "Earnings",         "[E]"},
    {"ECONOMIC",         "Economic",         "[~]"},
    {"PORTFOLIO",        "Portfolio",        "[P]"},
    {"GENERAL",          "General",          "[G]"},
}};

// Priorities
static constexpr const char* PRIORITIES[] = {"HIGH", "MEDIUM", "LOW"};

// Sentiments
static constexpr const char* SENTIMENTS[] = {"BULLISH", "BEARISH", "NEUTRAL"};

// Color palette for notes
static constexpr const char* COLOR_CODES[] = {
    "#FF8C00",  // Orange (default)
    "#FF4444",  // Red
    "#44BB44",  // Green
    "#4488FF",  // Blue
    "#AA44FF",  // Purple
    "#FF44AA",  // Pink
    "#44DDDD",  // Cyan
};
static constexpr int NUM_COLORS = 7;

// Convert hex color to ImVec4
inline ImVec4 hex_to_imvec4(const std::string& hex) {
    if (hex.size() < 7 || hex[0] != '#') return ImVec4(1, 0.55f, 0, 1); // default orange
    int r = 0, g = 0, b = 0;
    if (std::sscanf(hex.c_str(), "#%02x%02x%02x", &r, &g, &b) == 3) {
        return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
    }
    return ImVec4(1, 0.55f, 0, 1);
}

inline ImVec4 priority_color(const std::string& p) {
    if (p == "HIGH")   return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    if (p == "MEDIUM") return ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
    return ImVec4(0.5f, 0.7f, 0.5f, 1.0f); // LOW
}

inline ImVec4 sentiment_color(const std::string& s) {
    if (s == "BULLISH") return ImVec4(0.2f, 0.8f, 0.4f, 1.0f);
    if (s == "BEARISH") return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    return ImVec4(0.6f, 0.6f, 0.6f, 1.0f); // NEUTRAL
}

} // namespace fincept::notes
