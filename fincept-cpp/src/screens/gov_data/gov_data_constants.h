#pragma once
// Government Data Tab — provider definitions, colors, and UI helpers

#include "gov_data_types.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstdio>
#include <string>
#include <vector>

namespace fincept::gov_data {

// =============================================================================
// Provider accent colors
// =============================================================================
namespace provider_colors {
    constexpr ImVec4 US_TREASURY   = {0.231f, 0.510f, 0.965f, 1.0f}; // #3B82F6
    constexpr ImVec4 US_CONGRESS   = {0.545f, 0.361f, 0.965f, 1.0f}; // #8B5CF6
    constexpr ImVec4 CANADA        = {0.937f, 0.267f, 0.267f, 1.0f}; // #EF4444
    constexpr ImVec4 OPENAFRICA    = {0.961f, 0.620f, 0.043f, 1.0f}; // #F59E0B
    constexpr ImVec4 SPAIN         = {0.863f, 0.149f, 0.149f, 1.0f}; // #DC2626
    constexpr ImVec4 FINLAND       = {0.055f, 0.647f, 0.914f, 1.0f}; // #0EA5E9
    constexpr ImVec4 SWISS         = {0.882f, 0.114f, 0.282f, 1.0f}; // #E11D48
    constexpr ImVec4 FRANCE        = {0.145f, 0.388f, 0.922f, 1.0f}; // #2563EB
    constexpr ImVec4 CKAN          = {0.063f, 0.725f, 0.506f, 1.0f}; // #10B981
    constexpr ImVec4 HONG_KONG     = {0.957f, 0.247f, 0.369f, 1.0f}; // #F43F5E
}

// =============================================================================
// Provider configurations
// =============================================================================
inline const std::vector<ProviderConfig>& providers() {
    static const std::vector<ProviderConfig> p = {
        {ProviderId::USTreasury,   "US Treasury",        "U.S. Department of the Treasury",
         "Treasury securities prices, auctions & market data",
         provider_colors::US_TREASURY, "United States", "US", "government_us_data.py"},

        {ProviderId::USCongress,   "US Congress",        "United States Congress",
         "Congressional bills, resolutions & legislative activity",
         provider_colors::US_CONGRESS, "United States", "US", "congress_gov_data.py"},

        {ProviderId::CanadaGov,    "Canada Open Gov",    "Government of Canada Open Data",
         "Open data portal with datasets from Canadian federal departments",
         provider_colors::CANADA, "Canada", "CA", "canada_gov_api.py"},

        {ProviderId::OpenAfrica,   "openAFRICA",         "openAFRICA Open Data Portal",
         "African open data portal with datasets from across the continent",
         provider_colors::OPENAFRICA, "Africa", "AF", "openafrica_api.py"},

        {ProviderId::SpainData,    "Spain Open Data",    "datos.gob.es - Spain Open Data",
         "Spanish government open data portal with catalogues & datasets",
         provider_colors::SPAIN, "Spain", "ES", "spain_data.py"},

        {ProviderId::FinlandPxWeb, "Statistics Finland", "Statistics Finland (PxWeb)",
         "Finnish statistical database via PxWeb API",
         provider_colors::FINLAND, "Finland", "FI", "pxweb_fetcher.py"},

        {ProviderId::SwissGov,     "Swiss Open Data",    "opendata.swiss - Swiss Government",
         "Swiss federal open data portal with multilingual datasets",
         provider_colors::SWISS, "Switzerland", "CH", "swiss_gov_api.py"},

        {ProviderId::FranceGov,    "France Open Data",   "data.gouv.fr - French Government",
         "French government APIs: geographic, datasets, tabular & company",
         provider_colors::FRANCE, "France", "FR", "french_gov_api.py"},

        {ProviderId::UniversalCkan,"CKAN Portals",       "Universal CKAN Open Data Portals",
         "8 CKAN portals: US, UK, Australia, Italy, Brazil, Latvia, Slovenia, Uruguay",
         provider_colors::CKAN, "Multi-Country", "CKAN", "universal_ckan_api.py"},

        {ProviderId::HongKongGov,  "Hong Kong Gov",      "Data.gov.hk - Hong Kong Government",
         "Hong Kong government open data portal with CKAN catalog",
         provider_colors::HONG_KONG, "Hong Kong", "HK", "data_gov_hk_api.py"},
    };
    return p;
}

// =============================================================================
// CKAN portals
// =============================================================================
inline const std::vector<CkanPortal>& ckan_portals() {
    static const std::vector<CkanPortal> p = {
        {"us", "United States", "US", "catalog.data.gov"},
        {"uk", "United Kingdom", "UK", "data.gov.uk"},
        {"au", "Australia",     "AU", "data.gov.au"},
        {"it", "Italy",         "IT", "dati.gov.it"},
        {"br", "Brazil",        "BR", "dados.gov.br"},
        {"lv", "Latvia",        "LV", "data.gov.lv"},
        {"si", "Slovenia",      "SI", "podatki.gov.si"},
        {"uy", "Uruguay",       "UY", "catalogodatos.gub.uy"},
    };
    return p;
}

// =============================================================================
// Security type filters (US Treasury)
// =============================================================================
struct FilterOption { const char* value; const char* label; };

inline const std::vector<FilterOption>& security_types() {
    static const std::vector<FilterOption> f = {
        {"all", "All Securities"}, {"bill", "Treasury Bills"}, {"note", "Treasury Notes"},
        {"bond", "Treasury Bonds"}, {"tips", "TIPS"}, {"frn", "Floating Rate Notes"},
        {"cmb", "Cash Mgmt Bills"},
    };
    return f;
}

inline const std::vector<FilterOption>& auction_security_types() {
    static const std::vector<FilterOption> f = {
        {"all", "All Types"}, {"Bill", "Bills"}, {"Note", "Notes"}, {"Bond", "Bonds"},
        {"TIPS", "TIPS"}, {"FRN", "FRN"}, {"CMB", "CMB"},
    };
    return f;
}

// =============================================================================
// Congress bill types
// =============================================================================
inline const std::vector<FilterOption>& congress_bill_types() {
    static const std::vector<FilterOption> f = {
        {"", "All Types"}, {"hr", "House Bill"}, {"s", "Senate Bill"},
        {"hjres", "House Joint Res."}, {"sjres", "Senate Joint Res."},
        {"hconres", "House Con. Res."}, {"sconres", "Senate Con. Res."},
        {"hres", "House Simple Res."}, {"sres", "Senate Simple Res."},
    };
    return f;
}

// =============================================================================
// Format helpers
// =============================================================================
inline std::string format_size(int64_t bytes) {
    if (bytes <= 0) return "-";
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        char buf[32]; std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
        return buf;
    }
    char buf[32]; std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

inline std::string format_date(const std::string& d) {
    if (d.empty()) return "-";
    return d.substr(0, 10);
}

inline std::string truncate(const std::string& s, size_t max) {
    if (s.size() <= max) return s;
    return s.substr(0, max) + "...";
}

// =============================================================================
// UI Helpers — reusable across all providers
// =============================================================================

// Colored sub-tab button
inline bool ProviderTab(const char* label, const ImVec4& color, bool active) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::colors::BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
    bool clicked = ImGui::Button(label);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return clicked;
}

// Status message with fade
inline void StatusMsg(const std::string& msg, double time) {
    if (msg.empty()) return;
    double elapsed = ImGui::GetTime() - time;
    if (elapsed > 8.0) return;
    float alpha = elapsed < 6.0f ? 1.0f : 1.0f - static_cast<float>((elapsed - 6.0) / 2.0);
    bool is_error = msg.find("Error") != std::string::npos || msg.find("error") != std::string::npos;
    ImVec4 col = is_error ? theme::colors::ERROR_RED : theme::colors::SUCCESS;
    col.w = alpha;
    ImGui::TextColored(col, "%s", msg.c_str());
}

// Table header cell
inline void TableHeader(const char* label, const ImVec4& color) {
    ImGui::TableSetupColumn(label);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TableHeadersRow();
    ImGui::PopStyleColor();
}

// Data row with label and value
inline void InfoRow(const char* label, const char* value, const ImVec4& value_color = theme::colors::TEXT_PRIMARY) {
    ImGui::TextColored(theme::colors::TEXT_DIM, "%s", label);
    ImGui::SameLine(160);
    ImGui::TextColored(value_color, "%s", value);
}

// Fetch button
inline bool FetchButton(const char* label, bool loading, const ImVec4& color) {
    if (loading) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x, color.y, color.z, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x, color.y, color.z, 0.5f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 1.1f, color.y * 1.1f, color.z * 1.1f, 1.0f));
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 4));
    bool clicked = ImGui::Button(label) && !loading;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return clicked;
}

// Search input with button
inline bool SearchBar(char* buf, size_t buf_size, const char* hint, const ImVec4& color, bool loading) {
    ImGui::PushItemWidth(300);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    bool enter = ImGui::InputTextWithHint("##search", hint, buf, buf_size,
                                           ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::SameLine();
    bool clicked = FetchButton("SEARCH", loading, color);
    return enter || clicked;
}

// Empty state placeholder
inline void EmptyState(const char* msg, const char* sub = nullptr) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + avail.x * 0.3f,
                                ImGui::GetCursorPosY() + avail.y * 0.4f));
    ImGui::TextColored(theme::colors::TEXT_SECONDARY, "%s", msg);
    if (sub) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail.x * 0.25f);
        ImGui::TextColored(theme::colors::TEXT_DIM, "%s", sub);
    }
}

// Loading spinner centered
inline void LoadingState(const char* msg, const ImVec4& color) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + avail.x * 0.35f,
                                ImGui::GetCursorPosY() + avail.y * 0.4f));
    theme::LoadingSpinner(msg);
}

// Error state centered
inline void ErrorState(const std::string& error) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + avail.x * 0.2f,
                                ImGui::GetCursorPosY() + avail.y * 0.35f));
    ImGui::TextColored(theme::colors::ERROR_RED, "Error loading data");
    ImGui::Spacing();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail.x * 0.1f);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + avail.x * 0.6f);
    ImGui::TextColored(theme::colors::TEXT_DIM, "%s", error.c_str());
    ImGui::PopTextWrapPos();
}

} // namespace fincept::gov_data
