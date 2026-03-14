#pragma once
#include "../gov_data.h"
#include "../gov_data_constants.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::gov_data {

class FrenchGovProvider {
public:
    void render();

private:
    GovData data_;
    int active_view_ = 0; // 0=geo, 1=datasets, 2=tabular, 3=company

    // Geography
    char geo_query_[128] = "";
    int geo_type_ = 0; // 0=municipalities, 1=departments, 2=regions
    nlohmann::json geo_results_;

    // Datasets
    char dataset_query_[256] = "";
    nlohmann::json dataset_results_;

    // Tabular
    char resource_id_[128] = "";
    nlohmann::json tabular_profile_;
    nlohmann::json tabular_data_;

    // Company
    char company_query_[128] = "";
    nlohmann::json company_results_;

    std::string status_;
    double status_time_ = 0;

    void fetch_geo();
    void fetch_datasets();
    void fetch_tabular_profile();
    void fetch_tabular_data();
    void fetch_company();

    void render_toolbar();
    void render_geo();
    void render_datasets_view();
    void render_tabular();
    void render_company();
    void pickup_result();
};

} // namespace fincept::gov_data
