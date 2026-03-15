// DBnomics Screen — Browse economic data from db.nomics.world
// Port of DBnomicsTab.tsx + useDBnomicsData.ts — provider/dataset/series
// navigation, global search, single/comparison views, ImPlot charts, data table

#include "dbnomics_screen.h"
#include "http/http_client.h"
#include "storage/cache_service.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <implot.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <set>
#include <sstream>

namespace fincept::dbnomics {

using json = nlohmann::json;

// ============================================================================
// Chart color palette
// ============================================================================

static const ImVec4 CHART_COLORS[] = {
    {1.0f, 0.53f, 0.0f, 1.0f},   // orange
    {0.0f, 0.90f, 1.0f, 1.0f},   // cyan
    {0.0f, 0.84f, 0.44f, 1.0f},  // green
    {1.0f, 0.84f, 0.0f, 1.0f},   // yellow
    {0.62f, 0.31f, 0.87f, 1.0f}, // purple
    {0.0f, 0.53f, 1.0f, 1.0f},   // blue
};

static ImVec4 get_chart_color(int idx) {
    return CHART_COLORS[idx % CHART_COLOR_COUNT];
}

// ============================================================================
// Helpers
// ============================================================================

static std::string url_encode(const std::string& s) {
    std::ostringstream out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out << c;
        else
            out << '%' << "0123456789ABCDEF"[c >> 4] << "0123456789ABCDEF"[c & 0xF];
    }
    return out.str();
}

int DBnomicsScreen::next_color_index() const {
    int count = static_cast<int>(single_view_series_.size());
    for (const auto& slot : slots_)
        count += static_cast<int>(slot.size());
    return count % CHART_COLOR_COUNT;
}

bool DBnomicsScreen::is_async_done() const {
    return async_task_.valid() &&
           async_task_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}

void DBnomicsScreen::check_async() {
    if (async_task_.valid() &&
        async_task_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        async_task_.get();  // propagate exceptions / complete
    }
}

// ============================================================================
// Data fetching
// ============================================================================

void DBnomicsScreen::load_providers() {
    if (loading_) return;
    loading_ = true;
    status_ = "Loading providers...";
    error_.clear();

    async_task_ = std::async(std::launch::async, [this]() {
        try {
            auto& cache = CacheService::instance();
            std::string cache_key = CacheService::make_key("reference", "dbnomics", "providers");

            std::string body;
            auto cached = cache.get(cache_key);
            if (cached && !cached->empty()) {
                body = *cached;
                LOG_INFO("DBnomics", "Cache HIT for providers");
            } else {
                std::string url = std::string(DBNOMICS_API_BASE) + "/providers";
                auto resp = http::HttpClient::instance().get(url);
                if (!resp.success) {
                    error_ = "Failed to load providers: " + resp.body;
                    loading_ = false;
                    return;
                }
                body = resp.body;
                if (!body.empty()) {
                    cache.set(cache_key, body, "reference", CacheTTL::ONE_DAY);
                }
            }

            auto j = json::parse(body, nullptr, false);
            if (j.is_discarded()) {
                error_ = "Invalid JSON from providers API";
                loading_ = false;
                return;
            }

            std::vector<Provider> result;
            auto docs = j.value("providers", json::object()).value("docs", json::array());
            for (const auto& p : docs) {
                Provider prov;
                prov.code = p.value("code", "");
                prov.name = p.value("name", prov.code);
                if (!prov.code.empty())
                    result.push_back(std::move(prov));
            }

            providers_ = std::move(result);
            status_ = "Loaded " + std::to_string(providers_.size()) + " providers";
            loading_ = false;
        } catch (const std::exception& e) {
            error_ = std::string("Error: ") + e.what();
            loading_ = false;
        }
    });
}

void DBnomicsScreen::load_datasets(const std::string& provider_code, int offset, bool append) {
    if (loading_ || provider_code.empty()) return;
    loading_ = true;
    status_ = append ? "Loading more datasets..." : "Loading datasets...";

    async_task_ = std::async(std::launch::async, [this, provider_code, offset, append]() {
        try {
            auto& cache = CacheService::instance();
            std::string cache_key = CacheService::make_key("reference", "dbnomics-datasets",
                provider_code + "_o" + std::to_string(offset));

            std::string body;
            auto cached = cache.get(cache_key);
            if (cached && !cached->empty()) {
                body = *cached;
            } else {
                std::string url = std::string(DBNOMICS_API_BASE) + "/datasets/"
                    + url_encode(provider_code)
                    + "?limit=" + std::to_string(DATASETS_PAGE_SIZE)
                    + "&offset=" + std::to_string(offset);
                auto resp = http::HttpClient::instance().get(url);
                if (!resp.success) {
                    error_ = "Failed to load datasets";
                    loading_ = false;
                    return;
                }
                body = resp.body;
                if (!body.empty()) {
                    cache.set(cache_key, body, "reference", CacheTTL::ONE_HOUR);
                }
            }

            auto j = json::parse(body, nullptr, false);
            if (j.is_discarded()) { error_ = "Invalid JSON"; loading_ = false; return; }

            int total = j.value("datasets", json::object()).value("num_found", 0);
            auto docs = j.value("datasets", json::object()).value("docs", json::array());

            std::vector<Dataset> list;
            for (const auto& d : docs) {
                Dataset ds;
                ds.code = d.value("code", "");
                ds.name = d.value("name", ds.code);
                if (!ds.code.empty())
                    list.push_back(std::move(ds));
            }

            if (append) {
                datasets_.insert(datasets_.end(), list.begin(), list.end());
            } else {
                datasets_ = std::move(list);
                series_list_.clear();
            }

            int new_offset = offset + static_cast<int>(append ? list.size() : datasets_.size());
            datasets_pagination_ = {new_offset, DATASETS_PAGE_SIZE, total, new_offset < total};
            status_ = "Loaded " + std::to_string(datasets_.size()) + " of " + std::to_string(total) + " datasets";
            loading_ = false;
        } catch (const std::exception& e) {
            error_ = std::string("Error: ") + e.what();
            loading_ = false;
        }
    });
}

void DBnomicsScreen::load_series(const std::string& provider, const std::string& dataset,
                                  int offset, bool append, const std::string& query) {
    if (loading_ || provider.empty() || dataset.empty()) return;
    loading_ = true;
    status_ = append ? "Loading more series..." : "Loading series...";

    async_task_ = std::async(std::launch::async, [this, provider, dataset, offset, append, query]() {
        try {
            auto& cache = CacheService::instance();
            std::string cache_key = CacheService::make_key("reference", "dbnomics-series",
                provider + "_" + dataset + "_o" + std::to_string(offset) + "_q" + query);

            std::string body;
            auto cached = cache.get(cache_key);
            if (cached && !cached->empty()) {
                body = *cached;
            } else {
                std::string url = std::string(DBNOMICS_API_BASE) + "/series/"
                    + url_encode(provider) + "/" + url_encode(dataset)
                    + "?limit=" + std::to_string(SERIES_PAGE_SIZE)
                    + "&offset=" + std::to_string(offset)
                    + "&observations=false";
                if (!query.empty())
                    url += "&q=" + url_encode(query);

                auto resp = http::HttpClient::instance().get(url);
                if (!resp.success) { error_ = "Failed to load series"; loading_ = false; return; }
                body = resp.body;
                if (!body.empty()) {
                    cache.set(cache_key, body, "reference", CacheTTL::ONE_HOUR);
                }
            }

            auto j = json::parse(body, nullptr, false);
            if (j.is_discarded()) { error_ = "Invalid JSON"; loading_ = false; return; }

            int total = j.value("series", json::object()).value("num_found", 0);
            auto docs = j.value("series", json::object()).value("docs", json::array());

            std::vector<Series> list;
            for (const auto& s : docs) {
                Series sr;
                sr.code = s.value("series_code", "");
                sr.name = s.value("series_name", sr.code);
                sr.full_id = provider + "/" + dataset + "/" + sr.code;
                if (!sr.code.empty())
                    list.push_back(std::move(sr));
            }

            if (append) {
                series_list_.insert(series_list_.end(), list.begin(), list.end());
            } else {
                series_list_ = std::move(list);
            }

            int new_offset = offset + static_cast<int>(append ? list.size() : series_list_.size());
            series_pagination_ = {new_offset, SERIES_PAGE_SIZE, total, new_offset < total};
            status_ = "Loaded " + std::to_string(series_list_.size()) + " of " + std::to_string(total) + " series";
            loading_ = false;
        } catch (const std::exception& e) {
            error_ = std::string("Error: ") + e.what();
            loading_ = false;
        }
    });
}

void DBnomicsScreen::load_series_data(const std::string& full_id, const std::string& name) {
    if (loading_ || full_id.empty()) return;

    // Parse provider/dataset/series from full_id
    auto pos1 = full_id.find('/');
    auto pos2 = full_id.find('/', pos1 + 1);
    if (pos1 == std::string::npos || pos2 == std::string::npos) return;
    std::string prov = full_id.substr(0, pos1);
    std::string ds   = full_id.substr(pos1 + 1, pos2 - pos1 - 1);
    std::string sc   = full_id.substr(pos2 + 1);

    loading_ = true;
    status_ = "Loading data...";

    async_task_ = std::async(std::launch::async, [this, prov, ds, sc, full_id, name]() {
        try {
            auto& cache = CacheService::instance();
            std::string cache_key = CacheService::make_key("economic", "dbnomics-data",
                prov + "_" + ds + "_" + sc);

            std::string body;
            auto cached = cache.get(cache_key);
            if (cached && !cached->empty()) {
                body = *cached;
            } else {
                std::string url = std::string(DBNOMICS_API_BASE) + "/series/"
                    + url_encode(prov) + "/" + url_encode(ds) + "/" + url_encode(sc)
                    + "?observations=1&format=json";

                auto resp = http::HttpClient::instance().get(url);
                if (!resp.success) { error_ = "Failed to load data"; loading_ = false; return; }
                body = resp.body;
                if (!body.empty()) {
                    cache.set(cache_key, body, "economic", CacheTTL::ONE_HOUR);
                }
            }

            auto j = json::parse(body, nullptr, false);
            if (j.is_discarded()) { error_ = "Invalid JSON"; loading_ = false; return; }

            auto series_docs = j.value("series", json::object()).value("docs", json::array());
            if (series_docs.empty()) {
                status_ = "No data found";
                loading_ = false;
                return;
            }

            auto first = series_docs[0];
            auto periods = first.value("period", json::array());
            auto values  = first.value("value", json::array());

            DataPoint dp;
            dp.series_id = full_id;
            dp.series_name = name;
            dp.color_index = next_color_index();

            int n = std::min(static_cast<int>(periods.size()), static_cast<int>(values.size()));
            for (int i = 0; i < n; i++) {
                if (!values[i].is_null()) {
                    Observation obs;
                    obs.period = periods[i].get<std::string>();
                    obs.value  = values[i].get<double>();
                    dp.observations.push_back(std::move(obs));
                }
            }

            if (dp.observations.empty()) {
                status_ = "No observations found";
                loading_ = false;
                return;
            }

            current_data_ = std::move(dp);
            has_current_data_ = true;
            status_ = "Loaded " + std::to_string(current_data_.observations.size()) + " points";
            loading_ = false;
        } catch (const std::exception& e) {
            error_ = std::string("Error: ") + e.what();
            loading_ = false;
        }
    });
}

void DBnomicsScreen::execute_global_search(const std::string& query, int offset, bool append) {
    if (loading_ || query.empty()) return;
    loading_ = true;
    status_ = "Searching \"" + query + "\"...";

    async_task_ = std::async(std::launch::async, [this, query, offset, append]() {
        try {
            auto& cache = CacheService::instance();
            std::string cache_key = CacheService::make_key("reference", "dbnomics-search",
                query + "_o" + std::to_string(offset));

            std::string body;
            auto cached = cache.get(cache_key);
            if (cached && !cached->empty()) {
                body = *cached;
            } else {
                std::string url = std::string(DBNOMICS_API_BASE) + "/search?q="
                    + url_encode(query)
                    + "&limit=" + std::to_string(SEARCH_PAGE_SIZE)
                    + "&offset=" + std::to_string(offset);

                auto resp = http::HttpClient::instance().get(url);
                if (!resp.success) { error_ = "Search failed"; loading_ = false; return; }
                body = resp.body;
                if (!body.empty()) {
                    cache.set(cache_key, body, "reference", CacheTTL::FIFTEEN_MIN);
                }
            }

            auto j = json::parse(body, nullptr, false);
            if (j.is_discarded()) { error_ = "Invalid JSON"; loading_ = false; return; }

            int total = 0;
            json docs;
            if (j.contains("results")) {
                total = j["results"].value("num_found", 0);
                docs = j["results"].value("docs", json::array());
            } else {
                total = j.value("num_found", 0);
                docs = j.value("docs", json::array());
            }

            std::vector<SearchResult> list;
            for (const auto& r : docs) {
                SearchResult sr;
                sr.provider_code = r.value("provider_code", "");
                sr.provider_name = r.value("provider_name", sr.provider_code);
                sr.dataset_code  = r.value("dataset_code", "");
                sr.dataset_name  = r.value("dataset_name", sr.dataset_code);
                sr.nb_series     = r.value("nb_series", 0);
                list.push_back(std::move(sr));
            }

            if (append) {
                search_results_.insert(search_results_.end(), list.begin(), list.end());
            } else {
                search_results_ = std::move(list);
            }

            int new_offset = offset + static_cast<int>(append ? list.size() : search_results_.size());
            search_pagination_ = {new_offset, SEARCH_PAGE_SIZE, total, new_offset < total};
            status_ = "Found " + std::to_string(total) + " results";
            loading_ = false;
        } catch (const std::exception& e) {
            error_ = std::string("Search error: ") + e.what();
            loading_ = false;
        }
    });
}

// ============================================================================
// Actions
// ============================================================================

void DBnomicsScreen::select_provider(const std::string& code) {
    selected_provider_ = code;
    selected_dataset_.clear();
    selected_series_.clear();
    datasets_.clear();
    series_list_.clear();
    dataset_search_[0] = '\0';
    series_search_[0] = '\0';
    datasets_pagination_ = {};
    series_pagination_ = {};
    load_datasets(code);
}

void DBnomicsScreen::select_dataset(const std::string& code) {
    selected_dataset_ = code;
    selected_series_.clear();
    series_list_.clear();
    series_search_[0] = '\0';
    series_pagination_ = {};
    if (!selected_provider_.empty())
        load_series(selected_provider_, code);
}

void DBnomicsScreen::select_series(const Series& s) {
    selected_series_ = s.full_id;
    load_series_data(s.full_id, s.name);
}

void DBnomicsScreen::add_to_single_view() {
    if (has_current_data_)
        single_view_series_.push_back(current_data_);
}

void DBnomicsScreen::remove_from_single_view(int index) {
    if (index >= 0 && index < static_cast<int>(single_view_series_.size()))
        single_view_series_.erase(single_view_series_.begin() + index);
}

void DBnomicsScreen::add_slot() {
    slots_.push_back({});
    slot_chart_types_.push_back(ChartType::Line);
}

void DBnomicsScreen::remove_slot(int index) {
    if (slots_.size() <= 1) return;
    if (index >= 0 && index < static_cast<int>(slots_.size())) {
        slots_.erase(slots_.begin() + index);
        slot_chart_types_.erase(slot_chart_types_.begin() + index);
    }
}

void DBnomicsScreen::add_to_slot(int slot_index) {
    if (!has_current_data_) return;
    if (slot_index >= 0 && slot_index < static_cast<int>(slots_.size()))
        slots_[slot_index].push_back(current_data_);
}

void DBnomicsScreen::remove_from_slot(int slot_index, int series_index) {
    if (slot_index >= 0 && slot_index < static_cast<int>(slots_.size())) {
        auto& slot = slots_[slot_index];
        if (series_index >= 0 && series_index < static_cast<int>(slot.size()))
            slot.erase(slot.begin() + series_index);
    }
}

void DBnomicsScreen::clear_all() {
    slots_ = {{}};
    slot_chart_types_ = {ChartType::Line};
    single_view_series_.clear();
}

// ============================================================================
// Render — Main
// ============================================================================

void DBnomicsScreen::render() {
    check_async();

    if (!initialized_) {
        initialized_ = true;
        load_providers();
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Top bar
    render_top_bar();

    // Main content: left panel (280px) + center
    float panel_w = 280.0f;
    float remaining_h = ImGui::GetContentRegionAvail().y - 24.0f; // reserve status bar

    ImGui::BeginChild("##dbn_main", ImVec2(avail.x, remaining_h));
    {
        // Left panel
        ImGui::BeginChild("##dbn_left", ImVec2(panel_w, 0), ImGuiChildFlags_Borders);
        render_selection_panel();
        ImGui::EndChild();

        ImGui::SameLine();

        // Center panel
        ImGui::BeginChild("##dbn_center", ImVec2(0, 0));
        render_center_panel();
        ImGui::EndChild();
    }
    ImGui::EndChild();

    // Status bar
    render_status_bar();
}

// ============================================================================
// Render — Top Bar
// ============================================================================

void DBnomicsScreen::render_top_bar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::colors::BG_PANEL);
    ImGui::BeginChild("##dbn_topbar", ImVec2(0, 32), ImGuiChildFlags_Borders);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::SetCursorPosX(12.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::ACCENT);
    ImGui::Text("DBNOMICS ECONOMIC DATABASE");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // Status badge
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::SUCCESS);
    ImGui::Text(" %s", status_.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::INFO);
    ImGui::Text(" PROVIDERS: %d", static_cast<int>(providers_.size()));
    ImGui::PopStyleColor();

    // Right side buttons
    float right_x = ImGui::GetContentRegionMax().x;

    ImGui::SameLine(right_x - 200.0f);
    if (ImGui::SmallButton("FETCH") && !loading_)
        load_providers();

    ImGui::SameLine();
    bool can_refresh = has_current_data_ && !loading_;
    if (!can_refresh) ImGui::BeginDisabled();
    if (ImGui::SmallButton("REFRESH"))
        load_series_data(current_data_.series_id, current_data_.series_name);
    if (!can_refresh) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!can_refresh) ImGui::BeginDisabled();
    if (theme::AccentButton("EXPORT") && has_current_data_) {
        // CSV export to clipboard
        std::string csv = "Period,Value\n";
        for (const auto& obs : current_data_.observations)
            csv += obs.period + "," + std::to_string(obs.value) + "\n";
        ImGui::SetClipboardText(csv.c_str());
        status_ = "Exported to clipboard";
    }
    if (!can_refresh) ImGui::EndDisabled();

    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg
}

// ============================================================================
// Render — Selection Panel (Left)
// ============================================================================

void DBnomicsScreen::render_selection_panel() {
    render_global_search();
    render_provider_list();
    render_dataset_list();
    render_series_list();

    ImGui::Separator();
    render_action_buttons();
    render_comparison_slots();
}

// ============================================================================
// Render — Global Search
// ============================================================================

void DBnomicsScreen::render_global_search() {
    theme::SectionHeader("GLOBAL SEARCH");

    ImGui::SetNextItemWidth(-1);
    bool enter = ImGui::InputText("##dbn_gsearch", global_search_, sizeof(global_search_),
                                   ImGuiInputTextFlags_EnterReturnsTrue);
    if (enter && global_search_[0] != '\0') {
        search_results_.clear();
        search_pagination_ = {};
        execute_global_search(global_search_);
    }

    // Show search results
    if (!search_results_.empty()) {
        ImGui::BeginChild("##dbn_sresults", ImVec2(0, std::min(150.0f, search_results_.size() * 48.0f)),
                          ImGuiChildFlags_Borders);
        for (size_t i = 0; i < search_results_.size(); i++) {
            const auto& r = search_results_[i];
            ImGui::PushID(static_cast<int>(i));

            std::string label = r.provider_code + "/" + r.dataset_code;
            if (ImGui::Selectable(label.c_str(), false)) {
                // Navigate to this provider/dataset
                selected_provider_ = r.provider_code;
                selected_dataset_ = r.dataset_code;
                selected_series_.clear();
                series_list_.clear();
                load_datasets(r.provider_code);
                load_series(r.provider_code, r.dataset_code);
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
            ImGui::Text("%s (%d)", r.dataset_name.substr(0, 30).c_str(), r.nb_series);
            ImGui::PopStyleColor();

            ImGui::PopID();
        }

        if (search_pagination_.has_more && !loading_) {
            if (ImGui::SmallButton("Load More..."))
                execute_global_search(global_search_, search_pagination_.offset, true);
        }
        ImGui::EndChild();
    }
}

// ============================================================================
// Render — Provider List
// ============================================================================

void DBnomicsScreen::render_provider_list() {
    theme::SectionHeader("1. PROVIDER");
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::INFO);
    ImGui::SameLine(); ImGui::Text("%d", static_cast<int>(providers_.size()));
    ImGui::PopStyleColor();

    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##dbn_pfilter", provider_search_, sizeof(provider_search_));

    std::string pfilter(provider_search_);
    for (auto& c : pfilter) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    ImGui::BeginChild("##dbn_providers", ImVec2(0, 140), ImGuiChildFlags_Borders);
    for (const auto& p : providers_) {
        // Client-side filter
        if (!pfilter.empty()) {
            std::string lower_code = p.code, lower_name = p.name;
            for (auto& c : lower_code) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (auto& c : lower_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lower_code.find(pfilter) == std::string::npos &&
                lower_name.find(pfilter) == std::string::npos)
                continue;
        }

        bool selected = (p.code == selected_provider_);
        std::string label = p.code + " - " + p.name;
        if (ImGui::Selectable(label.c_str(), selected))
            select_provider(p.code);
    }
    ImGui::EndChild();
}

// ============================================================================
// Render — Dataset List
// ============================================================================

void DBnomicsScreen::render_dataset_list() {
    theme::SectionHeader("2. DATASET");
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::INFO);
    ImGui::SameLine(); ImGui::Text("%d/%d", static_cast<int>(datasets_.size()), datasets_pagination_.total);
    ImGui::PopStyleColor();

    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##dbn_dfilter", dataset_search_, sizeof(dataset_search_));

    std::string dfilter(dataset_search_);
    for (auto& c : dfilter) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    ImGui::BeginChild("##dbn_datasets", ImVec2(0, 140), ImGuiChildFlags_Borders);
    for (const auto& d : datasets_) {
        if (!dfilter.empty()) {
            std::string lower_code = d.code, lower_name = d.name;
            for (auto& c : lower_code) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (auto& c : lower_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lower_code.find(dfilter) == std::string::npos &&
                lower_name.find(dfilter) == std::string::npos)
                continue;
        }

        bool selected = (d.code == selected_dataset_);
        std::string label = d.code + " - " + d.name;
        if (ImGui::Selectable(label.c_str(), selected))
            select_dataset(d.code);
    }

    if (datasets_pagination_.has_more && !loading_) {
        if (ImGui::SmallButton("Load More Datasets..."))
            load_datasets(selected_provider_, datasets_pagination_.offset, true);
    }
    ImGui::EndChild();
}

// ============================================================================
// Render — Series List
// ============================================================================

void DBnomicsScreen::render_series_list() {
    theme::SectionHeader("3. SERIES");
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::INFO);
    ImGui::SameLine(); ImGui::Text("%d/%d", static_cast<int>(series_list_.size()), series_pagination_.total);
    ImGui::PopStyleColor();

    ImGui::SetNextItemWidth(-1);
    bool enter = ImGui::InputText("##dbn_sfilter", series_search_, sizeof(series_search_),
                                   ImGuiInputTextFlags_EnterReturnsTrue);
    if (enter && !selected_provider_.empty() && !selected_dataset_.empty()) {
        series_list_.clear();
        series_pagination_ = {};
        load_series(selected_provider_, selected_dataset_, 0, false, series_search_);
    }

    ImGui::BeginChild("##dbn_series", ImVec2(0, 140), ImGuiChildFlags_Borders);
    for (const auto& s : series_list_) {
        bool selected = (s.full_id == selected_series_);
        std::string label = s.code + " - " + s.name;
        if (ImGui::Selectable(label.c_str(), selected))
            select_series(s);
    }

    if (series_pagination_.has_more && !loading_) {
        if (ImGui::SmallButton("Load More Series..."))
            load_series(selected_provider_, selected_dataset_,
                        series_pagination_.offset, true, series_search_);
    }
    ImGui::EndChild();
}

// ============================================================================
// Render — Action Buttons
// ============================================================================

void DBnomicsScreen::render_action_buttons() {
    bool has_data = has_current_data_;

    if (!has_data) ImGui::BeginDisabled();
    if (theme::AccentButton("ADD TO SINGLE", ImVec2(-40, 0)))
        add_to_single_view();
    if (!has_data) ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::ERROR_RED);
    if (ImGui::SmallButton("CLR"))
        clear_all();
    ImGui::PopStyleColor();
}

// ============================================================================
// Render — Comparison Slots
// ============================================================================

void DBnomicsScreen::render_comparison_slots() {
    theme::SectionHeader("COMPARISON SLOTS");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::SUCCESS);
    if (ImGui::SmallButton("+##addslot"))
        add_slot();
    ImGui::PopStyleColor();

    for (int si = 0; si < static_cast<int>(slots_.size()); si++) {
        ImGui::PushID(si);
        auto& slot = slots_[si];

        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::WARNING);
        ImGui::Text("SLOT %d (%d)", si + 1, static_cast<int>(slot.size()));
        ImGui::PopStyleColor();

        ImGui::SameLine();
        bool has_data = has_current_data_;
        if (!has_data) ImGui::BeginDisabled();
        if (ImGui::SmallButton("+##addtoslot"))
            add_to_slot(si);
        if (!has_data) ImGui::EndDisabled();

        if (slots_.size() > 1) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::ERROR_RED);
            if (ImGui::SmallButton("-##rmslot"))
                remove_slot(si);
            ImGui::PopStyleColor();
        }

        // Series in slot
        for (int j = 0; j < static_cast<int>(slot.size()); j++) {
            ImGui::PushID(j);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
            ImGui::Text("  %s", slot[j].series_name.substr(0, 25).c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::ERROR_RED);
            if (ImGui::SmallButton("x##rmseries"))
                remove_from_slot(si, j);
            ImGui::PopStyleColor();
            ImGui::PopID();
        }

        if (slot.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
            ImGui::Text("  EMPTY");
            ImGui::PopStyleColor();
        }

        ImGui::PopID();
    }
}

// ============================================================================
// Render — Center Panel
// ============================================================================

void DBnomicsScreen::render_center_panel() {
    render_view_toggle_bar();

    ImGui::BeginChild("##dbn_content", ImVec2(0, 0));
    if (active_view_ == ActiveView::Single)
        render_single_view();
    else
        render_comparison_view();
    ImGui::EndChild();
}

// ============================================================================
// Render — View Toggle Bar
// ============================================================================

void DBnomicsScreen::render_view_toggle_bar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::colors::BG_PANEL);
    ImGui::BeginChild("##dbn_viewbar", ImVec2(0, 30), ImGuiChildFlags_Borders);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImGui::SetCursorPosX(8.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
    ImGui::Text("VIEW");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // Single button
    bool is_single = (active_view_ == ActiveView::Single);
    if (is_single) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT);
    if (ImGui::SmallButton("SINGLE"))
        active_view_ = ActiveView::Single;
    if (is_single) ImGui::PopStyleColor();
    ImGui::SameLine();

    // Compare button
    bool is_compare = (active_view_ == ActiveView::Comparison);
    if (is_compare) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT);
    if (ImGui::SmallButton("COMPARE"))
        active_view_ = ActiveView::Comparison;
    if (is_compare) ImGui::PopStyleColor();

    // Chart type buttons (single view only)
    if (is_single) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
        ImGui::Text("|  CHART TYPE");
        ImGui::PopStyleColor();

        static const ChartType types[] = {ChartType::Line, ChartType::Bar, ChartType::Area, ChartType::Scatter};
        for (auto ct : types) {
            ImGui::SameLine();
            bool active = (single_chart_type_ == ct);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT);
            if (ImGui::SmallButton(chart_type_label(ct)))
                single_chart_type_ = ct;
            if (active) ImGui::PopStyleColor();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg
}

// ============================================================================
// Render — Single View
// ============================================================================

void DBnomicsScreen::render_single_view() {
    // Series tags header
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
    ImGui::Text("SINGLE VIEW -");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::INFO);
    ImGui::Text("%d SERIES", static_cast<int>(single_view_series_.size()));
    ImGui::PopStyleColor();

    // Series tags (removable)
    for (int i = 0; i < static_cast<int>(single_view_series_.size()); i++) {
        ImGui::PushID(i);
        auto col = get_chart_color(single_view_series_[i].color_index);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("%s", single_view_series_[i].series_name.substr(0, 20).c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::ERROR_RED);
        if (ImGui::SmallButton("x##rmsvs"))
            remove_from_single_view(i);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PopID();
    }

    if (single_view_series_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
        ImGui::Text("No series added");
        ImGui::PopStyleColor();
    }
    ImGui::NewLine();
    ImGui::Separator();

    if (!single_view_series_.empty()) {
        render_chart(single_view_series_, single_chart_type_);
        ImGui::Spacing();
        render_data_table(single_view_series_);
    } else {
        // Empty state
        ImVec2 center = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2(center.x * 0.3f, center.y * 0.4f));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
        ImGui::Text("SELECT A SERIES AND ADD TO VIEW");
        ImGui::PopStyleColor();
    }
}

// ============================================================================
// Render — Comparison View
// ============================================================================

void DBnomicsScreen::render_comparison_view() {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
    ImGui::Text("COMPARISON DASHBOARD");
    ImGui::PopStyleColor();
    ImGui::Separator();

    int cols = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / 400.0f));
    int col = 0;

    for (int si = 0; si < static_cast<int>(slots_.size()); si++) {
        if (col > 0) ImGui::SameLine();

        float w = (ImGui::GetContentRegionAvail().x - (cols - 1) * 8.0f) / cols;
        ImGui::BeginChild(("##slot_" + std::to_string(si)).c_str(), ImVec2(w, 320), ImGuiChildFlags_Borders);

        // Slot header with chart type selector
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
        ImGui::Text("SLOT %d", si + 1);
        ImGui::PopStyleColor();
        ImGui::SameLine();

        static const ChartType types[] = {ChartType::Line, ChartType::Bar, ChartType::Area, ChartType::Scatter};
        for (auto ct : types) {
            ImGui::SameLine();
            bool active = (slot_chart_types_[si] == ct);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT);
            if (ImGui::SmallButton((std::string(chart_type_label(ct)) + "##s" + std::to_string(si)).c_str()))
                slot_chart_types_[si] = ct;
            if (active) ImGui::PopStyleColor();
        }

        ImGui::Separator();

        if (!slots_[si].empty()) {
            render_chart(slots_[si], slot_chart_types_[si]);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
            ImGui::SetCursorPos(ImVec2(w * 0.3f, 150.0f));
            ImGui::Text("EMPTY SLOT");
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();

        col++;
        if (col >= cols) col = 0;
    }
}

// ============================================================================
// Render — Chart (ImPlot)
// ============================================================================

void DBnomicsScreen::render_chart(const std::vector<DataPoint>& series_arr, ChartType chart_type) {
    if (series_arr.empty()) return;

    float h = 300.0f;
    if (ImPlot::BeginPlot("##dbn_chart", ImVec2(-1, h))) {
        ImPlot::SetupAxes("Period", "Value");

        for (const auto& dp : series_arr) {
            if (dp.observations.empty()) continue;

            // Prepare data arrays
            int n = static_cast<int>(dp.observations.size());
            std::vector<double> xs(n), ys(n);
            for (int i = 0; i < n; i++) {
                xs[i] = static_cast<double>(i);
                ys[i] = dp.observations[i].value;
            }

            auto col = get_chart_color(dp.color_index);
            ImPlot::SetNextLineStyle(col);
            ImPlot::SetNextFillStyle(col, 0.25f);

            std::string label = dp.series_name.substr(0, 20);

            switch (chart_type) {
                case ChartType::Line:
                    ImPlot::PlotLine(label.c_str(), xs.data(), ys.data(), n);
                    break;
                case ChartType::Bar:
                    ImPlot::PlotBars(label.c_str(), xs.data(), ys.data(), n, 0.6);
                    break;
                case ChartType::Area:
                    ImPlot::PlotShaded(label.c_str(), xs.data(), ys.data(), n);
                    ImPlot::PlotLine(label.c_str(), xs.data(), ys.data(), n);
                    break;
                case ChartType::Scatter:
                    ImPlot::PlotScatter(label.c_str(), xs.data(), ys.data(), n);
                    break;
            }
        }
        ImPlot::EndPlot();
    }
}

// ============================================================================
// Render — Data Table
// ============================================================================

void DBnomicsScreen::render_data_table(const std::vector<DataPoint>& series_arr) {
    if (series_arr.empty()) return;

    theme::SectionHeader("DATA TABLE");

    // Collect all unique periods, sorted descending
    std::set<std::string, std::greater<>> all_periods;
    for (const auto& dp : series_arr)
        for (const auto& obs : dp.observations)
            all_periods.insert(obs.period);

    int n_cols = 1 + static_cast<int>(series_arr.size());
    if (ImGui::BeginTable("##dbn_table", n_cols,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, 250))) {

        ImGui::TableSetupColumn("PERIOD", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        for (const auto& dp : series_arr)
            ImGui::TableSetupColumn(dp.series_name.substr(0, 25).c_str());
        ImGui::TableHeadersRow();

        int row_count = 0;
        for (const auto& period : all_periods) {
            if (row_count++ >= 50) break;  // limit to 50 rows

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", period.c_str());

            for (const auto& dp : series_arr) {
                ImGui::TableNextColumn();
                // Find observation for this period
                bool found = false;
                for (const auto& obs : dp.observations) {
                    if (obs.period == period) {
                        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::INFO);
                        ImGui::Text("%.4f", obs.value);
                        ImGui::PopStyleColor();
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
                    ImGui::Text("--");
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::EndTable();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
    ImGui::Text("SHOWING LATEST 50 PERIODS");
    ImGui::PopStyleColor();
}

// ============================================================================
// Render — Status Bar
// ============================================================================

void DBnomicsScreen::render_status_bar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::colors::BG_PANEL);
    ImGui::BeginChild("##dbn_statusbar", ImVec2(0, 20), ImGuiChildFlags_Borders);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
    ImGui::SetCursorPosX(8.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::ACCENT);
    ImGui::Text("DBNOMICS");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);
    ImGui::Text("PROVIDERS: %d  DATASETS: %d  SERIES: %d",
                static_cast<int>(providers_.size()),
                static_cast<int>(datasets_.size()),
                static_cast<int>(series_list_.size()));
    ImGui::PopStyleColor();
    ImGui::SameLine();

    float right_x = ImGui::GetContentRegionMax().x;
    ImGui::SameLine(right_x - 200.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, error_.empty() ? theme::colors::TEXT_DIM : theme::colors::ERROR_RED);
    ImGui::Text("%s", error_.empty() ? status_.c_str() : error_.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Text, selected_dataset_.empty() ? theme::colors::TEXT_DIM : theme::colors::SUCCESS);
    ImGui::Text("DATASET: %s", selected_dataset_.empty() ? "NONE" : "SELECTED");
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::dbnomics
