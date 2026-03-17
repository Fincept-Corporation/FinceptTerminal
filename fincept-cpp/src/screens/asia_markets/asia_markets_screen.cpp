// Asia Markets Screen — Asian exchange data explorer with 398+ stock endpoints
// Port of AsiaMarketsTab.tsx — category tabs, region selector, endpoint browser,
// data table with sorting/pagination, favorites, history

#include "asia_markets_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include "python/python_runner.h"
#include "storage/database.h"
#include "core/logger.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <chrono>
#include <sstream>

namespace fincept::asia_markets {

using namespace theme::colors;

// ============================================================================
// Static catalogs
// ============================================================================

static std::vector<StockCategory> build_categories() {
    return {
        {"realtime",   "REALTIME",    "akshare_stocks_realtime.py",
         "Live stock prices, A/B/HK/US shares",
         {0.05f, 0.85f, 0.42f, 1.0f}},   // green
        {"historical", "HISTORICAL",  "akshare_stocks_historical.py",
         "Daily, minute, tick, intraday data",
         {0.0f, 0.53f, 1.0f, 1.0f}},     // blue
        {"financial",  "FINANCIAL",   "akshare_stocks_financial.py",
         "Statements, earnings, dividends",
         {0.62f, 0.31f, 0.87f, 1.0f}},   // purple
        {"holders",    "HOLDERS",     "akshare_stocks_holders.py",
         "Shareholders, institutional holdings",
         {1.0f, 0.84f, 0.0f, 1.0f}},     // yellow
        {"funds",      "FUND FLOW",   "akshare_stocks_funds.py",
         "Capital flow, block trades, LHB, IPO",
         {1.0f, 0.23f, 0.23f, 1.0f}},    // red
        {"board",      "BOARDS",      "akshare_stocks_board.py",
         "Industry, concept boards, rankings",
         {0.0f, 0.90f, 1.0f, 1.0f}},     // cyan
        {"margin",     "MARGIN/HSGT", "akshare_stocks_margin.py",
         "Margin trading, Stock Connect, PE/PB",
         {1.0f, 0.53f, 0.0f, 1.0f}},     // orange
        {"hot",        "HOT & NEWS",  "akshare_stocks_hot.py",
         "Hot stocks, news, ESG, sentiment",
         {1.0f, 0.42f, 0.42f, 1.0f}},    // red-light
    };
}

static std::vector<MarketRegion> build_regions() {
    return {
        {"CN_A", "CHINA A-SHARES", "CN"},
        {"CN_B", "CHINA B-SHARES", "CN"},
        {"HK",   "HONG KONG",      "HK"},
        {"US",   "US MARKETS",     "US"},
    };
}

// ============================================================================
// Helpers
// ============================================================================

static int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

static std::string timestamp_to_str(int64_t ts) {
    time_t t = static_cast<time_t>(ts / 1000);
    struct tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);
    return buf;
}

// ============================================================================
// Initialization
// ============================================================================

void AsiaMarketsScreen::init() {
    categories_catalog_ = build_categories();
    regions_ = build_regions();
    load_history_and_favorites();
    // Auto-load endpoints for first category
    if (!categories_catalog_.empty())
        load_endpoints(categories_catalog_[0]);
}

// ============================================================================
// Async helpers
// ============================================================================

bool AsiaMarketsScreen::is_async_busy() const {
    return async_task_.valid() &&
           async_task_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready;
}

void AsiaMarketsScreen::check_async() {
    if (async_task_.valid() &&
        async_task_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        try { async_task_.get(); } catch (...) {}
    }
}

// ============================================================================
// Data fetching
// ============================================================================

void AsiaMarketsScreen::load_endpoints(const StockCategory& cat) {
    if (is_async_busy()) return;
    endpoints_status_ = AsyncStatus::Loading;
    endpoints_.clear();
    endpoint_groups_.clear();
    expanded_groups_.clear();
    selected_endpoint_.clear();

    std::string script = cat.script;
    async_task_ = std::async(std::launch::async, [this, script]() {
        auto result = python::execute(script, {"--list-endpoints"});
        if (!result.success || result.output.empty()) {
            endpoints_status_ = AsyncStatus::Error;
            error_ = result.error.empty() ? "Failed to load endpoints" : result.error;
            return;
        }
        try {
            auto j = nlohmann::json::parse(result.output);
            nlohmann::json payload = j;
            if (j.contains("data") && j["data"].is_object()) payload = j["data"];
            if (payload.contains("data") && payload["data"].is_object()) payload = payload["data"];

            // Extract endpoints
            if (payload.contains("available_endpoints") && payload["available_endpoints"].is_array()) {
                for (auto& ep : payload["available_endpoints"])
                    endpoints_.push_back(ep.get<std::string>());
            } else if (payload.contains("endpoints") && payload["endpoints"].is_array()) {
                for (auto& ep : payload["endpoints"])
                    endpoints_.push_back(ep.get<std::string>());
            }

            // Extract categories/groups
            if (payload.contains("categories") && payload["categories"].is_object()) {
                for (auto it = payload["categories"].begin(); it != payload["categories"].end(); ++it) {
                    std::vector<std::string> ep_list;
                    if (it.value().is_array()) {
                        for (auto& e : it.value()) ep_list.push_back(e.get<std::string>());
                    }
                    endpoint_groups_[it.key()] = std::move(ep_list);
                }
                if (!endpoint_groups_.empty())
                    expanded_groups_.insert(endpoint_groups_.begin()->first);
            }

            endpoints_status_ = AsyncStatus::Success;
        } catch (const std::exception& ex) {
            endpoints_status_ = AsyncStatus::Error;
            error_ = std::string("Parse error: ") + ex.what();
        }
    });
}

void AsiaMarketsScreen::execute_query(const std::string& script, const std::string& endpoint) {
    if (is_async_busy()) return;
    data_status_ = AsyncStatus::Loading;
    data_ = nlohmann::json::array();
    columns_.clear();
    row_count_ = 0;
    error_.clear();
    response_info_.clear();

    std::vector<std::string> args;
    args.push_back(endpoint);
    // Pass symbol if relevant
    if (std::strlen(symbol_buf_) > 0 &&
        (endpoint.find("stock") != std::string::npos ||
         endpoint.find("holder") != std::string::npos ||
         endpoint.find("individual") != std::string::npos)) {
        args.emplace_back(symbol_buf_);
    }

    std::string script_copy = script;
    std::string ep_copy = endpoint;

    async_task_ = std::async(std::launch::async, [this, script_copy, ep_copy, args]() {
        auto result = python::execute(script_copy, args);
        if (!result.success || result.output.empty()) {
            data_status_ = AsyncStatus::Error;
            error_ = result.error.empty() ? "Query failed" : result.error;
            add_history(script_copy, ep_copy, false, 0);
            return;
        }
        try {
            auto j = nlohmann::json::parse(result.output);
            parse_response(j);
            add_history(script_copy, ep_copy, data_status_ == AsyncStatus::Success, row_count_);
        } catch (const std::exception& ex) {
            data_status_ = AsyncStatus::Error;
            error_ = std::string("Parse error: ") + ex.what();
            add_history(script_copy, ep_copy, false, 0);
        }
    });
}

void AsiaMarketsScreen::parse_response(const nlohmann::json& j) {
    nlohmann::json payload = j;
    if (j.contains("data")) payload = j["data"];
    if (payload.is_object() && payload.contains("data")) payload = payload["data"];

    nlohmann::json arr = nlohmann::json::array();
    if (payload.is_array()) {
        arr = payload;
    } else if (payload.is_object()) {
        for (auto it = payload.begin(); it != payload.end(); ++it) {
            if (it.value().is_array() && !it.value().empty()) { arr = it.value(); break; }
        }
    }

    if (arr.empty() || !arr.is_array()) {
        data_status_ = AsyncStatus::Error;
        error_ = "No data records found in response";
        return;
    }

    columns_.clear();
    if (arr[0].is_object()) {
        for (auto it = arr[0].begin(); it != arr[0].end(); ++it)
            columns_.push_back(it.key());
    }

    data_ = arr;
    row_count_ = static_cast<int>(arr.size());
    response_info_ = std::to_string(row_count_) + " rows | " + timestamp_to_str(now_ms());
    data_status_ = AsyncStatus::Success;
}

// ============================================================================
// History & Favorites persistence
// ============================================================================

void AsiaMarketsScreen::load_history_and_favorites() {
    auto hist_opt = db::ops::get_setting("asia_markets_history");
    std::string hist_str = hist_opt.value_or("");
    if (!hist_str.empty()) {
        try {
            auto j = nlohmann::json::parse(hist_str);
            history_.clear();
            for (auto& item : j) {
                QueryHistoryItem h;
                h.id        = item.value("id", "");
                h.script    = item.value("script", "");
                h.endpoint  = item.value("endpoint", "");
                h.timestamp = item.value("timestamp", (int64_t)0);
                h.success   = item.value("success", false);
                h.count     = item.value("count", 0);
                history_.push_back(std::move(h));
            }
        } catch (...) {}
    }

    auto fav_opt = db::ops::get_setting("asia_markets_favorites");
    std::string fav_str = fav_opt.value_or("");
    if (!fav_str.empty()) {
        try {
            auto j = nlohmann::json::parse(fav_str);
            favorites_.clear();
            for (auto& item : j) {
                FavoriteEndpoint f;
                f.script   = item.value("script", "");
                f.endpoint = item.value("endpoint", "");
                f.name     = item.value("name", "");
                favorites_.push_back(std::move(f));
            }
        } catch (...) {}
    }
}

void AsiaMarketsScreen::save_history() {
    nlohmann::json j = nlohmann::json::array();
    for (auto& h : history_) {
        j.push_back({{"id", h.id}, {"script", h.script}, {"endpoint", h.endpoint},
                      {"timestamp", h.timestamp}, {"success", h.success}, {"count", h.count}});
    }
    db::ops::save_setting("asia_markets_history", j.dump());
}

void AsiaMarketsScreen::save_favorites() {
    nlohmann::json j = nlohmann::json::array();
    for (auto& f : favorites_) {
        j.push_back({{"script", f.script}, {"endpoint", f.endpoint}, {"name", f.name}});
    }
    db::ops::save_setting("asia_markets_favorites", j.dump());
}

void AsiaMarketsScreen::add_history(const std::string& script, const std::string& endpoint,
                                     bool success, int count) {
    QueryHistoryItem h;
    h.id        = std::to_string(now_ms());
    h.script    = script;
    h.endpoint  = endpoint;
    h.timestamp = now_ms();
    h.success   = success;
    h.count     = count;
    history_.insert(history_.begin(), std::move(h));
    if (history_.size() > 50) history_.resize(50);
    save_history();
}

bool AsiaMarketsScreen::is_favorite(const std::string& script, const std::string& endpoint) const {
    return std::any_of(favorites_.begin(), favorites_.end(),
        [&](const FavoriteEndpoint& f) { return f.script == script && f.endpoint == endpoint; });
}

void AsiaMarketsScreen::toggle_favorite(const std::string& script, const std::string& endpoint) {
    auto it = std::find_if(favorites_.begin(), favorites_.end(),
        [&](const FavoriteEndpoint& f) { return f.script == script && f.endpoint == endpoint; });
    if (it != favorites_.end()) {
        favorites_.erase(it);
    } else {
        favorites_.push_back({script, endpoint, endpoint});
    }
    save_favorites();
}

// ============================================================================
// Main render
// ============================================================================

void AsiaMarketsScreen::render() {
    if (!initialized_) { init(); initialized_ = true; }
    check_async();

    ui::ScreenFrame frame("##asia_markets_screen", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    float w = frame.width();
    float h = frame.height();

    render_header(w);
    render_category_bar(w);
    render_region_bar(w);

    float main_h = h - 110.0f; // header + category + region + status
    float bottom_h = bottom_minimized_ ? 28.0f : 180.0f;
    float content_h = main_h - bottom_h;

    render_main_area(w, content_h);
    render_bottom_panel(w, bottom_h);
    render_status_bar(w);

    frame.end();
}

// ============================================================================
// Header
// ============================================================================

void AsiaMarketsScreen::render_header(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##am_header", ImVec2(w, 32), false);

    ImGui::SetCursorPos(ImVec2(8, 6));
    ImGui::TextColored(ACCENT, "Asia Markets");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_SECONDARY, "Explorer");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "| 398+ Stock Endpoints | 8 Categories");

    // Symbol input on right
    float sym_x = w - 240.0f;
    ImGui::SameLine(sym_x);
    ImGui::Text("Symbol:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputText("##am_sym", symbol_buf_, sizeof(symbol_buf_));

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Category tab bar (8 stock categories)
// ============================================================================

void AsiaMarketsScreen::render_category_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##am_catbar", ImVec2(w, 30), false);

    ImGui::SetCursorPos(ImVec2(4, 3));
    for (int i = 0; i < (int)categories_catalog_.size(); i++) {
        auto& cat = categories_catalog_[i];
        bool active = (i == active_category_idx_);

        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(cat.color.x, cat.color.y, cat.color.z, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_Text, cat.color);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }

        if (ImGui::SmallButton(cat.name.c_str())) {
            if (i != active_category_idx_) {
                active_category_idx_ = i;
                load_endpoints(cat);
            }
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 4);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Market region selector
// ============================================================================

void AsiaMarketsScreen::render_region_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##am_regions", ImVec2(w, 24), false);

    ImGui::SetCursorPos(ImVec2(4, 3));
    ImGui::TextColored(TEXT_DIM, "Region:");
    ImGui::SameLine();

    for (int i = 0; i < (int)regions_.size(); i++) {
        auto& r = regions_[i];
        bool active = (i == active_region_idx_);

        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        std::string label = "[" + r.flag + "] " + r.name;
        if (ImGui::SmallButton(label.c_str())) {
            active_region_idx_ = i;
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 6);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Main area: sidebar + data panel
// ============================================================================

void AsiaMarketsScreen::render_main_area(float w, float h) {
    float sidebar_w = 260.0f;
    float data_w = w - sidebar_w - 4.0f;

    ImGui::BeginGroup();
    {
        ImGui::BeginChild("##am_sidebar", ImVec2(sidebar_w, h), false);
        render_endpoint_sidebar(sidebar_w, h);
        ImGui::EndChild();

        ImGui::SameLine(0, 2);

        ImGui::BeginChild("##am_data", ImVec2(data_w, h), false);
        render_data_panel(data_w, h);
        ImGui::EndChild();
    }
    ImGui::EndGroup();
}

// ============================================================================
// Endpoint sidebar
// ============================================================================

void AsiaMarketsScreen::render_endpoint_sidebar(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    // Category description
    if (active_category_idx_ >= 0 && active_category_idx_ < (int)categories_catalog_.size()) {
        auto& cat = categories_catalog_[active_category_idx_];
        ImGui::TextColored(cat.color, "%s", cat.name.c_str());
        ImGui::TextColored(TEXT_DIM, "%s", cat.description.c_str());
        ImGui::TextColored(TEXT_DIM, "%d endpoints", (int)endpoints_.size());
    }

    // Search
    ImGui::SetNextItemWidth(w - 16);
    ImGui::InputTextWithHint("##am_ep_search", "Filter endpoints...", search_buf_, sizeof(search_buf_));
    ImGui::Separator();

    // Loading states
    if (endpoints_status_ == AsyncStatus::Loading) {
        ImGui::TextColored(WARNING, "Loading endpoints...");
        ImGui::PopStyleColor();
        return;
    }
    if (endpoints_status_ == AsyncStatus::Error) {
        ImGui::TextColored(ERROR_RED, "Error loading endpoints");
        ImGui::TextWrapped("%s", error_.c_str());
        ImGui::PopStyleColor();
        return;
    }

    std::string search_lower(search_buf_);
    for (auto& c : search_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::string current_script = (active_category_idx_ >= 0) ?
        categories_catalog_[active_category_idx_].script : "";

    // Endpoint list
    ImGui::BeginChild("##am_ep_list", ImVec2(0, 0), false);

    if (!endpoint_groups_.empty()) {
        for (auto& kv : endpoint_groups_) {
            const auto& group_name = kv.first;
            const auto& eps = kv.second;
            bool expanded = expanded_groups_.count(group_name) > 0;

            if (ImGui::TreeNodeEx(group_name.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
                if (!expanded) expanded_groups_.insert(group_name);
                for (auto& ep : eps) {
                    if (!search_lower.empty()) {
                        std::string ep_lower = ep;
                        for (auto& c : ep_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        if (ep_lower.find(search_lower) == std::string::npos) continue;
                    }

                    bool is_sel = (ep == selected_endpoint_);
                    bool is_fav = is_favorite(current_script, ep);

                    ImGui::PushID(ep.c_str());
                    if (is_fav) { ImGui::TextColored(WARNING, "*"); ImGui::SameLine(); }
                    if (ImGui::Selectable(ep.c_str(), is_sel)) {
                        selected_endpoint_ = ep;
                        execute_query(current_script, ep);
                    }
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                        toggle_favorite(current_script, ep);
                    ImGui::PopID();
                }
                ImGui::TreePop();
            } else {
                expanded_groups_.erase(group_name);
            }
        }
    } else {
        for (int i = 0; i < (int)endpoints_.size(); i++) {
            auto& ep = endpoints_[i];
            if (!search_lower.empty()) {
                std::string ep_lower = ep;
                for (auto& c : ep_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (ep_lower.find(search_lower) == std::string::npos) continue;
            }

            bool is_sel = (ep == selected_endpoint_);
            bool is_fav = is_favorite(current_script, ep);

            ImGui::PushID(i);
            if (is_fav) { ImGui::TextColored(WARNING, "*"); ImGui::SameLine(); }
            if (ImGui::Selectable(ep.c_str(), is_sel)) {
                selected_endpoint_ = ep;
                execute_query(current_script, ep);
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                toggle_favorite(current_script, ep);
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Data panel
// ============================================================================

void AsiaMarketsScreen::render_data_panel(float w, float h) {
    // Toolbar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##am_toolbar", ImVec2(w, 28), false);
    ImGui::SetCursorPos(ImVec2(4, 4));

    bool is_table = (view_mode_ == ViewMode::Table);
    if (ImGui::SmallButton(is_table ? "[Table]" : "Table")) view_mode_ = ViewMode::Table;
    ImGui::SameLine();
    if (ImGui::SmallButton(!is_table ? "[JSON]" : "JSON")) view_mode_ = ViewMode::JSON;

    if (!response_info_.empty()) {
        ImGui::SameLine(0, 20);
        ImGui::TextColored(TEXT_DIM, "%s", response_info_.c_str());
    }

    if (!selected_endpoint_.empty() && active_category_idx_ >= 0) {
        ImGui::SameLine(w - 80);
        if (ImGui::SmallButton("Refresh") && !is_async_busy()) {
            execute_query(categories_catalog_[active_category_idx_].script, selected_endpoint_);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Content
    float content_h = h - 28;
    if (data_status_ == AsyncStatus::Loading) {
        ImGui::SetCursorPos(ImVec2(w / 2 - 60, h / 2));
        ImGui::TextColored(WARNING, "Fetching data...");
    } else if (data_status_ == AsyncStatus::Error) {
        ImGui::SetCursorPos(ImVec2(12, 36));
        ImGui::TextColored(ERROR_RED, "Error: %s", error_.c_str());
    } else if (data_status_ == AsyncStatus::Idle || row_count_ == 0) {
        ImGui::SetCursorPos(ImVec2(w / 2 - 120, h / 2));
        ImGui::TextColored(TEXT_DIM, "Select an endpoint from the sidebar to query data");
    } else {
        if (view_mode_ == ViewMode::Table)
            render_table_view(w, content_h);
        else
            render_json_view(w, content_h);
    }
}

// ============================================================================
// Table view
// ============================================================================

void AsiaMarketsScreen::render_table_view(float w, float h) {
    if (columns_.empty() || data_.empty()) return;

    int n_cols = static_cast<int>(columns_.size());
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                            ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable;

    if (!ImGui::BeginTable("##am_table", n_cols, flags, ImVec2(w - 4, h))) return;

    for (int c = 0; c < n_cols; c++)
        ImGui::TableSetupColumn(columns_[c].c_str(), ImGuiTableColumnFlags_DefaultSort);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    // Handle sorting
    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
        if (specs->SpecsDirty && specs->SpecsCount > 0) {
            sort_col_ = specs->Specs[0].ColumnIndex;
            sort_asc_ = (specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
            specs->SpecsDirty = false;
        }
    }

    // Build sorted index
    int total = static_cast<int>(data_.size());
    std::vector<int> indices(total);
    for (int i = 0; i < total; i++) indices[i] = i;

    if (sort_col_ >= 0 && sort_col_ < n_cols) {
        const std::string& col_key = columns_[sort_col_];
        std::stable_sort(indices.begin(), indices.end(), [&](int a, int b) {
            const auto va = data_[a].value(col_key, nlohmann::json());
            const auto vb = data_[b].value(col_key, nlohmann::json());
            bool less;
            if (va.is_number() && vb.is_number())
                less = va.get<double>() < vb.get<double>();
            else
                less = va.dump() < vb.dump();
            return sort_asc_ ? less : !less;
        });
    }

    // Render rows (show up to 500 for performance)
    int max_rows = std::min(total, 500);
    for (int ri = 0; ri < max_rows; ri++) {
        int idx = indices[ri];
        ImGui::TableNextRow();
        for (int c = 0; c < n_cols; c++) {
            ImGui::TableSetColumnIndex(c);
            const auto val = data_[idx].value(columns_[c], nlohmann::json());
            if (val.is_null())
                ImGui::TextColored(TEXT_DISABLED, "-");
            else if (val.is_number_float())
                ImGui::Text("%.4f", val.get<double>());
            else if (val.is_number_integer())
                ImGui::Text("%lld", (long long)val.get<int64_t>());
            else if (val.is_string())
                ImGui::TextUnformatted(val.get<std::string>().c_str());
            else
                ImGui::TextUnformatted(val.dump().c_str());
        }
    }

    if (total > max_rows) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(TEXT_DIM, "... %d more rows (showing first %d)", total - max_rows, max_rows);
    }

    ImGui::EndTable();
}

// ============================================================================
// JSON view
// ============================================================================

void AsiaMarketsScreen::render_json_view(float w, float h) {
    ImGui::BeginChild("##am_json", ImVec2(w - 4, h), true);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
    int max_rows = std::min((int)data_.size(), 100);
    for (int i = 0; i < max_rows; i++) {
        ImGui::TextWrapped("[%d] %s", i, data_[i].dump(2).c_str());
        ImGui::Separator();
    }
    if ((int)data_.size() > max_rows)
        ImGui::TextColored(TEXT_DIM, "... %d more rows", (int)data_.size() - max_rows);
    ImGui::PopStyleColor();
    ImGui::EndChild();
}

// ============================================================================
// Bottom panel (Favorites / History / Data info)
// ============================================================================

void AsiaMarketsScreen::render_bottom_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##am_bottom", ImVec2(w, h), true);

    // Tab bar + minimize toggle
    if (ImGui::SmallButton(bottom_minimized_ ? "^" : "v")) {
        bottom_minimized_ = !bottom_minimized_;
    }
    ImGui::SameLine();

    bool is_data = (bottom_tab_ == BottomTab::Data);
    bool is_fav  = (bottom_tab_ == BottomTab::Favorites);
    bool is_hist = (bottom_tab_ == BottomTab::History);

    if (ImGui::SmallButton(is_data ? "[Data]" : "Data")) bottom_tab_ = BottomTab::Data;
    ImGui::SameLine();
    if (ImGui::SmallButton(is_fav ? "[Favorites]" : "Favorites")) bottom_tab_ = BottomTab::Favorites;
    ImGui::SameLine();
    if (ImGui::SmallButton(is_hist ? "[History]" : "History")) bottom_tab_ = BottomTab::History;

    if (!bottom_minimized_) {
        ImGui::Separator();
        float panel_h = h - 32;
        switch (bottom_tab_) {
            case BottomTab::Data:      render_bottom_data(w, panel_h); break;
            case BottomTab::Favorites: render_bottom_favorites(w, panel_h); break;
            case BottomTab::History:   render_bottom_history(w, panel_h); break;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AsiaMarketsScreen::render_bottom_data(float w, float h) {
    ImGui::BeginChild("##bd_data", ImVec2(0, h), false);
    if (data_status_ == AsyncStatus::Success && row_count_ > 0) {
        ImGui::TextColored(TEXT_PRIMARY, "Current Query: %s", selected_endpoint_.c_str());
        ImGui::TextColored(TEXT_DIM, "%s", response_info_.c_str());
        ImGui::TextColored(TEXT_DIM, "Columns: %d | Rows: %d", (int)columns_.size(), row_count_);
    } else {
        ImGui::TextColored(TEXT_DIM, "No data loaded");
    }
    ImGui::EndChild();
}

void AsiaMarketsScreen::render_bottom_favorites(float w, float h) {
    ImGui::BeginChild("##bd_fav", ImVec2(0, h), false);
    if (favorites_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No favorites — right-click an endpoint to add");
    } else {
        for (int i = 0; i < (int)favorites_.size(); i++) {
            auto& f = favorites_[i];
            ImGui::PushID(i);
            ImGui::TextColored(WARNING, "*");
            ImGui::SameLine();
            if (ImGui::Selectable(f.endpoint.c_str(), false, 0, ImVec2(w - 80, 0))) {
                // Find category with matching script and execute
                for (int ci = 0; ci < (int)categories_catalog_.size(); ci++) {
                    if (categories_catalog_[ci].script == f.script) {
                        active_category_idx_ = ci;
                        load_endpoints(categories_catalog_[ci]);
                        break;
                    }
                }
                selected_endpoint_ = f.endpoint;
                execute_query(f.script, f.endpoint);
            }
            ImGui::SameLine(w - 60);
            if (ImGui::SmallButton("X")) {
                toggle_favorite(f.script, f.endpoint);
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void AsiaMarketsScreen::render_bottom_history(float w, float h) {
    ImGui::BeginChild("##bd_hist", ImVec2(0, h), false);
    if (history_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No queries yet");
    } else {
        for (int i = 0; i < (int)history_.size() && i < 30; i++) {
            auto& hi = history_[i];
            ImVec4 color = hi.success ? SUCCESS : ERROR_RED;
            ImGui::TextColored(color, "%s", hi.success ? "OK" : "ERR");
            ImGui::SameLine();
            if (ImGui::Selectable(hi.endpoint.c_str(), false, 0, ImVec2(w - 120, 0))) {
                for (int ci = 0; ci < (int)categories_catalog_.size(); ci++) {
                    if (categories_catalog_[ci].script == hi.script) {
                        active_category_idx_ = ci;
                        break;
                    }
                }
                selected_endpoint_ = hi.endpoint;
                execute_query(hi.script, hi.endpoint);
            }
            ImGui::SameLine(w - 100);
            ImGui::TextColored(TEXT_DIM, "%d rows", hi.count);
        }

        if (ImGui::SmallButton("Clear History")) {
            history_.clear();
            save_history();
        }
    }
    ImGui::EndChild();
}

// ============================================================================
// Status bar
// ============================================================================

void AsiaMarketsScreen::render_status_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##am_status", ImVec2(w, 24), false);
    ImGui::SetCursorPos(ImVec2(8, 4));

    if (is_async_busy()) {
        ImGui::TextColored(WARNING, "Loading...");
    } else if (data_status_ == AsyncStatus::Error) {
        ImGui::TextColored(ERROR_RED, "Error: %s", error_.c_str());
    } else if (data_status_ == AsyncStatus::Success) {
        ImGui::TextColored(SUCCESS, "Ready");
        ImGui::SameLine();
        ImGui::TextColored(TEXT_DIM, "| %s", response_info_.c_str());
    } else {
        ImGui::TextColored(TEXT_DIM, "Ready");
    }

    // Right: region + category info
    if (active_category_idx_ >= 0 && active_region_idx_ >= 0) {
        ImGui::SameLine(w - 320);
        ImGui::TextColored(TEXT_DIM, "%s | %s | Python: %s",
            regions_[active_region_idx_].name.c_str(),
            categories_catalog_[active_category_idx_].name.c_str(),
            python::is_python_available() ? "OK" : "unavailable");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::asia_markets
