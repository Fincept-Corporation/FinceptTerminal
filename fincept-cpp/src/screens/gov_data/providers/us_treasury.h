#pragma once
#include "../gov_data.h"
#include "../gov_data_constants.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::gov_data {

class USTreasuryProvider {
public:
    void render();

private:
    GovData data_;
    int active_endpoint_ = 0; // 0=prices, 1=auctions, 2=summary

    // Prices state
    int security_type_idx_ = 0;
    char cusip_filter_[32] = "";
    nlohmann::json prices_result_;

    // Auctions state
    int auction_type_idx_ = 0;
    int page_size_ = 50;
    int page_num_ = 1;
    nlohmann::json auctions_result_;

    // Summary state
    nlohmann::json summary_result_;

    // Date range
    char start_date_[16] = "";
    char end_date_[16] = "";

    std::string status_;
    double status_time_ = 0;

    void init_dates();
    void fetch_prices();
    void fetch_auctions();
    void fetch_summary();

    void render_toolbar();
    void render_prices();
    void render_auctions();
    void render_summary();
    void pickup_result();
};

} // namespace fincept::gov_data
