#pragma once
#include "../gov_data.h"
#include "../gov_data_constants.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::gov_data {

class USCongressProvider {
public:
    void render();

private:
    GovData data_;
    int active_view_ = 0; // 0=bills, 1=bill-detail, 2=summary

    // Bills state
    int congress_number_ = 119;
    int bill_type_idx_ = 0;
    int bills_offset_ = 0;
    nlohmann::json bills_result_;
    int bills_total_ = 0;

    // Bill detail
    std::string selected_bill_path_;
    nlohmann::json bill_detail_;

    // Summary
    nlohmann::json summary_result_;

    // Date range
    char start_date_[16] = "";
    char end_date_[16] = "";

    std::string status_;
    double status_time_ = 0;

    void init_dates();
    void fetch_bills();
    void fetch_bill_detail(const std::string& bill_path);
    void fetch_summary();

    void render_toolbar();
    void render_bills();
    void render_bill_detail();
    void render_summary_view();
    void pickup_result();
};

} // namespace fincept::gov_data
