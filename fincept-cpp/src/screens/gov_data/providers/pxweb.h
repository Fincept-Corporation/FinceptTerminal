#pragma once
#include "../gov_data.h"
#include "../gov_data_constants.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::gov_data {

class PxWebProvider {
public:
    void render();

private:
    GovData data_;
    int active_view_ = 0; // 0=browse, 1=metadata, 2=data

    // Browse state — tree navigation
    struct NodeEntry {
        std::string id;
        std::string text;
        char type; // 'l' = folder, 't' = table
        std::string updated;
    };
    std::vector<NodeEntry> nodes_;
    std::vector<std::string> path_stack_; // navigation path
    std::string current_path_;

    // Metadata
    std::string selected_table_;
    std::string table_label_;
    nlohmann::json variables_; // array of variable objects

    // Data
    nlohmann::json stat_data_;

    std::string status_;
    double status_time_ = 0;
    bool initial_fetch_ = false;

    void fetch_nodes(const std::string& path);
    void navigate_into(const NodeEntry& node);
    void navigate_back();
    void fetch_data();

    void render_toolbar();
    void render_browse();
    void render_metadata();
    void render_data_view();
    void pickup_result();

    void parse_nodes(const nlohmann::json& j);
    void parse_variables(const nlohmann::json& j);
};

} // namespace fincept::gov_data
