// AKShare Data Explorer — Browse 26 AKShare data sources with 1000+ endpoints
// Port of AkShareDataTab.tsx — three-panel layout: sources | endpoints | data
// Uses Python bridge (akshare_*.py scripts) for data fetching

#include "akshare_screen.h"
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
#include <thread>

namespace fincept::akshare {

using namespace theme::colors;
using namespace theme::colors;

// ============================================================================
// Static data source catalog (mirrors AKSHARE_DATA_SOURCES from akshareApi.ts)
// ============================================================================

static std::vector<DataSource> build_source_catalog() {
    return {
        {"data",              "Market Data",         "akshare_data.py",
         "Stock quotes, indices, forex, futures, ETFs - 14 endpoints",
         {0.02f, 0.71f, 0.83f, 1.0f},  // cyan #06B6D4
         {"Stocks", "Funds", "Macro", "Bonds", "Forex", "Futures"}},

        {"index",             "Index Data",          "akshare_index.py",
         "Index constituents, weights, historical - 74 endpoints",
         {0.39f, 0.40f, 0.95f, 1.0f},  // indigo #6366F1
         {"Chinese Index", "Shenwan Index", "CNI Index", "Global Index"}},

        {"futures",           "Futures",             "akshare_futures.py",
         "Futures contracts, positions, spot prices - 59 endpoints",
         {0.98f, 0.45f, 0.09f, 1.0f},  // orange #F97316
         {"Contract Info", "Realtime", "Historical", "Global", "Warehouse", "Positions"}},

        {"bonds",             "Bonds",               "akshare_bonds.py",
         "Bond markets, yield curves, government & corporate - 28 endpoints",
         {0.23f, 0.51f, 0.96f, 1.0f},  // blue #3B82F6
         {"Chinese Bond Market", "Yield Curves", "Government Bonds", "Corporate Bonds", "Convertible Bonds"}},

        {"currency",          "Currency/Forex",      "akshare_currency.py",
         "Exchange rates, currency pairs - 8 endpoints",
         {0.13f, 0.77f, 0.37f, 1.0f},  // green #22C55E
         {"Exchange Rates", "Currency Data"}},

        {"energy",            "Energy",              "akshare_energy.py",
         "Carbon trading, oil prices - 8 endpoints",
         {0.98f, 0.75f, 0.14f, 1.0f},  // yellow #FBBF24
         {"Carbon Trading", "Oil"}},

        {"crypto",            "Crypto",              "akshare_crypto.py",
         "Bitcoin CME futures, crypto spot - 3 endpoints",
         {0.97f, 0.57f, 0.10f, 1.0f},  // bitcoin orange #F7931A
         {"Bitcoin", "Spot"}},

        {"news",              "News",                "akshare_news.py",
         "CCTV news, economic news - 5 endpoints",
         {0.39f, 0.45f, 0.55f, 1.0f},  // slate #64748B
         {"News Feeds", "Trade Notifications"}},

        {"reits",             "REITs",               "akshare_reits.py",
         "Real Estate Investment Trusts - 3 endpoints",
         {0.05f, 0.65f, 0.91f, 1.0f},  // sky #0EA5E9
         {"REITs"}},

        {"stocks_realtime",   "Stocks: Realtime",    "akshare_stocks_realtime.py",
         "Realtime A-shares, B-shares, HK, US - 49 endpoints",
         {0.06f, 0.73f, 0.51f, 1.0f},  // emerald #10B981
         {"A-Shares Realtime", "B-Shares", "HK Stocks", "US Stocks"}},

        {"stocks_historical", "Stocks: Historical",  "akshare_stocks_historical.py",
         "Historical daily/minute/tick data - 52 endpoints",
         {0.23f, 0.51f, 0.96f, 1.0f},  // blue #3B82F6
         {"A-Shares Historical", "HK Historical", "US Historical", "Intraday"}},

        {"stocks_financial",  "Stocks: Financial",   "akshare_stocks_financial.py",
         "Financial statements, earnings - 45 endpoints",
         {0.55f, 0.36f, 0.96f, 1.0f},  // violet #8B5CF6
         {"Balance Sheet", "Income Statement", "Cash Flow", "Earnings"}},

        {"stocks_holders",    "Stocks: Shareholders","akshare_stocks_holders.py",
         "Shareholder data, holdings - 49 endpoints",
         {0.96f, 0.62f, 0.04f, 1.0f},  // amber #F59E0B
         {"Major Shareholders", "Fund Holdings", "Institutional"}},

        {"stocks_funds",      "Stocks: Fund Flows",  "akshare_stocks_funds.py",
         "Fund flows, block trades, IPO - 56 endpoints",
         {0.94f, 0.27f, 0.27f, 1.0f},  // red #EF4444
         {"Fund Flow", "Block Trade", "LHB", "IPO"}},

        {"stocks_board",      "Stocks: Boards",      "akshare_stocks_board.py",
         "Industry/concept boards - 49 endpoints",
         {0.39f, 0.40f, 0.95f, 1.0f},  // indigo #6366F1
         {"Concept Board", "Industry Board", "Rankings"}},

        {"stocks_margin",     "Stocks: Margin & HSGT","akshare_stocks_margin.py",
         "Margin trading, HSGT - 46 endpoints",
         {0.93f, 0.29f, 0.60f, 1.0f},  // pink #EC4899
         {"Margin Trading", "HSGT", "PE/PB"}},

        {"stocks_hot",        "Stocks: Hot & News",  "akshare_stocks_hot.py",
         "Hot stocks, news, sentiment - 55 endpoints",
         {0.98f, 0.45f, 0.09f, 1.0f},  // orange #F97316
         {"Hot Stocks", "News", "ESG", "Sentiment"}},

        {"alternative",       "Alternative Data",    "akshare_alternative.py",
         "Air quality, carbon, real estate - 14 endpoints",
         {0.93f, 0.29f, 0.60f, 1.0f},  // pink #EC4899
         {"Air Quality", "Carbon", "Energy", "Movies"}},

        {"analysis",          "Market Analysis",     "akshare_analysis.py",
         "Technical analysis, market breadth",
         {0.08f, 0.72f, 0.65f, 1.0f},  // teal #14B8A6
         {"Technical", "Breadth", "Fund Flow"}},

        {"derivatives",       "Derivatives",         "akshare_derivatives.py",
         "Options and derivatives data - 46 endpoints",
         {0.55f, 0.36f, 0.96f, 1.0f},  // violet #8B5CF6
         {"CFFEX Options", "SSE Options", "Commodity Options", "Historical"}},

        {"economics_china",   "China Economics",     "akshare_economics_china.py",
         "Chinese economic indicators - 85 endpoints",
         {0.86f, 0.15f, 0.15f, 1.0f},  // red #DC2626
         {"Core Indicators", "Monetary", "Trade", "Industry", "Real Estate"}},

        {"economics_global",  "Global Economics",    "akshare_economics_global.py",
         "International economic indicators - 35 endpoints",
         {0.02f, 0.59f, 0.40f, 1.0f},  // emerald #059669
         {"USA", "Eurozone", "UK", "Japan", "Canada", "Australia"}},

        {"funds_expanded",    "Funds Expanded",      "akshare_funds_expanded.py",
         "Comprehensive fund data - 70 endpoints",
         {0.49f, 0.23f, 0.93f, 1.0f},  // purple #7C3AED
         {"Fund Rankings", "Fund Info", "Holdings", "Managers", "Flows"}},

        {"company_info",      "Company Info",        "akshare_company_info.py",
         "Company profiles and details",
         {0.03f, 0.57f, 0.70f, 1.0f},  // cyan #0891B2
         {"CN Stocks", "HK Stocks", "US Stocks"}},

        {"macro",             "Macro Global",        "akshare_macro.py",
         "Global macro economic data - 96 endpoints",
         {0.75f, 0.15f, 0.83f, 1.0f},  // fuchsia #C026D3
         {"Australia", "Brazil", "India", "Russia", "Canada", "Euro", "Germany", "Japan", "Switzerland", "UK", "USA"}},

        {"misc",              "Miscellaneous",       "akshare_misc.py",
         "Misc data sources - 129 endpoints",
         {0.92f, 0.35f, 0.05f, 1.0f},  // orange #EA580C
         {"AMAC", "Air Quality", "Car Sales", "Movies", "FRED", "Migration"}},
    };
}

// ============================================================================
// Period labels
// ============================================================================

static const char* PERIOD_LABELS[] = {"daily", "weekly", "monthly"};
static constexpr int PERIOD_COUNT = 3;

// ============================================================================
// Helpers
// ============================================================================

static std::string timestamp_to_str(int64_t ts) {
    time_t t = static_cast<time_t>(ts / 1000);  // ms -> s
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

static int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// ============================================================================
// Initialization
// ============================================================================

void AkShareScreen::init() {
    sources_ = build_source_catalog();
    load_history_and_favorites();
}

// ============================================================================
// Async helpers
// ============================================================================

bool AkShareScreen::is_async_busy() const {
    return async_task_.valid() &&
           async_task_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready;
}

void AkShareScreen::check_async() {
    if (async_task_.valid() &&
        async_task_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        try { async_task_.get(); } catch (...) {}
    }
}

// ============================================================================
// Data fetching
// ============================================================================

void AkShareScreen::load_endpoints(const DataSource& src) {
    if (is_async_busy()) return;
    endpoints_status_ = AsyncStatus::Loading;
    endpoints_.clear();
    categories_.clear();
    expanded_categories_.clear();
    selected_endpoint_idx_ = -1;
    selected_endpoint_.clear();

    std::string script = src.script;
    async_task_ = std::async(std::launch::async, [this, script]() {
        auto result = python::execute(script, {"--list-endpoints"});
        if (!result.success || result.output.empty()) {
            endpoints_status_ = AsyncStatus::Error;
            error_ = result.error.empty() ? "Failed to load endpoints" : result.error;
            return;
        }
        try {
            auto j = nlohmann::json::parse(result.output);
            // Response may be {data: {available_endpoints: [...], categories: {...}}}
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

            // Extract categories
            if (payload.contains("categories") && payload["categories"].is_object()) {
                for (auto it = payload["categories"].begin(); it != payload["categories"].end(); ++it) {
                    std::vector<std::string> ep_list;
                    if (it.value().is_array()) {
                        for (auto& e : it.value()) ep_list.push_back(e.get<std::string>());
                    }
                    categories_[it.key()] = std::move(ep_list);
                }
                // Auto-expand first category
                if (!categories_.empty())
                    expanded_categories_.insert(categories_.begin()->first);
            }

            endpoints_status_ = AsyncStatus::Success;
        } catch (const std::exception& ex) {
            endpoints_status_ = AsyncStatus::Error;
            error_ = std::string("Parse error: ") + ex.what();
        }
    });
}

std::vector<std::string> AkShareScreen::build_params(const std::string& endpoint) const {
    std::vector<std::string> params;
    bool needs_symbol = endpoint.find("stock") != std::string::npos ||
                        endpoint.find("holder") != std::string::npos ||
                        endpoint.find("fund") != std::string::npos ||
                        endpoint.find("esg") != std::string::npos ||
                        endpoint.find("individual") != std::string::npos;
    bool needs_dates  = endpoint.find("hist") != std::string::npos ||
                        endpoint.find("historical") != std::string::npos;
    bool needs_period = endpoint.find("hist") != std::string::npos &&
                        endpoint.find("min") == std::string::npos;
    bool needs_market = endpoint.find("individual_fund_flow") != std::string::npos;

    if (needs_symbol && std::strlen(symbol_) > 0)
        params.emplace_back(symbol_);
    if (needs_period && period_idx_ >= 0 && period_idx_ < PERIOD_COUNT)
        params.emplace_back(PERIOD_LABELS[period_idx_]);
    if (needs_dates) {
        if (std::strlen(start_date_) > 0) params.emplace_back(start_date_);
        if (std::strlen(end_date_) > 0)   params.emplace_back(end_date_);
    }
    if (std::strlen(adjust_) > 0)
        params.emplace_back(adjust_);
    if (needs_market && std::strlen(market_) > 0)
        params.emplace_back(market_);

    return params;
}

void AkShareScreen::execute_query(const std::string& script, const std::string& endpoint) {
    if (is_async_busy()) return;
    data_status_ = AsyncStatus::Loading;
    data_ = nlohmann::json::array();
    columns_.clear();
    row_count_ = 0;
    error_.clear();
    response_info_.clear();
    current_page_ = 1;

    auto params = build_params(endpoint);

    // Build args: endpoint name + optional params
    std::vector<std::string> args;
    args.push_back(endpoint);
    for (auto& p : params) args.push_back(p);

    std::string script_copy = script;
    std::string ep_copy = endpoint;

    async_task_ = std::async(std::launch::async, [this, script_copy, ep_copy, args]() {
        auto result = python::execute(script_copy, args);
        if (!result.success || result.output.empty()) {
            data_status_ = AsyncStatus::Error;
            error_ = result.error.empty() ? "Query failed — no response" : result.error;
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

void AkShareScreen::parse_response(const nlohmann::json& j) {
    // Navigate to actual data array (may be nested)
    nlohmann::json payload = j;
    if (j.contains("data")) payload = j["data"];
    if (payload.is_object() && payload.contains("data")) payload = payload["data"];

    // Find array data
    nlohmann::json arr = nlohmann::json::array();
    if (payload.is_array()) {
        arr = payload;
    } else if (payload.is_object()) {
        // Search for array values
        for (auto it = payload.begin(); it != payload.end(); ++it) {
            if (it.value().is_array() && !it.value().empty()) { arr = it.value(); break; }
        }
    }

    if (arr.empty() || !arr.is_array()) {
        data_status_ = AsyncStatus::Error;
        error_ = "No data records found in response";
        return;
    }

    // Extract columns from first row
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
// History & Favorites persistence (via SQLite key-value)
// ============================================================================

void AkShareScreen::load_history_and_favorites() {
    // Load from SQLite settings
    auto hist_opt = db::ops::get_setting("akshare_history");
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

    auto fav_opt = db::ops::get_setting("akshare_favorites");
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

void AkShareScreen::save_history() {
    nlohmann::json j = nlohmann::json::array();
    for (auto& h : history_) {
        j.push_back({{"id", h.id}, {"script", h.script}, {"endpoint", h.endpoint},
                      {"timestamp", h.timestamp}, {"success", h.success}, {"count", h.count}});
    }
    db::ops::save_setting("akshare_history", j.dump());
}

void AkShareScreen::save_favorites() {
    nlohmann::json j = nlohmann::json::array();
    for (auto& f : favorites_) {
        j.push_back({{"script", f.script}, {"endpoint", f.endpoint}, {"name", f.name}});
    }
    db::ops::save_setting("akshare_favorites", j.dump());
}

void AkShareScreen::add_history(const std::string& script, const std::string& endpoint,
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

bool AkShareScreen::is_favorite(const std::string& script, const std::string& endpoint) const {
    return std::any_of(favorites_.begin(), favorites_.end(),
        [&](const FavoriteEndpoint& f) { return f.script == script && f.endpoint == endpoint; });
}

void AkShareScreen::toggle_favorite(const std::string& script, const std::string& endpoint) {
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

void AkShareScreen::render() {
    if (!initialized_) { init(); initialized_ = true; }
    check_async();

    ui::ScreenFrame frame("##akshare_screen", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    float w = frame.width();
    float h = frame.height();

    render_header(w);

    // Three-panel layout: Sources | Endpoints | Data
    float sources_w   = sources_collapsed_   ? 36.0f : std::min(220.0f, w * 0.18f);
    float endpoints_w = endpoints_collapsed_ ? 36.0f : std::min(260.0f, w * 0.22f);
    float data_w      = w - sources_w - endpoints_w - 8.0f;
    float panel_h     = h - 72.0f; // header + status bar

    ImGui::BeginGroup();
    {
        // Sources panel
        ImGui::BeginChild("##ak_sources", ImVec2(sources_w, panel_h), false);
        render_sources_panel(sources_w, panel_h);
        ImGui::EndChild();

        ImGui::SameLine(0, 2);

        // Endpoints panel
        ImGui::BeginChild("##ak_endpoints", ImVec2(endpoints_w, panel_h), false);
        render_endpoints_panel(endpoints_w, panel_h);
        ImGui::EndChild();

        ImGui::SameLine(0, 2);

        // Data panel
        ImGui::BeginChild("##ak_data", ImVec2(data_w, panel_h), false);
        render_data_panel(data_w, panel_h);
        ImGui::EndChild();
    }
    ImGui::EndGroup();

    render_status_bar(w);
    frame.end();
}

// ============================================================================
// Header
// ============================================================================

void AkShareScreen::render_header(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ak_header", ImVec2(w, 36), false);

    ImGui::SetCursorPos(ImVec2(8, 6));
    ImGui::TextColored(ACCENT, "AKShare");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_SECONDARY, "Data Explorer");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "| 26 Sources | 1000+ Endpoints");

    // Right side: history + favorites buttons
    float btn_x = w - 180.0f;
    ImGui::SameLine(btn_x);
    if (ImGui::SmallButton(show_history_ ? "History [x]" : "History")) {
        show_history_ = !show_history_;
        show_favorites_ = false;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(show_favorites_ ? "Favorites [x]" : "Favorites")) {
        show_favorites_ = !show_favorites_;
        show_history_ = false;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Popups
    if (show_history_)   render_history_popup();
    if (show_favorites_) render_favorites_popup();
}

// ============================================================================
// Sources panel (left)
// ============================================================================

void AkShareScreen::render_sources_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    // Collapse toggle
    if (ImGui::SmallButton(sources_collapsed_ ? ">>" : "<<")) {
        sources_collapsed_ = !sources_collapsed_;
    }
    if (sources_collapsed_) {
        ImGui::PopStyleColor();
        return;
    }

    ImGui::SameLine();
    ImGui::TextColored(TEXT_SECONDARY, "Sources (%d)", (int)sources_.size());

    // Search filter
    ImGui::SetNextItemWidth(w - 16);
    ImGui::InputTextWithHint("##src_search", "Filter sources...", source_search_, sizeof(source_search_));
    std::string search_lower(source_search_);
    for (auto& c : search_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    ImGui::Separator();

    // Source list
    ImGui::BeginChild("##src_list", ImVec2(0, 0), false);
    for (int i = 0; i < (int)sources_.size(); i++) {
        auto& src = sources_[i];

        // Filter
        if (!search_lower.empty()) {
            std::string name_lower = src.name;
            for (auto& c : name_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string desc_lower = src.description;
            for (auto& c : desc_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (name_lower.find(search_lower) == std::string::npos &&
                desc_lower.find(search_lower) == std::string::npos)
                continue;
        }

        bool selected = (i == selected_source_idx_);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(src.color.x, src.color.y, src.color.z, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(src.color.x, src.color.y, src.color.z, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(src.color.x, src.color.y, src.color.z, 0.45f));

        if (ImGui::Selectable(("##src_" + src.id).c_str(), selected, 0, ImVec2(0, 40))) {
            selected_source_idx_ = i;
            load_endpoints(src);
        }

        // Overlay text on the selectable
        ImVec2 p = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(p.x, p.y), ImVec2(p.x + 3, p.y + 40),
            ImGui::ColorConvertFloat4ToU32(src.color));

        ImGui::SetCursorScreenPos(ImVec2(p.x + 10, p.y + 4));
        ImGui::TextColored(TEXT_PRIMARY, "%s", src.name.c_str());
        ImGui::SetCursorScreenPos(ImVec2(p.x + 10, p.y + 22));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        float desc_w = w - 20.0f;
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + desc_w);
        ImGui::TextWrapped("%s", src.description.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        // Restore cursor after overlay
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 44));
        ImGui::PopStyleColor(3);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Endpoints panel (center-left)
// ============================================================================

void AkShareScreen::render_endpoints_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(BG_PANEL.x + 0.01f, BG_PANEL.y + 0.01f, BG_PANEL.z + 0.01f, 1.0f));

    // Collapse toggle
    if (ImGui::SmallButton(endpoints_collapsed_ ? ">>" : "<<##ep")) {
        endpoints_collapsed_ = !endpoints_collapsed_;
    }
    if (endpoints_collapsed_) {
        ImGui::PopStyleColor();
        return;
    }

    ImGui::SameLine();
    if (selected_source_idx_ >= 0) {
        ImGui::TextColored(TEXT_SECONDARY, "%s (%d)",
            sources_[selected_source_idx_].name.c_str(), (int)endpoints_.size());
    } else {
        ImGui::TextColored(TEXT_DIM, "Select a source");
    }

    // Search
    ImGui::SetNextItemWidth(w - 16);
    ImGui::InputTextWithHint("##ep_search", "Filter endpoints...", endpoint_search_, sizeof(endpoint_search_));
    std::string ep_search_lower(endpoint_search_);
    for (auto& c : ep_search_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // Parameters toggle
    if (ImGui::SmallButton(show_params_ ? "Params [-]" : "Params [+]")) {
        show_params_ = !show_params_;
    }
    if (show_params_) render_params_bar(w);

    ImGui::Separator();

    // Loading state
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
    if (selected_source_idx_ < 0) {
        ImGui::TextColored(TEXT_DIM, "Select a data source from the left panel");
        ImGui::PopStyleColor();
        return;
    }

    // Endpoint list — categorized or flat
    ImGui::BeginChild("##ep_list", ImVec2(0, 0), false);

    std::string current_script = (selected_source_idx_ >= 0) ? sources_[selected_source_idx_].script : "";

    if (!categories_.empty()) {
        // Categorized view
        for (auto& [cat, eps] : categories_) {
            bool expanded = expanded_categories_.count(cat) > 0;
            if (ImGui::TreeNodeEx(cat.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
                if (!expanded) expanded_categories_.insert(cat);
                for (auto& ep : eps) {
                    // Filter
                    if (!ep_search_lower.empty()) {
                        std::string ep_lower = ep;
                        for (auto& c : ep_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        if (ep_lower.find(ep_search_lower) == std::string::npos) continue;
                    }

                    bool is_selected = (ep == selected_endpoint_);
                    bool is_fav = is_favorite(current_script, ep);

                    ImGui::PushID(ep.c_str());
                    if (is_fav) {
                        ImGui::TextColored(WARNING, "*");
                        ImGui::SameLine();
                    }
                    if (ImGui::Selectable(ep.c_str(), is_selected)) {
                        selected_endpoint_ = ep;
                        execute_query(current_script, ep);
                    }
                    // Right-click to toggle favorite
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        toggle_favorite(current_script, ep);
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            } else {
                expanded_categories_.erase(cat);
            }
        }
    } else {
        // Flat list
        for (int i = 0; i < (int)endpoints_.size(); i++) {
            auto& ep = endpoints_[i];
            if (!ep_search_lower.empty()) {
                std::string ep_lower = ep;
                for (auto& c : ep_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (ep_lower.find(ep_search_lower) == std::string::npos) continue;
            }

            bool is_selected = (ep == selected_endpoint_);
            bool is_fav = is_favorite(current_script, ep);

            ImGui::PushID(i);
            if (is_fav) {
                ImGui::TextColored(WARNING, "*");
                ImGui::SameLine();
            }
            if (ImGui::Selectable(ep.c_str(), is_selected)) {
                selected_endpoint_ = ep;
                execute_query(current_script, ep);
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                toggle_favorite(current_script, ep);
            }
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Query Parameters bar
// ============================================================================

void AkShareScreen::render_params_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##ak_params", ImVec2(w - 8, 80), true);

    float field_w = (w - 40) / 3.0f;

    ImGui::Text("Symbol:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_w);
    ImGui::InputText("##sym", symbol_, sizeof(symbol_));

    ImGui::SameLine();
    ImGui::Text("Period:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::Combo("##period", &period_idx_, PERIOD_LABELS, PERIOD_COUNT);

    ImGui::SameLine();
    ImGui::Text("Market:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    ImGui::InputText("##mkt", market_, sizeof(market_));

    ImGui::Text("Start:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_w);
    ImGui::InputTextWithHint("##sdate", "YYYY-MM-DD", start_date_, sizeof(start_date_));

    ImGui::SameLine();
    ImGui::Text("End:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_w);
    ImGui::InputTextWithHint("##edate", "YYYY-MM-DD", end_date_, sizeof(end_date_));

    ImGui::SameLine();
    ImGui::Text("Adjust:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    ImGui::InputText("##adj", adjust_, sizeof(adjust_));

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Data panel (right)
// ============================================================================

void AkShareScreen::render_data_panel(float w, float h) {
    // Toolbar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##data_toolbar", ImVec2(w, 30), false);
    ImGui::SetCursorPos(ImVec2(4, 4));

    // View mode toggle
    bool is_table = (view_mode_ == ViewMode::Table);
    if (ImGui::SmallButton(is_table ? "[Table]" : "Table")) view_mode_ = ViewMode::Table;
    ImGui::SameLine();
    if (ImGui::SmallButton(!is_table ? "[JSON]" : "JSON")) view_mode_ = ViewMode::JSON;

    // Info
    if (!response_info_.empty()) {
        ImGui::SameLine(0, 20);
        ImGui::TextColored(TEXT_DIM, "%s", response_info_.c_str());
    }

    // Refresh button
    if (selected_source_idx_ >= 0 && !selected_endpoint_.empty()) {
        ImGui::SameLine(w - 80);
        if (ImGui::SmallButton("Refresh") && !is_async_busy()) {
            execute_query(sources_[selected_source_idx_].script, selected_endpoint_);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Content area
    float content_h = h - 30 - 28; // toolbar + pagination
    if (data_status_ == AsyncStatus::Loading) {
        ImGui::SetCursorPos(ImVec2(w / 2 - 60, h / 2));
        ImGui::TextColored(WARNING, "Fetching data...");
    } else if (data_status_ == AsyncStatus::Error) {
        ImGui::SetCursorPos(ImVec2(12, 40));
        ImGui::TextColored(ERROR_RED, "Error: %s", error_.c_str());
    } else if (data_status_ == AsyncStatus::Idle || row_count_ == 0) {
        ImGui::SetCursorPos(ImVec2(w / 2 - 100, h / 2));
        ImGui::TextColored(TEXT_DIM, "Select a source and endpoint to query data");
    } else {
        if (view_mode_ == ViewMode::Table)
            render_table_view(w, content_h);
        else
            render_json_view(w, content_h);

        render_pagination(w);
    }
}

// ============================================================================
// Table view
// ============================================================================

void AkShareScreen::render_table_view(float w, float h) {
    if (columns_.empty() || data_.empty()) return;

    int n_cols = static_cast<int>(columns_.size());
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                            ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable;

    if (!ImGui::BeginTable("##ak_table", n_cols, flags, ImVec2(w - 4, h))) return;

    // Setup columns
    for (int c = 0; c < n_cols; c++) {
        ImGui::TableSetupColumn(columns_[c].c_str(), ImGuiTableColumnFlags_DefaultSort);
    }
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

    // Pagination
    int start = (current_page_ - 1) * page_size_;
    int end   = std::min(start + page_size_, total);

    // Render rows
    for (int ri = start; ri < end; ri++) {
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

    ImGui::EndTable();
}

// ============================================================================
// JSON view
// ============================================================================

void AkShareScreen::render_json_view(float w, float h) {
    ImGui::BeginChild("##json_view", ImVec2(w - 4, h), true);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);

    // Show paginated JSON
    int start = (current_page_ - 1) * page_size_;
    int end   = std::min(start + page_size_, (int)data_.size());

    for (int i = start; i < end; i++) {
        std::string line = data_[i].dump(2);
        ImGui::TextWrapped("[%d] %s", i, line.c_str());
        ImGui::Separator();
    }

    ImGui::PopStyleColor();
    ImGui::EndChild();
}

// ============================================================================
// Pagination
// ============================================================================

void AkShareScreen::render_pagination(float w) {
    int total_pages = (row_count_ + page_size_ - 1) / page_size_;
    if (total_pages <= 1) return;

    ImGui::BeginChild("##pagination", ImVec2(w, 26), false);
    ImGui::SetCursorPos(ImVec2(w / 2 - 120, 3));

    if (ImGui::SmallButton("<< First") && current_page_ > 1) current_page_ = 1;
    ImGui::SameLine();
    if (ImGui::SmallButton("< Prev") && current_page_ > 1) current_page_--;
    ImGui::SameLine();
    ImGui::TextColored(TEXT_PRIMARY, " %d / %d ", current_page_, total_pages);
    ImGui::SameLine();
    if (ImGui::SmallButton("Next >") && current_page_ < total_pages) current_page_++;
    ImGui::SameLine();
    if (ImGui::SmallButton("Last >>") && current_page_ < total_pages) current_page_ = total_pages;
    ImGui::SameLine(0, 20);
    ImGui::TextColored(TEXT_DIM, "(%d rows)", row_count_);

    ImGui::SameLine(0, 20);
    ImGui::SetNextItemWidth(60);
    int sizes[] = {25, 50, 100, 200};
    const char* size_labels[] = {"25", "50", "100", "200"};
    int cur_size_idx = 1;
    for (int i = 0; i < 4; i++) { if (sizes[i] == page_size_) cur_size_idx = i; }
    if (ImGui::Combo("##pgsize", &cur_size_idx, size_labels, 4)) {
        page_size_ = sizes[cur_size_idx];
        current_page_ = 1;
    }

    ImGui::EndChild();
}

// ============================================================================
// History popup
// ============================================================================

void AkShareScreen::render_history_popup() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 400, 50));
    ImGui::BeginChild("##ak_history", ImVec2(380, 300), true);

    ImGui::TextColored(ACCENT, "Query History");
    ImGui::Separator();

    if (history_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No queries yet");
    } else {
        for (int i = 0; i < (int)history_.size() && i < 20; i++) {
            auto& h = history_[i];
            ImVec4 color = h.success ? SUCCESS : ERROR_RED;
            ImGui::TextColored(color, "%s", h.success ? "[OK]" : "[ERR]");
            ImGui::SameLine();
            ImGui::TextColored(TEXT_PRIMARY, "%s", h.endpoint.c_str());
            ImGui::SameLine();
            ImGui::TextColored(TEXT_DIM, "(%d rows)", h.count);

            // Click to re-run
            if (ImGui::IsItemClicked() && !is_async_busy()) {
                // Find the source with matching script
                for (int s = 0; s < (int)sources_.size(); s++) {
                    if (sources_[s].script == h.script) {
                        selected_source_idx_ = s;
                        break;
                    }
                }
                selected_endpoint_ = h.endpoint;
                execute_query(h.script, h.endpoint);
                show_history_ = false;
            }
        }
    }

    if (ImGui::SmallButton("Clear History")) {
        history_.clear();
        save_history();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Favorites popup
// ============================================================================

void AkShareScreen::render_favorites_popup() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 400, 50));
    ImGui::BeginChild("##ak_favorites", ImVec2(380, 250), true);

    ImGui::TextColored(ACCENT, "Favorite Endpoints");
    ImGui::Separator();

    if (favorites_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No favorites — right-click an endpoint to add");
    } else {
        for (int i = 0; i < (int)favorites_.size(); i++) {
            auto& f = favorites_[i];
            ImGui::PushID(i);
            ImGui::TextColored(WARNING, "*");
            ImGui::SameLine();
            if (ImGui::Selectable(f.endpoint.c_str())) {
                // Find source, load & execute
                for (int s = 0; s < (int)sources_.size(); s++) {
                    if (sources_[s].script == f.script) {
                        selected_source_idx_ = s;
                        load_endpoints(sources_[s]);
                        break;
                    }
                }
                selected_endpoint_ = f.endpoint;
                execute_query(f.script, f.endpoint);
                show_favorites_ = false;
            }
            ImGui::SameLine(320);
            if (ImGui::SmallButton("X")) {
                toggle_favorite(f.script, f.endpoint);
            }
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Status bar
// ============================================================================

void AkShareScreen::render_status_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##ak_status", ImVec2(w, 24), false);
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
        ImGui::TextColored(TEXT_DIM, "Ready — select a source and endpoint to begin");
    }

    // Right side: source count + Python status
    ImGui::SameLine(w - 250);
    ImGui::TextColored(TEXT_DIM, "%d sources | Python: %s",
        (int)sources_.size(),
        python::is_python_available() ? "OK" : "unavailable");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::akshare
