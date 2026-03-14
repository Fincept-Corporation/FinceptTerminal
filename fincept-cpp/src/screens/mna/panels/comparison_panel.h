#pragma once
#include "../mna_types.h"
#include "../mna_constants.h"
#include "../mna_data.h"
#include "storage/database.h"

namespace fincept::mna {

class ComparisonPanel {
public:
    void render(MNAData& data);

private:
    // 0=Compare, 1=Rank, 2=Benchmark, 3=Payment, 4=Industry
    int sub_tab_ = 0;
    bool inputs_collapsed_ = false;
    int rank_criteria_ = 0;  // maps to DealSortCriteria
    int target_deal_idx_ = 0;

    // Editable deals for comparison
    struct CompDeal {
        char target[64] = "";
        char acquirer[64] = "";
        double value = 0;
        double premium = 0;
        double ev_revenue = 0;
        double ev_ebitda = 0;
        double synergies = 0;
        double cash_pct = 100;
        double stock_pct = 0;
        char industry[32] = "Technology";
        char status[32] = "Completed";
    };
    std::vector<CompDeal> deals_;
    bool deals_initialized_ = false;

    nlohmann::json result_;
    std::string status_;
    double status_time_ = 0;

    void init_default_deals();
    std::vector<MADeal> build_deal_list() const;
    void render_deal_editor();
};

} // namespace fincept::mna
