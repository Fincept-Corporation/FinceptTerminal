#pragma once
// M&A Analytics — module definitions, accent colors, format helpers

#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <string>

namespace fincept::mna {

// ============================================================================
// Module accent colors (matching Tauri MA_COLORS)
// ============================================================================
namespace ma_colors {
    constexpr ImVec4 VALUATION  = {1.00f, 0.42f, 0.21f, 1.0f};  // #FF6B35 Orange
    constexpr ImVec4 MERGER     = {0.00f, 0.90f, 1.00f, 1.0f};  // #00E5FF Cyan
    constexpr ImVec4 DEAL_DB    = {0.00f, 0.53f, 1.00f, 1.0f};  // #0088FF Blue
    constexpr ImVec4 STARTUP    = {0.62f, 0.31f, 0.87f, 1.0f};  // #9D4EDD Purple
    constexpr ImVec4 FAIRNESS   = {0.00f, 0.84f, 0.44f, 1.0f};  // #00D66F Green
    constexpr ImVec4 INDUSTRY   = {1.00f, 0.77f, 0.00f, 1.0f};  // #FFC400 Yellow
    constexpr ImVec4 ADVANCED   = {1.00f, 0.23f, 0.56f, 1.0f};  // #FF3B8E Pink
    constexpr ImVec4 COMPARISON = {0.00f, 0.74f, 0.83f, 1.0f};  // #00BCD4 Teal
}

// Chart palette (8 series colors)
inline const ImVec4* chart_palette() {
    static const ImVec4 p[] = {
        ma_colors::VALUATION, ma_colors::MERGER, ma_colors::STARTUP, ma_colors::FAIRNESS,
        ma_colors::INDUSTRY, ma_colors::ADVANCED, ma_colors::DEAL_DB, ma_colors::COMPARISON,
    };
    return p;
}

// Heatmap color scale (red → yellow → green, 10 entries)
inline const ImVec4* heatmap_scale() {
    static const ImVec4 s[] = {
        {1.00f, 0.23f, 0.23f, 1.0f},  // #FF3B3B
        {1.00f, 0.42f, 0.21f, 1.0f},  // #FF6B35
        {1.00f, 0.58f, 0.00f, 1.0f},  // #FF9500
        {1.00f, 0.77f, 0.00f, 1.0f},  // #FFC400
        {1.00f, 0.84f, 0.00f, 1.0f},  // #FFD700
        {0.78f, 0.90f, 0.00f, 1.0f},  // #C8E600
        {0.50f, 0.84f, 0.44f, 1.0f},  // #7FD66F
        {0.00f, 0.84f, 0.44f, 1.0f},  // #00D66F
        {0.00f, 0.78f, 0.32f, 1.0f},  // #00C851
        {0.00f, 0.66f, 0.27f, 1.0f},  // #00A844
    };
    return s;
}

// ============================================================================
// Module definitions
// ============================================================================
struct ModuleDef {
    const char* id;
    const char* label;
    const char* short_label;
    const char* category;
    ImVec4 color;
};

inline const ModuleDef* modules() {
    static const ModuleDef m[] = {
        {"valuation",  "VALUATION TOOLKIT",  "VAL", "CORE",        ma_colors::VALUATION},
        {"merger",     "MERGER ANALYSIS",     "MRG", "CORE",        ma_colors::MERGER},
        {"deals",      "DEAL DATABASE",       "DDB", "CORE",        ma_colors::DEAL_DB},
        {"startup",    "STARTUP VALUATION",   "STV", "SPECIALIZED", ma_colors::STARTUP},
        {"fairness",   "FAIRNESS OPINION",    "FOP", "SPECIALIZED", ma_colors::FAIRNESS},
        {"industry",   "INDUSTRY METRICS",    "IND", "SPECIALIZED", ma_colors::INDUSTRY},
        {"advanced",   "ADVANCED ANALYTICS",  "ADV", "ANALYTICS",   ma_colors::ADVANCED},
        {"comparison", "DEAL COMPARISON",     "CMP", "ANALYTICS",   ma_colors::COMPARISON},
    };
    return m;
}
constexpr int MODULE_COUNT = 8;

// Module capabilities (for right info panel)
inline const char* const* module_capabilities(int idx) {
    static const char* valuation[] = {
        "DCF Analysis", "LBO Modeling (Returns, Full Model, Debt Schedule, Sensitivity)",
        "Trading Comparables", "Precedent Transactions", nullptr};
    static const char* merger[] = {
        "Accretion / Dilution Analysis", "Revenue & Cost Synergies",
        "Deal Structure (Payment, Earnout, Exchange, Collar, CVR)",
        "Pro Forma Financials", nullptr};
    static const char* deals[] = {
        "Deal Database & Search", "SEC Filing Scanner (EDGAR)",
        "Auto-Parse Deal Indicators", "Confidence Scoring", nullptr};
    static const char* startup[] = {
        "Berkus Method", "Scorecard Method", "VC Method",
        "First Chicago Method", "Risk Factor Summation",
        "Comprehensive (All Methods)", nullptr};
    static const char* fairness[] = {
        "Fairness Analysis & Conclusion", "Premium Analysis (Multi-Period)",
        "Process Quality Assessment", nullptr};
    static const char* industry[] = {
        "Technology (SaaS, Marketplace, Semi)", "Healthcare (Pharma, Biotech, Devices)",
        "Financial Services (Banking, Insurance, AM)", nullptr};
    static const char* advanced[] = {
        "Monte Carlo Simulation (1K-100K runs)",
        "Regression Analysis (OLS, Multiple)", nullptr};
    static const char* comparison[] = {
        "Side-by-Side Deal Comparison", "Deal Rankings",
        "Premium Benchmarking", "Payment Structure Analysis",
        "Industry Analysis", nullptr};

    static const char* const* all[] = {
        valuation, merger, deals, startup, fairness, industry, advanced, comparison
    };
    if (idx < 0 || idx >= MODULE_COUNT) return valuation;
    return all[idx];
}

// ============================================================================
// Format helpers
// ============================================================================
inline std::string format_deal_value(double v) {
    char buf[32];
    if (std::abs(v) >= 1e9)      std::snprintf(buf, sizeof(buf), "$%.1fB", v / 1e9);
    else if (std::abs(v) >= 1e6) std::snprintf(buf, sizeof(buf), "$%.1fM", v / 1e6);
    else if (std::abs(v) >= 1e3) std::snprintf(buf, sizeof(buf), "$%.0fK", v / 1e3);
    else                         std::snprintf(buf, sizeof(buf), "$%.0f", v);
    return buf;
}

inline std::string format_multiple(double v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1fx", v);
    return buf;
}

inline std::string format_bps(double v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%dbps", static_cast<int>(v * 100));
    return buf;
}

inline std::string format_currency(double v, int decimals = 2) {
    char buf[32];
    if (std::abs(v) >= 1e9)      std::snprintf(buf, sizeof(buf), "$%.2fB", v / 1e9);
    else if (std::abs(v) >= 1e6) std::snprintf(buf, sizeof(buf), "$%.2fM", v / 1e6);
    else if (decimals == 0)      std::snprintf(buf, sizeof(buf), "$%.0f", v);
    else                         std::snprintf(buf, sizeof(buf), "$%.*f", decimals, v);
    return buf;
}

inline std::string format_percent(double v, int decimals = 1) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.*f%%", decimals, v);
    return buf;
}

// Color-coded button helper for module accent colors
inline bool ColoredButton(const char* label, const ImVec4& color, bool active, const ImVec2& size = ImVec2(0, 0)) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x, color.y, color.z, 0.20f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x, color.y, color.z, 0.30f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x, color.y, color.z, 0.40f));
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(color.x, color.y, color.z, 0.50f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x, color.y, color.z, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x, color.y, color.z, 0.20f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.53f, 0.53f, 0.57f, 1.0f));  // TEXT_DIM
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    }
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
    return clicked;
}

// Collapsible section header (returns true if expanded)
inline bool CollapsibleHeader(const char* label, bool* open, const ImVec4& color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    const char* arrow = *open ? "[-] " : "[+] ";
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s%s", arrow, label);
    if (ImGui::Selectable(buf, false, ImGuiSelectableFlags_None, ImVec2(0, 18))) {
        *open = !*open;
    }
    ImGui::PopStyleColor();
    return *open;
}

// Dense input row helper: label on left, InputDouble on right
inline bool InputRow(const char* label, double* val, const char* fmt = "%.2f",
                     float label_w = 160, float input_w = 120) {
    ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "%s", label);
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(input_w);
    char id[64];
    std::snprintf(id, sizeof(id), "##%s", label);
    return ImGui::InputDouble(id, val, 0, 0, fmt);
}

// Dense input row for int
inline bool InputRowInt(const char* label, int* val, float label_w = 160, float input_w = 120) {
    ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "%s", label);
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(input_w);
    char id[64];
    std::snprintf(id, sizeof(id), "##%s", label);
    return ImGui::InputInt(id, val);
}

// Data row: label left, value right-aligned with color
inline void DataRow(const char* label, const char* value, const ImVec4& color,
                    float total_w = 0) {
    ImGui::TextColored(ImVec4(0.53f, 0.53f, 0.57f, 1.0f), "%s", label);
    if (total_w <= 0) total_w = ImGui::GetContentRegionAvail().x;
    float val_w = ImGui::CalcTextSize(value).x;
    ImGui::SameLine(total_w - val_w);
    ImGui::TextColored(color, "%s", value);
}

// Status message display (auto-dismiss after timeout)
inline void StatusMessage(const std::string& msg, double msg_time, double timeout = 5.0) {
    if (msg.empty()) return;
    double elapsed = ImGui::GetTime() - msg_time;
    if (elapsed >= timeout) return;
    ImVec4 col = {0.25f, 0.84f, 0.42f, 1.0f};  // green
    if (msg.find("Error") != std::string::npos || msg.find("Failed") != std::string::npos)
        col = {0.96f, 0.30f, 0.30f, 1.0f};  // red
    ImGui::TextColored(col, "%s", msg.c_str());
}

// Run button with accent color
inline bool RunButton(const char* label, bool loading, const ImVec4& accent) {
    if (loading) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x, accent.y, accent.z, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent.x, accent.y, accent.z, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.5f));
        ImGui::Button("COMPUTING...");
        ImGui::PopStyleColor(3);
        return false;
    }
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x, accent.y, accent.z, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(accent.x, accent.y, accent.z, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    bool clicked = ImGui::Button(label);
    ImGui::PopStyleColor(4);
    return clicked;
}

} // namespace fincept::mna
