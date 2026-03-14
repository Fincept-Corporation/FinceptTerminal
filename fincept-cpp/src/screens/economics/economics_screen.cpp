// Economics Screen — Multi-source economic data visualization
// Port of EconomicsTab.tsx — 15 data sources, country/indicator selectors,
// line charts, stats bar, data table, Fincept CEIC drill-down

#include "economics_screen.h"
#include "python/python_runner.h"
#include "http/http_client.h"
#include "auth/auth_manager.h"
#include "storage/database.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <implot.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <ctime>

namespace fincept::economics {

// ============================================================================
// Date range helpers
// ============================================================================

static void compute_date_range(int preset_idx, char* start, char* end) {
    time_t now = time(nullptr);
    struct tm t_end;
#ifdef _WIN32
    localtime_s(&t_end, &now);
#else
    localtime_r(&now, &t_end);
#endif
    std::snprintf(end, 16, "%04d-%02d-%02d", t_end.tm_year + 1900, t_end.tm_mon + 1, t_end.tm_mday);

    struct tm t_start = t_end;
    switch (preset_idx) {
        case 0: t_start.tm_year -= 1; break;  // 1Y
        case 1: t_start.tm_year -= 3; break;  // 3Y
        case 2: t_start.tm_year -= 5; break;  // 5Y
        case 3: t_start.tm_year -= 10; break; // 10Y
        case 4: t_start.tm_year = 70; break;  // ALL (1970)
        default: t_start.tm_year -= 5; break;
    }
    std::snprintf(start, 16, "%04d-%02d-%02d", t_start.tm_year + 1900, t_start.tm_mon + 1, t_start.tm_mday);
}

static std::string format_value(double val) {
    char buf[32];
    if (std::abs(val) >= 1e12)      std::snprintf(buf, sizeof(buf), "%.2fT", val / 1e12);
    else if (std::abs(val) >= 1e9)  std::snprintf(buf, sizeof(buf), "%.2fB", val / 1e9);
    else if (std::abs(val) >= 1e6)  std::snprintf(buf, sizeof(buf), "%.2fM", val / 1e6);
    else if (std::abs(val) >= 1e3)  std::snprintf(buf, sizeof(buf), "%.2fK", val / 1e3);
    else if (std::abs(val) < 0.01 && val != 0.0) std::snprintf(buf, sizeof(buf), "%.2e", val);
    else std::snprintf(buf, sizeof(buf), "%.2f", val);
    return buf;
}

// ============================================================================
// Initialization helpers
// ============================================================================

void EconomicsScreen::load_api_keys() {
    if (api_keys_loaded_) return;
    api_keys_loaded_ = true;
    const char* key_names[] = {"wto_api_key", "eia_api_key", "bls_api_key", "bea_api_key"};
    for (const auto* name : key_names) {
        auto cred = db::ops::get_credential_by_service(name);
        if (cred && cred->api_key) {
            api_keys_[name] = *cred->api_key;
        }
    }
}

void EconomicsScreen::save_api_key(const std::string& key_name, const std::string& key_value) {
    db::Credential cred;
    cred.service_name = key_name;
    cred.api_key = key_value;
    db::ops::save_credential(cred);
    api_keys_[key_name] = key_value;
}

std::string EconomicsScreen::get_api_key(const std::string& key_name) const {
    auto it = api_keys_.find(key_name);
    return it != api_keys_.end() ? it->second : "";
}

void EconomicsScreen::set_data_source(DataSource src) {
    data_source_ = src;
    selected_indicator_ = get_default_indicator(src);
    has_data_ = false;
    has_fincept_table_ = false;
    error_.clear();
    indicator_search_[0] = '\0';

    // Set appropriate default country
    if (src == DataSource::ADB)
        selected_country_ = "IND";
    else if (src == DataSource::FRED || src == DataSource::Fed ||
             src == DataSource::BLS || src == DataSource::FiscalData || src == DataSource::BEA)
        selected_country_ = "USA";
}

void EconomicsScreen::update_date_range() {
    compute_date_range(date_preset_idx_, start_date_, end_date_);
}

std::string EconomicsScreen::get_country_code() const {
    for (int i = 0; i < COUNTRY_COUNT; i++) {
        if (selected_country_ == COUNTRIES[i].code) {
            const auto& c = COUNTRIES[i];
            switch (data_source_) {
                case DataSource::BIS:  return c.bis ? c.bis : selected_country_;
                case DataSource::IMF:  return c.imf ? c.imf : selected_country_;
                case DataSource::FRED: return "US";
                case DataSource::OECD: return c.oecd ? c.oecd : selected_country_;
                case DataSource::WTO:  return c.wto ? c.wto : selected_country_;
                case DataSource::ADB:  return c.adb ? c.adb : selected_country_;
                default: return selected_country_;
            }
        }
    }
    return selected_country_;
}

// ============================================================================
// Data fetching
// ============================================================================

void EconomicsScreen::fetch_data() {
    if (loading_) return;
    loading_ = true;
    error_.clear();
    has_fincept_table_ = false;

    const auto& cfg = get_source_config(data_source_);
    std::string country_code = get_country_code();

    try {
        // Check API key requirements
        if (cfg.requires_api_key && cfg.api_key_name) {
            std::string key = get_api_key(cfg.api_key_name);
            if (key.empty()) {
                // EIA only needs key for STEO
                bool needs_key = true;
                if (data_source_ == DataSource::EIA) {
                    std::string ind(selected_indicator_);
                    needs_key = ind.substr(0, 5) == "steo_";
                }
                if (needs_key) {
                    error_ = std::string(cfg.name) + " API Key required. Configure in the key panel.";
                    show_api_key_panel_ = true;
                    loading_ = false;
                    return;
                }
            }
        }

        std::string raw_json;

        if (data_source_ == DataSource::Fincept) {
            // Direct HTTP to Fincept API
            const std::string BASE = "https://api.fincept.in";
            std::string url;
            std::string ind(selected_indicator_);

            static const char* CEIC_COUNTRIES[] = {
                "china", "european-union", "france", "germany", "italy", "japan", "united-states"
            };
            std::string ceic_country = CEIC_COUNTRIES[fincept_ceic_country_idx_];

            if (ind == "ceic_series_countries")
                url = BASE + "/macro/ceic/series/countries";
            else if (ind == "ceic_series_indicators")
                url = BASE + "/macro/ceic/series/indicators?country=" + ceic_country;
            else if (ind == "ceic_series") {
                url = BASE + "/macro/ceic/series?country=" + ceic_country;
                if (!fincept_ceic_indicator_.empty())
                    url += "&indicator=" + fincept_ceic_indicator_;
                url += "&year_from=" + std::string(start_date_).substr(0, 4);
                url += "&year_to=" + std::string(end_date_).substr(0, 4);
                url += "&limit=500";
            } else if (ind == "economic_calendar")
                url = BASE + "/macro/economic-calendar?start_date=" + std::string(start_date_) + "&end_date=" + std::string(end_date_) + "&limit=200";
            else if (ind == "upcoming_events")
                url = BASE + "/macro/upcoming-events?limit=200";
            else if (ind == "wgb_central_bank_rates")
                url = BASE + "/macro/wgb/central-bank-rates";
            else if (ind == "wgb_credit_ratings")
                url = BASE + "/macro/wgb/credit-ratings";
            else if (ind == "wgb_sovereign_cds")
                url = BASE + "/macro/wgb/sovereign-cds";
            else if (ind == "wgb_bond_spreads")
                url = BASE + "/macro/wgb/bond-spreads";
            else if (ind == "wgb_inverted_yields")
                url = BASE + "/macro/wgb/inverted-yields";
            else
                url = BASE + "/macro/wgb/central-bank-rates";

            auto& auth = auth::AuthManager::instance();
            std::string api_key = auth.session().api_key;
            http::Headers headers = {{"X-API-Key", api_key}, {"Accept", "application/json"}};
            auto resp = http::HttpClient::instance().get(url, headers);
            if (!resp.success) {
                error_ = "HTTP error: " + std::to_string(resp.status_code);
                loading_ = false;
                return;
            }
            raw_json = resp.body;
        } else {
            // Python script execution
            std::vector<std::string> args;
            std::string env_key, env_val;

            switch (data_source_) {
                case DataSource::WorldBank:
                    args = {"indicators", country_code, selected_indicator_};
                    break;
                case DataSource::BIS:
                    args = {"fetch", selected_indicator_, country_code};
                    break;
                case DataSource::IMF:
                    args = {"weo", country_code, selected_indicator_};
                    break;
                case DataSource::FRED:
                    args = {"series", selected_indicator_};
                    break;
                case DataSource::OECD:
                    args = {"fetch", selected_indicator_, country_code};
                    break;
                case DataSource::WTO:
                    args = {"timeseries_data", "--i", selected_indicator_, "--reporters", country_code, get_api_key("wto_api_key")};
                    break;
                case DataSource::CFTC:
                    args = {"cot_historical_trend", selected_indicator_, "legacy", "104"};
                    break;
                case DataSource::EIA: {
                    std::string ind(selected_indicator_);
                    if (ind.substr(0, 5) == "steo_") {
                        std::string table = ind.substr(5);
                        args = {"get_steo", table, "", "month"};
                        env_key = "eia_api_key";
                        env_val = get_api_key("eia_api_key");
                    } else {
                        args = {"get_petroleum", selected_indicator_};
                    }
                    break;
                }
                case DataSource::ADB: {
                    std::string ind(selected_indicator_);
                    std::string sy = std::string(start_date_).substr(0, 4);
                    std::string ey = std::string(end_date_).substr(0, 4);
                    if (ind.substr(0, 3) == "LP_" || ind.substr(0, 3) == "SP_" || ind.substr(0, 3) == "SM_")
                        args = {"get_population", country_code, selected_indicator_, sy, ey};
                    else
                        args = {"get_gdp", country_code, selected_indicator_, sy, ey};
                    break;
                }
                case DataSource::Fed: {
                    std::string ind(selected_indicator_);
                    if (ind == "market_overview")
                        args = {"market_overview"};
                    else if (ind == "yield_curve")
                        args = {"yield_curve", end_date_};
                    else if (ind == "money_measures")
                        args = {"money_measures", start_date_, end_date_, "false"};
                    else if (ind == "money_measures_adjusted")
                        args = {"money_measures", start_date_, end_date_, "true"};
                    else
                        args = {selected_indicator_, start_date_, end_date_};
                    break;
                }
                case DataSource::BLS:
                    args = {"get_series", selected_indicator_, start_date_, end_date_};
                    env_key = "bls_api_key";
                    env_val = get_api_key("bls_api_key");
                    break;
                case DataSource::UNESCO: {
                    std::string sy = std::string(start_date_).substr(0, 4);
                    std::string ey = std::string(end_date_).substr(0, 4);
                    args = {"fetch", selected_indicator_, country_code, sy, ey};
                    break;
                }
                case DataSource::FiscalData:
                    args = {"fetch", selected_indicator_, start_date_, end_date_};
                    break;
                case DataSource::BEA:
                    args = {"fetch", selected_indicator_, start_date_, end_date_};
                    env_key = "bea_api_key";
                    env_val = get_api_key("bea_api_key");
                    break;
                default:
                    break;
            }

            // Build env JSON if needed
            std::string stdin_data = "{}";
            // For scripts that need env vars, we pass them via args or stdin
            // The python_runner uses execute_with_stdin which takes script, args, stdin

            auto result = python::execute_with_stdin(cfg.script_name, args, stdin_data);
            if (!result.success || result.output.empty()) {
                error_ = result.error.empty() ? "No data returned" : result.error;
                loading_ = false;
                return;
            }
            raw_json = result.output;
        }

        parse_response(raw_json);

    } catch (const std::exception& e) {
        error_ = std::string("Failed to fetch data: ") + e.what();
    }

    loading_ = false;
}

void EconomicsScreen::parse_response(const std::string& raw_json) {
    auto parsed = nlohmann::json::parse(raw_json, nullptr, false);
    if (parsed.is_discarded()) {
        error_ = "Failed to parse response JSON";
        return;
    }

    // Check for error
    if (parsed.contains("error") || (parsed.contains("success") && parsed["success"] == false)) {
        if (parsed.contains("error")) {
            if (parsed["error"].is_string()) error_ = parsed["error"].get<std::string>();
            else error_ = parsed["error"].dump();
        } else if (parsed.contains("message")) {
            error_ = parsed["message"].get<std::string>();
        } else {
            error_ = "An error occurred while fetching data";
        }
        has_data_ = false;
        return;
    }

    const auto& cfg = get_source_config(data_source_);
    auto inds = get_indicators(data_source_);

    // Find indicator name from our constants
    std::string indicator_name;
    for (int i = 0; i < inds.count; i++) {
        if (std::string(inds.items[i].id) == selected_indicator_) {
            indicator_name = inds.items[i].name;
            break;
        }
    }

    // Fincept: table display, not chart
    if (data_source_ == DataSource::Fincept) {
        auto data_block = parsed.value("data", nlohmann::json::object());
        const char* known_keys[] = {
            "central_bank_rates", "credit_ratings", "sovereign_cds",
            "bond_spreads", "inverted_yields", "events",
            "countries", "indicators", "series"
        };
        nlohmann::json rows;
        for (const auto* key : known_keys) {
            if (data_block.contains(key) && data_block[key].is_array() && !data_block[key].empty()) {
                rows = data_block[key];
                break;
            }
        }
        // Fallback: first array in data_block
        if (rows.is_null() || rows.empty()) {
            for (auto& [k, v] : data_block.items()) {
                if (v.is_array() && !v.empty()) { rows = v; break; }
            }
        }

        if (!rows.is_null() && rows.is_array() && !rows.empty()) {
            fincept_table_ = rows;
            fincept_columns_.clear();
            static const char* HIDDEN[] = {"scraped_at", "id", "detail_url", "url", "created_at", "event_id", "source"};
            for (auto& [k, v] : rows[0].items()) {
                bool hidden = false;
                for (const auto* h : HIDDEN) { if (k == h) { hidden = true; break; } }
                if (!hidden) fincept_columns_.push_back(k);
            }
            has_fincept_table_ = true;
            has_data_ = false; // No chart for Fincept

            data_.indicator = selected_indicator_;
            data_.country = selected_country_;
            data_.indicator_name = indicator_name;
            data_.source = "Fincept";
            data_.data.clear();
        } else {
            error_ = "No data returned for this endpoint";
        }
        return;
    }

    // Standard data sources — extract time series
    std::vector<DataPoint> raw_data;
    std::string country_name;

    if (data_source_ == DataSource::WTO) {
        auto dataset = parsed.contains("data") && parsed["data"].contains("Dataset")
            ? parsed["data"]["Dataset"] : parsed.value("Dataset", nlohmann::json::array());
        for (auto& d : dataset) {
            raw_data.push_back({
                d.value("Year", d.value("year", std::string(""))),
                d.value("Value", d.value("value", 0.0))
            });
        }
    } else if (data_source_ == DataSource::CFTC) {
        for (auto& d : parsed.value("data", nlohmann::json::array())) {
            double val = d.value("non_commercial_net",
                         d.value("commercial_net",
                         d.value("open_interest", 0.0)));
            raw_data.push_back({d.value("date", std::string("")), val});
        }
        indicator_name += " - Net Speculator Position";
        country_name = "Global";
    } else if (data_source_ == DataSource::EIA) {
        for (auto& d : parsed.value("data", nlohmann::json::array())) {
            raw_data.push_back({
                d.value("date", d.value("period", std::string(""))),
                d.value("value", 0.0)
            });
        }
        country_name = "United States";
    } else if (data_source_ == DataSource::ADB) {
        for (auto& d : parsed.value("data", nlohmann::json::array())) {
            raw_data.push_back({
                d.value("time_period", d.value("date", std::string(""))),
                d.value("value", 0.0)
            });
        }
    } else if (data_source_ == DataSource::Fed) {
        auto fed_data = parsed.value("data", nlohmann::json::array());
        std::string ind(selected_indicator_);
        if (ind == "federal_funds_rate" || ind == "sofr_rate") {
            for (auto& d : fed_data) {
                double rate = d.value("rate", 0.0) * 100.0;
                raw_data.push_back({d.value("date", std::string("")), rate});
            }
        } else if (ind == "treasury_rates") {
            for (auto& d : fed_data) {
                double rate = d.value("year_10", 0.0) * 100.0;
                raw_data.push_back({d.value("date", std::string("")), rate});
            }
            indicator_name = "10-Year Treasury Rate";
        } else if (ind == "money_measures" || ind == "money_measures_adjusted") {
            for (auto& d : fed_data) {
                raw_data.push_back({
                    d.value("month", d.value("date", std::string(""))),
                    d.value("M2", d.value("m2", 0.0))
                });
            }
        } else if (ind == "market_overview") {
            auto ffr = parsed.contains("endpoints") &&
                       parsed["endpoints"].contains("federal_funds_rate")
                ? parsed["endpoints"]["federal_funds_rate"].value("data", nlohmann::json::array())
                : nlohmann::json::array();
            for (auto& d : ffr) {
                double rate = d.value("rate", 0.0) * 100.0;
                raw_data.push_back({d.value("date", std::string("")), rate});
            }
            indicator_name = "Federal Funds Rate";
        } else {
            for (auto& d : fed_data) {
                raw_data.push_back({
                    d.value("date", d.value("month", std::string(""))),
                    d.value("value", d.value("rate", d.value("M2", 0.0)))
                });
            }
        }
        country_name = "United States";
    } else if (data_source_ == DataSource::BLS) {
        auto series = parsed.contains("data") && parsed["data"].contains("series_data")
            ? parsed["data"]["series_data"] : nlohmann::json::array();
        for (auto& d : series) {
            raw_data.push_back({d.value("date", std::string("")), d.value("value", 0.0)});
        }
        country_name = "United States";
    } else if (data_source_ == DataSource::UNESCO ||
               data_source_ == DataSource::FiscalData ||
               data_source_ == DataSource::BEA) {
        for (auto& d : parsed.value("data", nlohmann::json::array())) {
            raw_data.push_back({d.value("date", std::string("")), d.value("value", 0.0)});
        }
        if (data_source_ == DataSource::FiscalData || data_source_ == DataSource::BEA)
            country_name = "United States";
    } else {
        // WorldBank, BIS, IMF, OECD — generic format
        for (auto& d : parsed.value("data", nlohmann::json::array())) {
            std::string date = d.value("date", d.value("period", d.value("year", std::string(""))));
            double val = d.value("value", 0.0);
            raw_data.push_back({date, val});
        }
    }

    // Use metadata if available
    if (parsed.contains("metadata")) {
        auto& meta = parsed["metadata"];
        if (indicator_name.empty())
            indicator_name = meta.value("indicator_name", meta.value("description", std::string("")));
        if (country_name.empty())
            country_name = meta.value("country_name", meta.value("economy_name", std::string("")));
    }

    // Find country name from our list
    if (country_name.empty()) {
        for (int i = 0; i < COUNTRY_COUNT; i++) {
            if (selected_country_ == COUNTRIES[i].code) {
                country_name = COUNTRIES[i].name;
                break;
            }
        }
    }

    // Filter valid data + sort + apply date range
    data_.data.clear();
    for (auto& dp : raw_data) {
        if (dp.date.empty() || std::isnan(dp.value)) continue;
        // Normalize date for comparison
        std::string norm = dp.date;
        if (norm.size() == 4) norm += "-01-01";
        else if (norm.size() == 7) norm += "-01";
        if (norm >= start_date_ && norm <= end_date_) {
            data_.data.push_back(dp);
        }
    }

    std::sort(data_.data.begin(), data_.data.end(),
              [](const DataPoint& a, const DataPoint& b) { return a.date < b.date; });

    if (data_.data.empty()) {
        error_ = "No data available for this selection";
        has_data_ = false;
    } else {
        data_.indicator = selected_indicator_;
        data_.country = selected_country_;
        data_.indicator_name = indicator_name;
        data_.country_name = country_name;
        data_.source = cfg.name;
        has_data_ = true;
        compute_stats();
    }
}

void EconomicsScreen::compute_stats() {
    if (data_.data.empty()) return;
    double latest = data_.data.back().value;
    double prev = data_.data.size() > 1 ? data_.data[data_.data.size() - 2].value : latest;
    double change = (prev != 0.0) ? ((latest - prev) / std::abs(prev)) * 100.0 : 0.0;
    double mn = data_.data[0].value, mx = data_.data[0].value, sum = 0.0;
    for (const auto& dp : data_.data) {
        mn = std::min(mn, dp.value);
        mx = std::max(mx, dp.value);
        sum += dp.value;
    }
    stats_ = {latest, change, mn, mx, sum / (double)data_.data.size(), (int)data_.data.size()};
}

void EconomicsScreen::fetch_fincept_ceic_indicators() {
    if (fincept_ceic_loading_) return;
    fincept_ceic_loading_ = true;
    fincept_ceic_list_.clear();
    fincept_ceic_indicator_.clear();

    static const char* CEIC_COUNTRIES[] = {
        "china", "european-union", "france", "germany", "italy", "japan", "united-states"
    };
    std::string country = CEIC_COUNTRIES[fincept_ceic_country_idx_];

    try {
        auto& auth_mgr = auth::AuthManager::instance();
        std::string api_key = auth_mgr.session().api_key;
        std::string url = "https://api.fincept.in/macro/ceic/series/indicators?country=" + country;
        http::Headers headers = {{"X-API-Key", api_key}, {"Accept", "application/json"}};
        auto resp = http::HttpClient::instance().get(url, headers);

        if (resp.success && !resp.body.empty()) {
            auto j = nlohmann::json::parse(resp.body, nullptr, false);
            if (!j.is_discarded() && j.contains("data") && j["data"].contains("indicators")) {
                for (auto& ind : j["data"]["indicators"]) {
                    std::string name = ind.value("indicator", "");
                    int pts = ind.value("data_points", 0);
                    fincept_ceic_list_.push_back({name, pts});
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("EconomicsScreen", "CEIC indicators fetch failed: %s", e.what());
    }
    fincept_ceic_loading_ = false;
}

// ============================================================================
// Main render
// ============================================================================

void EconomicsScreen::render() {
    load_api_keys();

    // Initialize date range on first render
    static bool date_init = false;
    if (!date_init) { update_date_range(); date_init = true; }

    render_header();

    if (show_api_key_panel_) {
        render_api_key_panel();
    }

    // Main content: left panel (selectors) + right panel (chart/data)
    float avail_h = ImGui::GetContentRegionAvail().y - 22; // Reserve for status bar
    ImGui::BeginChild("##econ_main", ImVec2(0, avail_h));

    // Left panel — 280px
    ImGui::BeginChild("##econ_left", ImVec2(280, 0), ImGuiChildFlags_Borders);
    if (data_source_ == DataSource::Fincept) {
        render_fincept_selector();
    } else {
        render_country_selector();
        render_indicator_selector();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel — rest of width
    ImGui::BeginChild("##econ_right", ImVec2(0, 0));

    if (has_data_) render_stats_bar();

    if (loading_) {
        float w = ImGui::GetContentRegionAvail().x;
        float h = ImGui::GetContentRegionAvail().y;
        ImVec2 center(w * 0.5f, h * 0.5f);
        ImGui::SetCursorPos(center);
        theme::LoadingSpinner("Loading data...");
    } else if (!error_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(theme::colors::ERROR_RED, "Error: %s", error_.c_str());
        ImGui::Spacing();
        if (theme::AccentButton("Try Again")) fetch_data();
    } else if (has_fincept_table_) {
        render_fincept_table();
    } else if (has_data_) {
        render_chart();
        ImGui::Spacing();
        render_data_table();
    } else {
        float w = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(w * 0.3f);
        ImGui::SetCursorPosY(ImGui::GetContentRegionAvail().y * 0.4f);
        ImGui::TextColored(theme::colors::TEXT_DIM, "Select data and click FETCH DATA");
    }

    ImGui::EndChild();
    ImGui::EndChild();

    render_status_bar();
}

// ============================================================================
// Header bar
// ============================================================================

void EconomicsScreen::render_header() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::colors::BG_PANEL);
    ImGui::BeginChild("##econ_header", ImVec2(0, 36), ImGuiChildFlags_Borders);

    // Title
    ImGui::TextColored(theme::colors::ACCENT, "ECONOMIC DATA");
    ImGui::SameLine(0, 16);

    // Data source combo
    const auto& cur = get_source_config(data_source_);
    ImGui::PushStyleColor(ImGuiCol_Text, cur.color);
    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("##src", cur.full_name)) {
        for (int i = 0; i < DATA_SOURCE_COUNT; i++) {
            const auto& s = DATA_SOURCES[i];
            bool selected = (i == static_cast<int>(data_source_));
            ImGui::PushStyleColor(ImGuiCol_Text, s.color);
            if (ImGui::Selectable(s.full_name, selected)) {
                set_data_source(s.id);
            }
            ImGui::PopStyleColor();
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor();

    // API Key button (if source requires it)
    if (cur.requires_api_key && cur.api_key_name) {
        ImGui::SameLine(0, 8);
        std::string key = get_api_key(cur.api_key_name);
        ImVec4 btn_col = key.empty() ? theme::colors::ERROR_RED : theme::colors::SUCCESS;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(btn_col.x, btn_col.y, btn_col.z, 0.2f));
        ImGui::PushStyleColor(ImGuiCol_Text, btn_col);
        if (ImGui::SmallButton(key.empty() ? "SET API KEY" : "API KEY SET")) {
            show_api_key_panel_ = !show_api_key_panel_;
            if (show_api_key_panel_) {
                std::strncpy(api_key_input_, key.c_str(), sizeof(api_key_input_) - 1);
            }
        }
        ImGui::PopStyleColor(2);
    }

    // Date range preset
    ImGui::SameLine(0, 16);
    static const char* presets[] = {"1Y", "3Y", "5Y", "10Y", "ALL"};
    ImGui::SetNextItemWidth(60);
    if (ImGui::BeginCombo("##date", presets[date_preset_idx_])) {
        for (int i = 0; i < 5; i++) {
            if (ImGui::Selectable(presets[i], i == date_preset_idx_)) {
                date_preset_idx_ = i;
                update_date_range();
            }
        }
        ImGui::EndCombo();
    }

    // Fetch button
    ImGui::SameLine(0, 16);
    if (theme::AccentButton("FETCH DATA")) {
        fetch_data();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// API Key panel
// ============================================================================

void EconomicsScreen::render_api_key_panel() {
    const auto& cur = get_source_config(data_source_);
    if (!cur.requires_api_key || !cur.api_key_name) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::colors::BG_WIDGET);
    ImGui::BeginChild("##api_key_panel", ImVec2(0, 60), ImGuiChildFlags_Borders);

    ImGui::TextColored(cur.color, "%s API KEY", cur.name);
    ImGui::SameLine(0, 16);

    ImGui::SetNextItemWidth(300);
    ImGui::InputText("##api_key", api_key_input_, sizeof(api_key_input_),
                     ImGuiInputTextFlags_Password);
    ImGui::SameLine();
    if (theme::AccentButton("Save")) {
        save_api_key(cur.api_key_name, api_key_input_);
        show_api_key_panel_ = false;
    }
    ImGui::SameLine();
    if (theme::SecondaryButton("Close")) {
        show_api_key_panel_ = false;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Country selector (left panel)
// ============================================================================

void EconomicsScreen::render_country_selector() {
    theme::SectionHeader("COUNTRY");
    ImGui::SameLine();
    ImGui::TextColored(theme::colors::TEXT_DIM, "(%d)", COUNTRY_COUNT);

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##csearch", "Search countries...", country_search_, sizeof(country_search_));

    float h = ImGui::GetContentRegionAvail().y * 0.35f;
    ImGui::BeginChild("##country_list", ImVec2(-1, h));

    for (int i = 0; i < COUNTRY_COUNT; i++) {
        const auto& c = COUNTRIES[i];
        // Filter
        if (country_search_[0] != '\0') {
            std::string search_lower = country_search_;
            std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
            std::string name_lower = c.name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            std::string code_lower = c.code;
            std::transform(code_lower.begin(), code_lower.end(), code_lower.begin(), ::tolower);
            if (name_lower.find(search_lower) == std::string::npos &&
                code_lower.find(search_lower) == std::string::npos)
                continue;
        }

        bool selected = (selected_country_ == c.code);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_PRIMARY);
        else ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_DIM);

        char label[128];
        std::snprintf(label, sizeof(label), "%-30s %s", c.name, c.code);
        if (ImGui::Selectable(label, selected)) {
            selected_country_ = c.code;
        }
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
}

// ============================================================================
// Indicator selector (left panel)
// ============================================================================

void EconomicsScreen::render_indicator_selector() {
    ImGui::Separator();
    const auto& cfg = get_source_config(data_source_);
    auto inds = get_indicators(data_source_);

    char header[64];
    std::snprintf(header, sizeof(header), "INDICATORS (%s)", cfg.name);
    theme::SectionHeader(header);
    ImGui::SameLine();
    ImGui::TextColored(theme::colors::TEXT_DIM, "(%d)", inds.count);

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##isearch", "Search indicators...", indicator_search_, sizeof(indicator_search_));

    ImGui::BeginChild("##indicator_list", ImVec2(-1, 0));

    for (int i = 0; i < inds.count; i++) {
        const auto& ind = inds.items[i];
        // Filter
        if (indicator_search_[0] != '\0') {
            std::string search_lower = indicator_search_;
            std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
            std::string name_lower = ind.name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            std::string id_lower = ind.id;
            std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);
            std::string cat_lower = ind.category;
            std::transform(cat_lower.begin(), cat_lower.end(), cat_lower.begin(), ::tolower);
            if (name_lower.find(search_lower) == std::string::npos &&
                id_lower.find(search_lower) == std::string::npos &&
                cat_lower.find(search_lower) == std::string::npos)
                continue;
        }

        bool selected = (selected_indicator_ == ind.id);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Text, cfg.color);
        else ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_SECONDARY);

        if (ImGui::Selectable(ind.name, selected)) {
            selected_indicator_ = ind.id;
        }
        ImGui::PopStyleColor();

        // Category + ID subtitle
        ImGui::TextColored(theme::colors::TEXT_DISABLED, "  %s  |  %s", ind.category, ind.id);
    }

    ImGui::EndChild();
}

// ============================================================================
// Fincept selector (left panel — CEIC drill-down)
// ============================================================================

void EconomicsScreen::render_fincept_selector() {
    theme::SectionHeader("ENDPOINT");

    auto inds = get_indicators(DataSource::Fincept);
    for (int i = 0; i < inds.count; i++) {
        bool selected = (selected_indicator_ == inds.items[i].id);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::ACCENT);
        else ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_SECONDARY);

        if (ImGui::Selectable(inds.items[i].name, selected)) {
            selected_indicator_ = inds.items[i].id;
        }
        ImGui::PopStyleColor();
        ImGui::TextColored(theme::colors::TEXT_DISABLED, "  %s", inds.items[i].category);
    }

    // CEIC drill-down (for ceic_series and ceic_series_indicators)
    std::string ind(selected_indicator_);
    if (ind == "ceic_series" || ind == "ceic_series_indicators") {
        ImGui::Separator();
        theme::SectionHeader("CEIC COUNTRY");

        static const char* CEIC_COUNTRIES[] = {
            "china", "european-union", "france", "germany", "italy", "japan", "united-states"
        };
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##ceic_country", CEIC_COUNTRIES[fincept_ceic_country_idx_])) {
            for (int i = 0; i < 7; i++) {
                if (ImGui::Selectable(CEIC_COUNTRIES[i], i == fincept_ceic_country_idx_)) {
                    fincept_ceic_country_idx_ = i;
                }
            }
            ImGui::EndCombo();
        }

        if (fincept_ceic_loading_) {
            theme::LoadingSpinner("Loading...");
        } else {
            if (theme::AccentButton("Load Indicators")) {
                fetch_fincept_ceic_indicators();
            }
        }
    }

    // CEIC indicator picker (for ceic_series only)
    if (ind == "ceic_series" && !fincept_ceic_list_.empty()) {
        ImGui::Separator();
        char hdr[64];
        std::snprintf(hdr, sizeof(hdr), "INDICATOR (%d)", (int)fincept_ceic_list_.size());
        theme::SectionHeader(hdr);

        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##ceic_search", "Search...", fincept_ceic_search_, sizeof(fincept_ceic_search_));

        ImGui::BeginChild("##ceic_list", ImVec2(-1, 0));
        for (const auto& [name, pts] : fincept_ceic_list_) {
            if (fincept_ceic_search_[0] != '\0') {
                std::string search_lower = fincept_ceic_search_;
                std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
                std::string name_lower = name;
                std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
                if (name_lower.find(search_lower) == std::string::npos) continue;
            }

            bool sel = (fincept_ceic_indicator_ == name);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::ACCENT);
            else ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::TEXT_SECONDARY);

            if (ImGui::Selectable(name.c_str(), sel)) {
                fincept_ceic_indicator_ = name;
            }
            ImGui::PopStyleColor();
            ImGui::TextColored(theme::colors::TEXT_DISABLED, "  %d data points", pts);
        }
        ImGui::EndChild();
    }
}

// ============================================================================
// Stats bar
// ============================================================================

void EconomicsScreen::render_stats_bar() {
    const auto& cfg = get_source_config(data_source_);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::colors::BG_WIDGET);
    ImGui::BeginChild("##stats_bar", ImVec2(0, 28), ImGuiChildFlags_Borders);

    auto stat = [&](const char* label, const std::string& value, ImVec4 col = theme::colors::TEXT_PRIMARY) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "%s:", label);
        ImGui::SameLine(0, 4);
        ImGui::TextColored(col, "%s", value.c_str());
        ImGui::SameLine(0, 16);
    };

    stat("Latest", format_value(stats_.latest), cfg.color);
    ImVec4 change_col = stats_.change >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
    char change_buf[32];
    std::snprintf(change_buf, sizeof(change_buf), "%+.2f%%", stats_.change);
    stat("Change", change_buf, change_col);
    stat("Min", format_value(stats_.min));
    stat("Max", format_value(stats_.max));
    stat("Avg", format_value(stats_.avg));
    char count_buf[16];
    std::snprintf(count_buf, sizeof(count_buf), "%d", stats_.count);
    stat("Points", count_buf);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Chart (ImPlot line chart)
// ============================================================================

void EconomicsScreen::render_chart() {
    if (data_.data.empty()) return;

    const auto& cfg = get_source_config(data_source_);

    // Title
    ImGui::TextColored(theme::colors::TEXT_PRIMARY, "%s", data_.indicator_name.c_str());
    ImGui::TextColored(theme::colors::TEXT_DIM, "%s  |  Source: %s",
                       data_.country_name.c_str(), data_.source.c_str());
    ImGui::Spacing();

    // Prepare plot data
    int n = static_cast<int>(data_.data.size());
    std::vector<double> xs(n), ys(n);
    for (int i = 0; i < n; i++) {
        xs[i] = static_cast<double>(i);
        ys[i] = data_.data[i].value;
    }

    float chart_h = std::min(300.0f, ImGui::GetContentRegionAvail().y * 0.5f);

    if (ImPlot::BeginPlot("##econ_chart", ImVec2(-1, chart_h))) {
        ImPlot::SetupAxes("", "Value");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, n - 1, ImPlotCond_Always);

        // Custom tick labels: show first, last, and ~5 intermediate dates
        int tick_count = std::min(7, n);
        std::vector<double> tick_pos(tick_count);
        std::vector<const char*> tick_labels(tick_count);
        std::vector<std::string> tick_strings(tick_count); // Keep strings alive
        for (int i = 0; i < tick_count; i++) {
            int idx = (n > 1) ? i * (n - 1) / (tick_count - 1) : 0;
            tick_pos[i] = static_cast<double>(idx);
            tick_strings[i] = data_.data[idx].date;
            tick_labels[i] = tick_strings[i].c_str();
        }
        ImPlot::SetupAxisTicks(ImAxis_X1, tick_pos.data(), tick_count, tick_labels.data());

        ImPlot::PushStyleColor(ImPlotCol_Line, cfg.color);
        ImPlot::PushStyleColor(ImPlotCol_Fill, ImVec4(cfg.color.x, cfg.color.y, cfg.color.z, 0.15f));
        ImPlot::PlotLine("##line", xs.data(), ys.data(), n);
        ImPlot::PlotShaded("##fill", xs.data(), ys.data(), n, -INFINITY);
        ImPlot::PopStyleColor(2);

        ImPlot::EndPlot();
    }
}

// ============================================================================
// Data table
// ============================================================================

void EconomicsScreen::render_data_table() {
    if (data_.data.empty()) return;

    theme::SectionHeader("Data");
    ImGui::SameLine();
    ImGui::TextColored(theme::colors::TEXT_DIM, "(%d rows)", (int)data_.data.size());

    float table_h = std::min(200.0f, ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("##data_table_scroll", ImVec2(-1, table_h));

    if (ImGui::BeginTable("##econ_data", 3,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Show most recent first
        for (int i = static_cast<int>(data_.data.size()) - 1; i >= 0; i--) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(theme::colors::TEXT_DIM, "%d", static_cast<int>(data_.data.size()) - i);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(data_.data[i].date.c_str());
            ImGui::TableNextColumn();
            ImVec4 col = data_.data[i].value >= 0 ? theme::colors::MARKET_GREEN : theme::colors::MARKET_RED;
            ImGui::TextColored(col, "%s", format_value(data_.data[i].value).c_str());
        }

        ImGui::EndTable();
    }
    ImGui::EndChild();
}

// ============================================================================
// Fincept table (raw JSON table display)
// ============================================================================

void EconomicsScreen::render_fincept_table() {
    if (!has_fincept_table_ || fincept_columns_.empty()) return;

    ImGui::TextColored(theme::colors::TEXT_PRIMARY, "%s", data_.indicator_name.c_str());
    ImGui::TextColored(theme::colors::TEXT_DIM, "Source: Fincept Macro API  |  %d records",
                       (int)fincept_table_.size());
    ImGui::Spacing();

    int col_count = static_cast<int>(fincept_columns_.size());
    if (ImGui::BeginTable("##fincept_data", col_count,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
            ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY)) {

        for (const auto& col : fincept_columns_) {
            ImGui::TableSetupColumn(col.c_str());
        }
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (size_t r = 0; r < fincept_table_.size(); r++) {
            ImGui::TableNextRow();
            const auto& row = fincept_table_[r];
            for (const auto& col : fincept_columns_) {
                ImGui::TableNextColumn();
                if (!row.contains(col) || row[col].is_null()) {
                    ImGui::TextColored(theme::colors::TEXT_DISABLED, "--");
                } else if (row[col].is_number()) {
                    double val = row[col].get<double>();
                    ImVec4 c = val < 0 ? theme::colors::MARKET_RED : theme::colors::MARKET_GREEN;
                    ImGui::TextColored(c, "%s", format_value(val).c_str());
                } else if (row[col].is_string()) {
                    ImGui::TextUnformatted(row[col].get<std::string>().c_str());
                } else {
                    ImGui::TextUnformatted(row[col].dump().c_str());
                }
            }
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// Status bar
// ============================================================================

void EconomicsScreen::render_status_bar() {
    const auto& cfg = get_source_config(data_source_);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::colors::BG_PANEL);
    ImGui::BeginChild("##econ_status", ImVec2(0, 20), ImGuiChildFlags_Borders);

    ImGui::TextColored(theme::colors::TEXT_DIM, "SOURCE:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(cfg.color, "%s", cfg.name);
    ImGui::SameLine(0, 16);

    ImGui::TextColored(theme::colors::TEXT_DIM, "COUNTRY:");
    ImGui::SameLine(0, 4);
    ImGui::TextUnformatted(selected_country_.c_str());
    ImGui::SameLine(0, 16);

    ImGui::TextColored(theme::colors::TEXT_DIM, "INDICATOR:");
    ImGui::SameLine(0, 4);
    ImGui::TextUnformatted(selected_indicator_.c_str());
    ImGui::SameLine(0, 16);

    if (has_data_) {
        ImGui::TextColored(theme::colors::TEXT_DIM, "POINTS:");
        ImGui::SameLine(0, 4);
        ImGui::Text("%d", (int)data_.data.size());
        ImGui::SameLine(0, 16);
    }

    // Status dot
    float x = ImGui::GetContentRegionAvail().x - 60;
    if (x > 0) ImGui::SameLine(ImGui::GetCursorPosX() + x);
    ImVec4 dot_col = loading_ ? theme::colors::WARNING :
                     !error_.empty() ? theme::colors::ERROR_RED : theme::colors::SUCCESS;
    ImGui::TextColored(dot_col, "%s", loading_ ? "Loading..." : !error_.empty() ? "Error" : "Ready");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::economics
