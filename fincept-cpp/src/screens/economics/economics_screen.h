#pragma once
// Economics Screen — Multi-source economic data visualization
// Port of EconomicsTab.tsx

#include "economics_types.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <map>

namespace fincept::economics {

class EconomicsScreen {
public:
    void render();

private:
    // Data source selection
    DataSource data_source_ = DataSource::WorldBank;
    std::string selected_country_ = "USA";
    std::string selected_indicator_ = "NY.GDP.MKTP.CD";

    // Search filters
    char country_search_[128] = {};
    char indicator_search_[128] = {};

    // Date range
    int date_preset_idx_ = 2; // 0=1Y, 1=3Y, 2=5Y, 3=10Y, 4=ALL
    char start_date_[16] = "2021-01-01";
    char end_date_[16] = "2026-01-01";

    // API Keys (stored in credentials DB)
    std::map<std::string, std::string> api_keys_; // key_name -> value
    bool show_api_key_panel_ = false;
    char api_key_input_[256] = {};
    bool api_keys_loaded_ = false;

    // Data state
    IndicatorData data_;
    bool has_data_ = false;
    ChartStats stats_;
    bool loading_ = false;
    std::string error_;

    // Fincept table data (for non-chart endpoints)
    nlohmann::json fincept_table_;
    std::vector<std::string> fincept_columns_;
    bool has_fincept_table_ = false;

    // Fincept CEIC drill-down
    int fincept_ceic_country_idx_ = 0;
    std::string fincept_ceic_indicator_;
    std::vector<std::pair<std::string, int>> fincept_ceic_list_; // indicator -> data_points
    bool fincept_ceic_loading_ = false;
    char fincept_ceic_search_[128] = {};

    // Methods
    void load_api_keys();
    void save_api_key(const std::string& key_name, const std::string& key_value);
    std::string get_api_key(const std::string& key_name) const;

    void set_data_source(DataSource src);
    void update_date_range();
    std::string get_country_code() const;

    void fetch_data();
    void fetch_fincept_ceic_indicators();
    void parse_response(const std::string& raw_json);

    void compute_stats();

    // UI sections
    void render_header();
    void render_api_key_panel();
    void render_country_selector();
    void render_indicator_selector();
    void render_fincept_selector();
    void render_stats_bar();
    void render_chart();
    void render_data_table();
    void render_fincept_table();
    void render_status_bar();
};

} // namespace fincept::economics
