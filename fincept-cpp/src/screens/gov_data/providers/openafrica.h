#pragma once
#include "../gov_data.h"
#include "../gov_data_constants.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::gov_data {

class OpenAfricaProvider {
public:
    void render();

private:
    GovData data_;
    int active_view_ = 0; // 0=orgs, 1=datasets, 2=resources, 3=search, 4=recent

    std::vector<OrganizationRecord> organizations_;
    std::string selected_org_;
    std::vector<DatasetRecord> datasets_;
    int datasets_total_ = 0;
    std::string selected_dataset_;
    std::vector<ResourceRecord> resources_;

    char search_buf_[256] = "";
    std::vector<DatasetRecord> search_results_;
    int search_total_ = 0;
    std::vector<DatasetRecord> recent_;

    std::string status_;
    double status_time_ = 0;
    bool initial_fetch_ = false;

    void fetch_organizations();
    void fetch_datasets(const std::string& org_name);
    void fetch_resources(const std::string& dataset_id);
    void fetch_search();
    void fetch_recent();

    void render_toolbar();
    void render_organizations();
    void render_datasets(const std::vector<DatasetRecord>& ds, int total);
    void render_resources();
    void pickup_result();

    void parse_organizations(const nlohmann::json& j);
    void parse_datasets(const nlohmann::json& j, std::vector<DatasetRecord>& out, int& total);
    void parse_resources(const nlohmann::json& j);
};

} // namespace fincept::gov_data
