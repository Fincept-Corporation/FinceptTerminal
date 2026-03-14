#pragma once
#include "../gov_data.h"
#include "../gov_data_constants.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::gov_data {

class SpainDataProvider {
public:
    void render();

private:
    GovData data_;
    int active_view_ = 0; // 0=catalogues, 1=datasets, 2=resources, 3=search

    std::vector<std::pair<std::string, std::string>> catalogues_; // id, uri
    std::string selected_catalogue_;
    std::vector<DatasetRecord> datasets_;
    std::string selected_dataset_;
    std::vector<ResourceRecord> resources_;
    int page_ = 1;

    char search_buf_[256] = "";
    std::vector<DatasetRecord> search_results_;

    std::string status_;
    double status_time_ = 0;
    bool initial_fetch_ = false;

    void fetch_catalogues();
    void fetch_datasets(const std::string& catalogue_id = "");
    void fetch_resources(const std::string& dataset_id);
    void fetch_search();

    void render_toolbar();
    void render_catalogues();
    void render_datasets(const std::vector<DatasetRecord>& ds);
    void render_resources();
    void pickup_result();

    void parse_catalogues(const nlohmann::json& j);
    void parse_datasets(const nlohmann::json& j, std::vector<DatasetRecord>& out);
    void parse_resources(const nlohmann::json& j);
};

} // namespace fincept::gov_data
