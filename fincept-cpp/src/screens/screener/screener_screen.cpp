// Screener Screen — FRED Economic Data Screener
// Bloomberg Terminal-style dense data layout with FRED API integration

#include "screener_screen.h"
#include "ui/theme.h"
#include "storage/database.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <sstream>
#include <set>

namespace fincept::screener {

using json = nlohmann::json;
using namespace theme::colors;

// Bloomberg-style colors
static constexpr ImVec4 BB_BG        = {0.055f, 0.055f, 0.060f, 1.0f};
static constexpr ImVec4 BB_PANEL     = {0.075f, 0.075f, 0.082f, 1.0f};
static constexpr ImVec4 BB_ROW_ALT   = {0.082f, 0.082f, 0.090f, 1.0f};
static constexpr ImVec4 BB_HEADER_BG = {0.090f, 0.090f, 0.098f, 1.0f};
static constexpr ImVec4 BB_AMBER     = {1.0f, 0.65f, 0.0f, 1.0f};
static constexpr ImVec4 BB_CYAN      = {0.0f, 0.84f, 0.44f, 1.0f};
static constexpr ImVec4 BB_GREEN     = {0.18f, 0.80f, 0.44f, 1.0f};
static constexpr ImVec4 BB_RED       = {0.95f, 0.30f, 0.25f, 1.0f};
static constexpr ImVec4 BB_BLUE      = {0.30f, 0.55f, 0.95f, 1.0f};
static constexpr ImVec4 BB_DIM       = {0.45f, 0.45f, 0.48f, 1.0f};
static constexpr ImVec4 BB_TEXT      = {0.85f, 0.85f, 0.87f, 1.0f};

static constexpr ImVec4 CHART_IMCOLORS[] = {
    {0.92f, 0.35f, 0.05f, 1.0f},  // orange
    {0.13f, 0.77f, 0.37f, 1.0f},  // green
    {0.23f, 0.51f, 0.96f, 1.0f},  // blue
    {0.92f, 0.70f, 0.03f, 1.0f},  // yellow
    {0.94f, 0.27f, 0.27f, 1.0f},  // red
};

// ============================================================================
// Initialization
// ============================================================================

void ScreenerScreen::init() {
    if (initialized_) return;
    initialized_ = true;

    // Set default end date to today
    const time_t now = time(nullptr);
    const struct tm* t = localtime(&now);
    std::snprintf(end_date_buf_, sizeof(end_date_buf_), "%04d-%02d-%02d",
                  t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

    category_path_.push_back({0, "All Categories"});
    check_api_key();
    parse_selected_ids();
}

// ============================================================================
// API Key
// ============================================================================

void ScreenerScreen::check_api_key() {
    const auto key = get_fred_api_key();
    api_key_configured_ = !key.empty();
}

std::string ScreenerScreen::get_fred_api_key() const {
    const auto cred = db::ops::get_credential_by_service("fred_api_key");
    if (cred && cred->api_key) return *cred->api_key;
    return "";
}

// ============================================================================
// Selected IDs parsing
// ============================================================================

void ScreenerScreen::parse_selected_ids() {
    selected_ids_.clear();
    std::string ids(series_ids_buf_);
    std::istringstream ss(ids);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // Trim whitespace
        const auto start = token.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        const auto end = token.find_last_not_of(" \t");
        selected_ids_.insert(token.substr(start, end - start + 1));
    }
}

void ScreenerScreen::toggle_series(const std::string& series_id) {
    parse_selected_ids();
    if (selected_ids_.count(series_id)) {
        selected_ids_.erase(series_id);
    } else {
        selected_ids_.insert(series_id);
    }
    // Rebuild buffer
    std::string result;
    for (const auto& id : selected_ids_) {
        if (!result.empty()) result += ",";
        result += id;
    }
    std::strncpy(series_ids_buf_, result.c_str(), sizeof(series_ids_buf_) - 1);
    series_ids_buf_[sizeof(series_ids_buf_) - 1] = '\0';
}

// ============================================================================
// FRED API helpers
// ============================================================================

FREDSeries ScreenerScreen::fetch_single_series(
    const std::string& series_id, const std::string& start_date,
    const std::string& end_date, const std::string& api_key) {

    FREDSeries result;
    result.series_id = series_id;

    auto& client = http::HttpClient::instance();

    // 1. Fetch series info
    const std::string info_url =
        "https://api.stlouisfed.org/fred/series?series_id=" + series_id +
        "&api_key=" + api_key + "&file_type=json";
    const auto info_resp = client.get(info_url);
    if (info_resp.success) {
        try {
            const auto j = json::parse(info_resp.body);
            if (j.contains("seriess") && j["seriess"].is_array() && !j["seriess"].empty()) {
                const auto& s = j["seriess"][0];
                result.title = s.value("title", "");
                result.units = s.value("units", "");
                result.frequency = s.value("frequency", "");
                result.seasonal_adjustment = s.value("seasonal_adjustment", "");
                result.last_updated = s.value("last_updated", "");
            }
        } catch (...) {}
    }

    // 2. Fetch observations
    const std::string obs_url =
        "https://api.stlouisfed.org/fred/series/observations?series_id=" + series_id +
        "&observation_start=" + start_date + "&observation_end=" + end_date +
        "&api_key=" + api_key + "&file_type=json";
    const auto obs_resp = client.get(obs_url);
    if (obs_resp.success) {
        try {
            const auto j = json::parse(obs_resp.body);
            if (j.contains("observations") && j["observations"].is_array()) {
                for (const auto& obs : j["observations"]) {
                    const std::string date = obs.value("date", "");
                    const std::string val_str = obs.value("value", ".");
                    if (val_str != "." && !val_str.empty()) {
                        try {
                            const double val = std::stod(val_str);
                            result.observations.push_back({date, val});
                        } catch (...) {}
                    }
                }
            }
            result.observation_count = static_cast<int>(result.observations.size());
        } catch (const std::exception& e) {
            result.error = e.what();
        }
    } else {
        result.error = "HTTP error: " + obs_resp.error;
    }

    return result;
}

std::vector<SearchResult> ScreenerScreen::search_fred(
    const std::string& query, const std::string& api_key, int limit) {

    std::vector<SearchResult> results;
    auto& client = http::HttpClient::instance();

    // URL-encode query (simple: replace spaces with +)
    std::string encoded_query;
    for (char c : query) {
        if (c == ' ') encoded_query += '+';
        else encoded_query += c;
    }

    const std::string url =
        "https://api.stlouisfed.org/fred/series/search?search_text=" + encoded_query +
        "&limit=" + std::to_string(limit) +
        "&api_key=" + api_key + "&file_type=json";

    const auto resp = client.get(url);
    if (resp.success) {
        try {
            const auto j = json::parse(resp.body);
            if (j.contains("seriess") && j["seriess"].is_array()) {
                for (const auto& s : j["seriess"]) {
                    SearchResult sr;
                    sr.id = s.value("id", "");
                    sr.title = s.value("title", "");
                    sr.frequency = s.value("frequency", "");
                    sr.units = s.value("units", "");
                    sr.seasonal_adjustment = s.value("seasonal_adjustment", "");
                    sr.last_updated = s.value("last_updated", "");
                    sr.popularity = s.value("popularity", 0);
                    results.push_back(std::move(sr));
                }
            }
        } catch (...) {}
    }
    return results;
}

std::vector<Category> ScreenerScreen::fetch_categories(
    int parent_id, const std::string& api_key) {

    std::vector<Category> results;
    auto& client = http::HttpClient::instance();

    const std::string url =
        "https://api.stlouisfed.org/fred/category/children?category_id=" +
        std::to_string(parent_id) + "&api_key=" + api_key + "&file_type=json";

    const auto resp = client.get(url);
    if (resp.success) {
        try {
            const auto j = json::parse(resp.body);
            if (j.contains("categories") && j["categories"].is_array()) {
                for (const auto& c : j["categories"]) {
                    Category cat;
                    cat.id = c.value("id", 0);
                    cat.name = c.value("name", "");
                    cat.parent_id = c.value("parent_id", 0);
                    results.push_back(std::move(cat));
                }
            }
        } catch (...) {}
    }
    return results;
}

std::vector<SearchResult> ScreenerScreen::fetch_category_series(
    int cat_id, const std::string& api_key, int limit) {

    std::vector<SearchResult> results;
    auto& client = http::HttpClient::instance();

    const std::string url =
        "https://api.stlouisfed.org/fred/category/series?category_id=" +
        std::to_string(cat_id) + "&limit=" + std::to_string(limit) +
        "&api_key=" + api_key + "&file_type=json";

    const auto resp = client.get(url);
    if (resp.success) {
        try {
            const auto j = json::parse(resp.body);
            if (j.contains("seriess") && j["seriess"].is_array()) {
                for (const auto& s : j["seriess"]) {
                    SearchResult sr;
                    sr.id = s.value("id", "");
                    sr.title = s.value("title", "");
                    sr.frequency = s.value("frequency", "");
                    sr.units = s.value("units", "");
                    sr.popularity = s.value("popularity", 0);
                    sr.last_updated = s.value("last_updated", "");
                    results.push_back(std::move(sr));
                }
            }
        } catch (...) {}
    }
    return results;
}

// ============================================================================
// Actions
// ============================================================================

void ScreenerScreen::fetch_data() {
    const auto api_key = get_fred_api_key();
    if (api_key.empty()) {
        error_ = "FRED API key not configured. Add it in Settings > Credentials.";
        api_key_configured_ = false;
        return;
    }

    parse_selected_ids();
    if (selected_ids_.empty()) {
        error_ = "Please enter at least one series ID.";
        return;
    }

    loading_ = true;
    error_.clear();
    loading_message_ = "Fetching " + std::to_string(selected_ids_.size()) + " series from FRED...";

    // Copy state for the async lambda
    const std::set<std::string> ids = selected_ids_;
    const std::string start = start_date_buf_;
    const std::string end = end_date_buf_;
    const std::string key = api_key;

    fetch_future_ = std::async(std::launch::async, [this, ids, start, end, key]() {
        std::vector<FREDSeries> results;
        for (const auto& id : ids) {
            results.push_back(fetch_single_series(id, start, end, key));
        }
        return results;
    });
}

void ScreenerScreen::search_series(const std::string& query) {
    const auto api_key = get_fred_api_key();
    if (api_key.empty()) return;

    search_loading_ = true;
    const std::string q = query;
    const std::string key = api_key;

    search_future_ = std::async(std::launch::async, [this, q, key]() {
        return search_fred(q, key, 50);
    });
}

void ScreenerScreen::load_categories(int category_id) {
    const auto api_key = get_fred_api_key();
    if (api_key.empty()) return;

    category_loading_ = true;
    const std::string key = api_key;

    category_future_ = std::async(std::launch::async, [this, category_id, key]() {
        return fetch_categories(category_id, key);
    });
}

void ScreenerScreen::load_category_series(int category_id) {
    const auto api_key = get_fred_api_key();
    if (api_key.empty()) return;

    category_series_loading_ = true;
    const std::string key = api_key;

    cat_series_future_ = std::async(std::launch::async, [this, category_id, key]() {
        return fetch_category_series(category_id, key, 50);
    });
}

void ScreenerScreen::export_csv() {
    if (data_.empty()) return;

    // Build date set
    std::set<std::string> all_dates;
    for (const auto& series : data_) {
        for (const auto& obs : series.observations) {
            all_dates.insert(obs.date);
        }
    }

    std::ostringstream csv;
    // Header
    csv << "Date";
    for (const auto& series : data_) {
        csv << "," << series.series_id;
    }
    csv << "\n";

    // Rows
    for (const auto& date : all_dates) {
        csv << date;
        for (const auto& series : data_) {
            const auto it = std::find_if(series.observations.begin(), series.observations.end(),
                [&](const Observation& o) { return o.date == date; });
            if (it != series.observations.end()) {
                char val_buf[32];
                std::snprintf(val_buf, sizeof(val_buf), "%.4f", it->value);
                csv << "," << val_buf;
            } else {
                csv << ",";
            }
        }
        csv << "\n";
    }

    // Write to file (desktop export)
    const time_t now = time(nullptr);
    const struct tm* t = localtime(&now);
    char filename[128];
    std::snprintf(filename, sizeof(filename), "fred_data_%04d-%02d-%02d.csv",
                  t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

    FILE* f = fopen(filename, "w");
    if (f) {
        const std::string content = csv.str();
        fwrite(content.data(), 1, content.size(), f);
        fclose(f);
        LOG_INFO("Screener", "Exported CSV: %s", filename);
    }
}

// ============================================================================
// Main render
// ============================================================================

void ScreenerScreen::render() {
    init();

    // Check async results
    if (fetch_future_.valid()) {
        if (fetch_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            data_ = fetch_future_.get();
            loading_ = false;
            loading_message_.clear();
            // Check for errors in any series
            for (const auto& s : data_) {
                if (!s.error.empty()) {
                    error_ = "Error fetching " + s.series_id + ": " + s.error;
                    break;
                }
            }
        }
    }
    if (search_future_.valid()) {
        if (search_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            search_results_ = search_future_.get();
            search_loading_ = false;
        }
    }
    if (category_future_.valid()) {
        if (category_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            categories_ = category_future_.get();
            category_loading_ = false;
        }
    }
    if (cat_series_future_.valid()) {
        if (cat_series_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            category_series_ = cat_series_future_.get();
            category_series_loading_ = false;
        }
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float tab_h = ImGui::GetFrameHeight() + 6;
    const float footer_h = ImGui::GetFrameHeight() + 4;
    const float x = vp->WorkPos.x;
    const float y = vp->WorkPos.y + tab_h;
    const float w = vp->WorkSize.x;
    const float total_h = vp->WorkSize.y - tab_h;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, total_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BB_BG);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##screener", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Header bar
    render_header(w);

    // API key warning (if not configured)
    if (!api_key_configured_) {
        render_api_key_warning(w);
    }

    // Query panel (series IDs, dates, fetch button, quick-add)
    render_query_panel(w);

    // Main content area: chart (left 2/3) + data tables (right 1/3)
    const float content_y = ImGui::GetCursorPosY();
    const float available_h = total_h - content_y - footer_h;
    const float chart_w = w * 0.65f;
    const float table_w = w - chart_w - 4;

    render_chart_panel(0, content_y, chart_w, available_h);
    render_data_tables(chart_w + 4, content_y, table_w, available_h);

    // Footer
    render_footer(w, total_h);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Browser modal (rendered on top)
    if (show_browser_) {
        render_browser_modal();
    }
}

// ============================================================================
// Header
// ============================================================================

void ScreenerScreen::render_header(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_PANEL);
    ImGui::BeginChild("##scr_header", ImVec2(w, 32), false);

    ImGui::SetCursorPos(ImVec2(12, 6));
    ImGui::TextColored(BB_AMBER, "ECONOMIC DATA SCREENER");

    // Right side: chart type + normalize + export buttons
    const float btn_x = w - 280;
    ImGui::SameLine(btn_x);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);

    if (ImGui::SmallButton(chart_type_ == ChartType::line ? "AREA" : "LINE")) {
        chart_type_ = (chart_type_ == ChartType::line) ? ChartType::area : ChartType::line;
    }
    ImGui::SameLine();

    if (data_.size() > 1) {
        ImGui::PushStyleColor(ImGuiCol_Text, normalize_data_ ? BB_GREEN : BB_DIM);
        if (ImGui::SmallButton(normalize_data_ ? "[NORM]" : "NORM")) {
            normalize_data_ = !normalize_data_;
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::TextColored(BB_DIM, "NORM");
    }
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Text, data_.empty() ? BB_DIM : BB_GREEN);
    if (ImGui::SmallButton("CSV") && !data_.empty()) {
        export_csv();
    }
    ImGui::PopStyleColor();

    ImGui::PopStyleColor(3);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// API Key Warning
// ============================================================================

void ScreenerScreen::render_api_key_warning(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.12f, 0.02f, 1.0f));
    ImGui::BeginChild("##scr_apikey_warn", ImVec2(w, 70), false);

    ImGui::SetCursorPos(ImVec2(16, 8));
    ImGui::TextColored(BB_AMBER, "FRED API Key Required");

    ImGui::SetCursorPos(ImVec2(16, 26));
    ImGui::TextColored(BB_TEXT, "Configure your FRED API key in Settings > Credentials to access economic data.");

    ImGui::SetCursorPos(ImVec2(16, 44));
    ImGui::TextColored(BB_DIM, "1. Get free key at research.stlouisfed.org  ");
    ImGui::SameLine();
    ImGui::TextColored(BB_DIM, "2. Add to Settings > Credentials > FRED_API_KEY  ");
    ImGui::SameLine();
    ImGui::TextColored(BB_DIM, "3. Reload this tab");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Query Panel
// ============================================================================

void ScreenerScreen::render_query_panel(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_PANEL);
    ImGui::BeginChild("##scr_query", ImVec2(w, 80), false);

    const float pad = 12.0f;
    ImGui::SetCursorPos(ImVec2(pad, 6));

    // Row 1: Series IDs input + dates + fetch button
    ImGui::TextColored(BB_DIM, "SERIES IDS (comma-separated)");
    ImGui::SameLine(0, 8);
    ImGui::PushItemWidth(w * 0.35f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.04f, 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, BB_TEXT);
    if (ImGui::InputText("##series_ids", series_ids_buf_, sizeof(series_ids_buf_))) {
        parse_selected_ids();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 16);
    ImGui::TextColored(BB_DIM, "FROM");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(90);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.04f, 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, BB_TEXT);
    ImGui::InputText("##start_date", start_date_buf_, sizeof(start_date_buf_));
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 8);
    ImGui::TextColored(BB_DIM, "TO");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(90);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.04f, 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, BB_TEXT);
    ImGui::InputText("##end_date", end_date_buf_, sizeof(end_date_buf_));
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 12);
    ImGui::PushStyleColor(ImGuiCol_Button, loading_ ? ImVec4(0.1f, 0.1f, 0.1f, 1.0f) : ImVec4(0.0f, 0.55f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.65f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    if (ImGui::Button(loading_ ? "LOADING..." : "FETCH", ImVec2(80, 0)) && !loading_) {
        fetch_data();
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0, 16);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
    if (ImGui::Button("BROWSE", ImVec2(70, 0)) && api_key_configured_) {
        show_browser_ = true;
        if (categories_.empty()) {
            load_categories(0);
        }
    }
    ImGui::PopStyleColor(3);

    // Row 2: Quick-add buttons
    ImGui::SetCursorPos(ImVec2(pad, 46));
    ImGui::TextColored(BB_DIM, "QUICK ADD");
    ImGui::SameLine(0, 8);

    for (int i = 0; i < POPULAR_SERIES_COUNT; i++) {
        const bool is_selected = selected_ids_.count(POPULAR_SERIES[i].id) > 0;
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.3f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, BB_GREEN);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.06f, 0.07f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);
        }
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.1f, 0.12f, 1.0f));

        if (ImGui::SmallButton(POPULAR_SERIES[i].id)) {
            toggle_series(POPULAR_SERIES[i].id);
        }
        ImGui::PopStyleColor(3);

        if (i < POPULAR_SERIES_COUNT - 1) ImGui::SameLine(0, 4);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Chart Panel
// ============================================================================

void ScreenerScreen::render_chart_panel(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_PANEL);
    ImGui::BeginChild("##scr_chart", ImVec2(w, h), false);

    const float pad = 12.0f;
    ImGui::SetCursorPos(ImVec2(pad, 8));
    ImGui::TextColored(BB_AMBER, "TIME SERIES CHART");

    if (loading_) {
        const float center_y = h * 0.4f;
        ImGui::SetCursorPos(ImVec2(w * 0.35f, center_y));
        ImGui::TextColored(BB_AMBER, "%s", loading_message_.c_str());
        ImGui::SetCursorPos(ImVec2(w * 0.35f, center_y + 20));
        ImGui::TextColored(BB_DIM, "Please wait...");
    } else if (!error_.empty()) {
        ImGui::SetCursorPos(ImVec2(pad, 40));
        ImGui::TextColored(BB_RED, "Error: %s", error_.c_str());
    } else if (data_.empty()) {
        const float center_y = h * 0.4f;
        ImGui::SetCursorPos(ImVec2(w * 0.3f, center_y));
        ImGui::TextColored(BB_DIM, "No data to display");
        ImGui::SetCursorPos(ImVec2(w * 0.25f, center_y + 20));
        ImGui::TextColored(BB_DIM, "Enter series IDs and click FETCH to load data");
    } else {
        // Render stacked line charts using ImGui DrawList
        const float chart_x = pad;
        const float chart_y = 30.0f;
        const float chart_w = w - pad * 2;
        const float chart_h = h - chart_y - 30.0f;

        if (chart_w > 50 && chart_h > 50) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 origin = ImGui::GetWindowPos();
            const ImVec2 p0(origin.x + chart_x, origin.y + chart_y);
            const ImVec2 p1(origin.x + chart_x + chart_w, origin.y + chart_y + chart_h);

            // Background
            draw->AddRectFilled(p0, p1, IM_COL32(10, 10, 12, 255));
            draw->AddRect(p0, p1, IM_COL32(60, 60, 65, 255));

            // Grid lines
            for (int gi = 1; gi < 5; gi++) {
                const float gy = p0.y + chart_h * gi / 5.0f;
                draw->AddLine(ImVec2(p0.x, gy), ImVec2(p1.x, gy), IM_COL32(40, 40, 45, 255));
            }

            // Find global min/max for normalization
            for (int si = 0; si < static_cast<int>(data_.size()); si++) {
                const auto& series = data_[si];
                if (series.observations.empty()) continue;

                double min_val = series.observations[0].value;
                double max_val = series.observations[0].value;
                for (const auto& obs : series.observations) {
                    min_val = std::min(min_val, obs.value);
                    max_val = std::max(max_val, obs.value);
                }

                if (normalize_data_) {
                    min_val = 0.0;
                    max_val = 100.0;
                }

                const double range = (max_val - min_val);
                const double safe_range = (range > 0.0) ? range : 1.0;
                const int n = static_cast<int>(series.observations.size());
                const int color_idx = si % 5;
                const ImU32 color = ImColor(CHART_IMCOLORS[color_idx]);

                auto normalize_val = [&](double v) -> double {
                    if (normalize_data_ && range > 0.0) {
                        return ((v - series.observations[0].value) /
                                (max_val - min_val)) * 100.0;  // 0-100 scale
                    }
                    return v;
                };

                // Draw the line
                for (int i = 1; i < n; i++) {
                    const double v0 = normalize_data_ ? normalize_val(series.observations[i-1].value)
                                                       : series.observations[i-1].value;
                    const double v1 = normalize_data_ ? normalize_val(series.observations[i].value)
                                                       : series.observations[i].value;

                    const float x0 = p0.x + chart_w * (i - 1) / static_cast<float>(n - 1);
                    const float x1 = p0.x + chart_w * i / static_cast<float>(n - 1);
                    const float y0 = p1.y - chart_h * static_cast<float>((v0 - min_val) / safe_range);
                    const float y1 = p1.y - chart_h * static_cast<float>((v1 - min_val) / safe_range);

                    draw->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), color, 1.5f);

                    // Fill for area chart
                    if (chart_type_ == ChartType::area) {
                        const ImU32 fill = (color & 0x00FFFFFF) | 0x30000000; // 20% opacity
                        draw->AddQuadFilled(
                            ImVec2(x0, y0), ImVec2(x1, y1),
                            ImVec2(x1, p1.y), ImVec2(x0, p1.y), fill);
                    }
                }

                // Y-axis labels (min/max)
                if (si == 0) {
                    char min_str[32], max_str[32];
                    std::snprintf(min_str, sizeof(min_str), "%.2f", min_val);
                    std::snprintf(max_str, sizeof(max_str), "%.2f", max_val);
                    draw->AddText(ImVec2(p0.x + 4, p1.y - 14), IM_COL32(120, 120, 130, 255), min_str);
                    draw->AddText(ImVec2(p0.x + 4, p0.y + 2), IM_COL32(120, 120, 130, 255), max_str);
                }
            }

            // Legend
            float legend_x = p0.x + 8;
            const float legend_y = p1.y + 4;
            for (int si = 0; si < static_cast<int>(data_.size()); si++) {
                const int ci = si % 5;
                const ImU32 lc = ImColor(CHART_IMCOLORS[ci]);
                draw->AddRectFilled(ImVec2(legend_x, legend_y + 2),
                                    ImVec2(legend_x + 10, legend_y + 10), lc);
                legend_x += 14;
                draw->AddText(ImVec2(legend_x, legend_y),
                              IM_COL32(180, 180, 185, 255), data_[si].series_id.c_str());
                legend_x += ImGui::CalcTextSize(data_[si].series_id.c_str()).x + 16;
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Data Tables (right panel — scrollable series cards)
// ============================================================================

void ScreenerScreen::render_data_tables(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_BG);
    ImGui::BeginChild("##scr_tables", ImVec2(w, h), false);

    for (const auto& series : data_) {
        render_series_table(series, w - 8);
        ImGui::Spacing();
    }

    if (data_.empty()) {
        ImGui::SetCursorPos(ImVec2(16, h * 0.3f));
        ImGui::TextColored(BB_DIM, "Series data will appear here");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void ScreenerScreen::render_series_table(const FREDSeries& series, float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_PANEL);
    const std::string id = "##srt_" + series.series_id;
    ImGui::BeginChild(id.c_str(), ImVec2(w, 220), true);

    // Header
    ImGui::TextColored(BB_AMBER, "%s", series.series_id.c_str());
    ImGui::TextColored(BB_TEXT, "%s", series.title.c_str());

    ImGui::Spacing();

    // Metadata grid
    ImGui::TextColored(BB_DIM, "Units: %s", series.units.c_str());
    ImGui::SameLine(w * 0.5f);
    ImGui::TextColored(BB_DIM, "Freq: %s", series.frequency.c_str());

    ImGui::TextColored(BB_DIM, "Obs: %d", series.observation_count);
    ImGui::SameLine(w * 0.5f);
    const std::string updated_short = series.last_updated.substr(0,
        std::min(series.last_updated.size(), static_cast<size_t>(10)));
    ImGui::TextColored(BB_DIM, "Updated: %s", updated_short.c_str());

    ImGui::Separator();

    // Recent observations table (last 15, reversed)
    if (ImGui::BeginTable("##obs", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg,
                           ImVec2(0, 130))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BB_HEADER_BG);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        const int n = static_cast<int>(series.observations.size());
        const int start = std::max(0, n - 15);
        for (int i = n - 1; i >= start; i--) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(BB_DIM, "%s", series.observations[i].date.c_str());
            ImGui::TableNextColumn();

            char val_buf[32];
            std::snprintf(val_buf, sizeof(val_buf), "%.4f", series.observations[i].value);
            ImGui::TextColored(BB_TEXT, "%s", val_buf);
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Footer
// ============================================================================

void ScreenerScreen::render_footer(float w, float total_h) {
    const float footer_h = ImGui::GetFrameHeight() + 4;
    ImGui::SetCursorPos(ImVec2(0, total_h - footer_h));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_PANEL);
    ImGui::BeginChild("##scr_footer", ImVec2(w, footer_h), false);

    ImGui::SetCursorPos(ImVec2(12, 3));
    ImGui::TextColored(BB_AMBER, "ECONOMIC SCREENER");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(BB_DIM, "FRED Economic Data | Federal Reserve Bank of St. Louis");

    if (!data_.empty()) {
        int total_obs = 0;
        for (const auto& s : data_) total_obs += s.observation_count;

        char stats[64];
        std::snprintf(stats, sizeof(stats), "%d series | %d observations",
                      static_cast<int>(data_.size()), total_obs);
        const float stats_w = ImGui::CalcTextSize(stats).x;
        ImGui::SameLine(w - stats_w - 16);
        ImGui::TextColored(BB_AMBER, "%s", stats);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Browser Modal
// ============================================================================

void ScreenerScreen::render_browser_modal() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float modal_w = vp->WorkSize.x * 0.85f;
    const float modal_h = vp->WorkSize.y * 0.8f;
    const float modal_x = vp->WorkPos.x + (vp->WorkSize.x - modal_w) * 0.5f;
    const float modal_y = vp->WorkPos.y + (vp->WorkSize.y - modal_h) * 0.5f;

    // Dim background
    ImGui::GetForegroundDrawList()->AddRectFilled(
        vp->WorkPos, ImVec2(vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y),
        IM_COL32(0, 0, 0, 200));

    ImGui::SetNextWindowPos(ImVec2(modal_x, modal_y));
    ImGui::SetNextWindowSize(ImVec2(modal_w, modal_h));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BB_BG);
    ImGui::PushStyleColor(ImGuiCol_Border, BB_AMBER);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##fred_browser", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking);

    // Modal header
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_PANEL);
        ImGui::BeginChild("##browser_hdr", ImVec2(modal_w, 36), false);

        ImGui::SetCursorPos(ImVec2(16, 8));
        ImGui::TextColored(BB_AMBER, "FRED SERIES BROWSER");

        // Close button
        ImGui::SameLine(modal_w - 60);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);
        if (ImGui::SmallButton("[X] CLOSE")) {
            show_browser_ = false;
        }
        ImGui::PopStyleColor(2);

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // Search bar
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.07f, 1.0f));
        ImGui::BeginChild("##browser_search", ImVec2(modal_w, 36), false);

        ImGui::SetCursorPos(ImVec2(16, 6));
        ImGui::PushItemWidth(modal_w - 40);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.04f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, BB_TEXT);
        if (ImGui::InputTextWithHint("##browser_q", "Search FRED series (e.g. unemployment, inflation, GDP)...",
                                      browser_search_buf_, sizeof(browser_search_buf_),
                                      ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (std::strlen(browser_search_buf_) > 0) {
                search_series(browser_search_buf_);
            }
        }
        ImGui::PopStyleColor(2);
        ImGui::PopItemWidth();

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // Content: left categories (if no search) + right results
    const float content_h = modal_h - 36 - 36 - 30; // header + search + footer
    const bool has_search = std::strlen(browser_search_buf_) > 0;
    const float cat_w = has_search ? 0 : 280;
    const float results_w = modal_w - cat_w;

    if (!has_search) {
        // Categories panel (left)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.055f, 1.0f));
        ImGui::BeginChild("##browser_cats", ImVec2(cat_w, content_h), true);
        render_browser_categories();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 0);
    }

    // Results panel (right)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_BG);
    ImGui::BeginChild("##browser_results", ImVec2(results_w, content_h), false);

    if (has_search) {
        render_browser_search_results();
    } else {
        render_browser_series_list();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Modal footer
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_PANEL);
        ImGui::BeginChild("##browser_ftr", ImVec2(modal_w, 30), false);
        ImGui::SetCursorPos(ImVec2(16, 6));
        ImGui::TextColored(BB_DIM, "Federal Reserve Economic Data (FRED) | 800,000+ series available");

        ImGui::SameLine(modal_w - 80);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.55f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        if (ImGui::SmallButton("DONE")) {
            show_browser_ = false;
        }
        ImGui::PopStyleColor(2);

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Browser sub-panels
// ============================================================================

void ScreenerScreen::render_browser_categories() {
    const float w = ImGui::GetContentRegionAvail().x;

    // Breadcrumb + back button
    if (category_path_.size() > 1) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.08f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
        if (ImGui::SmallButton("<< BACK")) {
            category_path_.pop_back();
            const int parent_id = category_path_.back().id;
            current_category_ = parent_id;
            load_categories(parent_id);
        }
        ImGui::PopStyleColor(2);
    }

    // Breadcrumb text
    std::string path_str;
    for (size_t i = 0; i < category_path_.size(); i++) {
        if (i > 0) path_str += " > ";
        path_str += category_path_[i].name;
    }
    ImGui::TextColored(BB_DIM, "%s", path_str.c_str());
    ImGui::Separator();

    // Popular categories (only at root)
    if (current_category_ == 0) {
        ImGui::TextColored(BB_AMBER, "POPULAR CATEGORIES");
        ImGui::Spacing();

        for (int i = 0; i < POPULAR_CATEGORIES_COUNT; i++) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.06f, 0.07f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.1f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, BB_TEXT);
            if (ImGui::Button(POPULAR_CATEGORIES[i].name, ImVec2(w - 8, 0))) {
                category_path_.push_back({POPULAR_CATEGORIES[i].id, POPULAR_CATEGORIES[i].name});
                current_category_ = POPULAR_CATEGORIES[i].id;
                load_categories(POPULAR_CATEGORIES[i].id);
                load_category_series(POPULAR_CATEGORIES[i].id);
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::Separator();
    }

    // Subcategories list
    if (category_loading_) {
        ImGui::TextColored(BB_DIM, "Loading categories...");
    } else {
        char lbl[64];
        std::snprintf(lbl, sizeof(lbl), "SUBCATEGORIES (%d)", static_cast<int>(categories_.size()));
        ImGui::TextColored(BB_DIM, "%s", lbl);
        ImGui::Spacing();

        for (const auto& cat : categories_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.06f, 0.07f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.1f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, BB_TEXT);
            const std::string cat_btn = cat.name + " >##cat" + std::to_string(cat.id);
            if (ImGui::Button(cat_btn.c_str(), ImVec2(w - 8, 0))) {
                category_path_.push_back({cat.id, cat.name});
                current_category_ = cat.id;
                load_categories(cat.id);
                load_category_series(cat.id);
            }
            ImGui::PopStyleColor(3);
        }
    }
}

void ScreenerScreen::render_browser_search_results() {
    if (search_loading_) {
        ImGui::SetCursorPos(ImVec2(16, 16));
        ImGui::TextColored(BB_AMBER, "Searching FRED...");
        return;
    }

    char hdr[64];
    std::snprintf(hdr, sizeof(hdr), "SEARCH RESULTS (%d)", static_cast<int>(search_results_.size()));
    ImGui::SetCursorPos(ImVec2(16, 8));
    ImGui::TextColored(BB_AMBER, "%s", hdr);

    if (search_results_.empty()) {
        ImGui::SetCursorPos(ImVec2(16, 30));
        ImGui::TextColored(BB_DIM, "No results found");
        return;
    }

    ImGui::SetCursorPosY(28);
    for (const auto& sr : search_results_) {
        render_search_result_row(sr);
    }
}

void ScreenerScreen::render_browser_series_list() {
    if (category_series_loading_) {
        ImGui::SetCursorPos(ImVec2(16, 16));
        ImGui::TextColored(BB_AMBER, "Loading series...");
        return;
    }

    if (category_series_.empty()) {
        ImGui::SetCursorPos(ImVec2(16, 40));
        ImGui::TextColored(BB_DIM, "Search or browse categories to find series");
        return;
    }

    char hdr[64];
    std::snprintf(hdr, sizeof(hdr), "SERIES IN CATEGORY (%d)", static_cast<int>(category_series_.size()));
    ImGui::SetCursorPos(ImVec2(16, 8));
    ImGui::TextColored(BB_AMBER, "%s", hdr);

    ImGui::SetCursorPosY(28);
    for (const auto& sr : category_series_) {
        render_search_result_row(sr);
    }
}

void ScreenerScreen::render_search_result_row(const SearchResult& result) {
    parse_selected_ids();
    const bool is_added = selected_ids_.count(result.id) > 0;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BB_PANEL);
    const std::string child_id = "##sr_" + result.id;
    ImGui::BeginChild(child_id.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 16, 60), true);

    ImGui::SetCursorPos(ImVec2(8, 4));
    ImGui::TextColored(BB_AMBER, "%s", result.id.c_str());
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BB_DIM, "Pop: %d", result.popularity);

    // Add button (right side)
    const float btn_x = ImGui::GetContentRegionAvail().x - 50;
    ImGui::SameLine(btn_x);
    if (is_added) {
        ImGui::TextColored(BB_GREEN, "ADDED");
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.55f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        const std::string btn_id = "+ADD##add_" + result.id;
        if (ImGui::SmallButton(btn_id.c_str())) {
            toggle_series(result.id);
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::SetCursorPos(ImVec2(8, 22));
    ImGui::TextColored(BB_TEXT, "%s", result.title.c_str());

    ImGui::SetCursorPos(ImVec2(8, 40));
    ImGui::TextColored(BB_DIM, "Freq: %s  |  Units: %s  |  Updated: %s",
                        result.frequency.c_str(), result.units.c_str(),
                        result.last_updated.substr(0, 10).c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

} // namespace fincept::screener
