#pragma once
#include "../gov_data.h"
#include "../gov_data_constants.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::gov_data {

// Historical file record (HK-specific)
struct HkHistoricalFile {
    std::string name;
    std::string format;
    std::string category;
    std::string date; // YYYYMMDD
    std::string url;
    int64_t size = 0;
};

class DataGovHkProvider {
public:
    void render();

private:
    GovData data_;
    int active_view_ = 0; // 0=categories, 1=datasets, 2=resources, 3=historical, 4=search

    std::vector<OrganizationRecord> categories_; // reuse OrganizationRecord for categories
    std::string selected_category_;
    std::vector<DatasetRecord> datasets_;
    std::string selected_dataset_;
    std::vector<ResourceRecord> resources_;

    // Historical
    char hist_start_[16] = "";
    char hist_end_[16] = "";
    char hist_category_[64] = "";
    char hist_format_[32] = "";
    std::vector<HkHistoricalFile> historical_;

    // Search
    char search_buf_[256] = "";
    std::vector<DatasetRecord> search_results_;

    std::string status_;
    double status_time_ = 0;
    bool initial_fetch_ = false;

    void fetch_categories();
    void fetch_datasets(const std::string& category_id);
    void fetch_resources(const std::string& dataset_id);
    void fetch_historical();
    void fetch_search();

    void render_toolbar();
    void render_categories();
    void render_datasets(const std::vector<DatasetRecord>& ds);
    void render_resources();
    void render_historical();
    void pickup_result();

    void parse_categories(const nlohmann::json& j);
    void parse_datasets(const nlohmann::json& j, std::vector<DatasetRecord>& out);
    void parse_resources(const nlohmann::json& j);
    void parse_historical(const nlohmann::json& j);
};

} // namespace fincept::gov_data
