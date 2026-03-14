#pragma once
// Data Sources Screen — Bloomberg Terminal-inspired connector gallery
// Dense table layout with category sidebar, command bar, status ticker

#include "data_sources_types.h"
#include "adapters/adapter_registry.h"
#include "storage/database.h"
#include <string>
#include <vector>
#include <map>
#include <future>
#include <optional>

namespace fincept::data_sources {

class DataSourcesScreen {
public:
    void render();

private:
    void init();

    // Bloomberg-style panels
    void render_command_bar(float w, float h);
    void render_sidebar(float w, float h);
    void render_gallery_table(float w, float h);
    void render_connections_table(float w, float h);
    void render_status_bar(float w, float h);
    void render_config_modal();

    // Helpers
    void render_field(const FieldDef& field, const std::string& ds_id);

    // Actions
    void open_config(const DataSourceDef& def);
    void save_connection();
    void delete_connection(const std::string& id);
    void load_connections();

    // State
    bool initialized_ = false;
    int view_ = 0;  // 0 = gallery, 1 = connections
    char search_buf_[128] = {};
    int selected_category_ = -1;  // -1 = all

    // Config modal state
    bool show_modal_ = false;
    const DataSourceDef* modal_def_ = nullptr;
    std::string editing_id_;
    std::map<std::string, std::string> form_data_;
    std::string save_error_;

    // Connection testing state
    bool testing_ = false;
    std::future<adapters::TestResult> test_future_;
    std::optional<adapters::TestResult> test_result_;

    // Per-connection test
    std::string testing_conn_id_;
    std::future<adapters::TestResult> conn_test_future_;
    std::map<std::string, adapters::TestResult> conn_test_results_;

    // Data
    std::vector<DataSourceDef> all_defs_;
    std::vector<db::DataSource> connections_;
};

} // namespace fincept::data_sources
