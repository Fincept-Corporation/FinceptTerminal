#pragma once
#include "../mna_types.h"
#include "../mna_constants.h"
#include "../mna_data.h"
#include "storage/database.h"

namespace fincept::mna {

class DealDbPanel {
public:
    void render(MNAData& data);

private:
    bool initialized_ = false;
    int left_tab_ = 0;  // 0=deals, 1=filings
    int selected_deal_ = -1;
    char search_buf_[128] = "";
    bool hide_self_tenders_ = true;
    int scan_days_ = 30;
    bool scanning_ = false;

    std::vector<db::MADealRow> deals_;
    nlohmann::json filings_;

    // Create deal modal
    bool show_create_modal_ = false;
    char cd_target_[64] = "";
    char cd_acquirer_[64] = "";
    double cd_value_ = 0;
    double cd_premium_ = 0;
    double cd_cash_pct_ = 100;
    char cd_industry_[32] = "Technology";
    char cd_status_[32] = "Pending";

    std::string status_;
    double status_time_ = 0;

    void init();
    void load_deals();
    void render_deal_list();
    void render_deal_detail();
    void render_scanner(MNAData& data);
    void render_create_modal();
};

} // namespace fincept::mna
