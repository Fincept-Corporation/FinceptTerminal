#pragma once
#include "../mna_types.h"
#include "../mna_constants.h"
#include "../mna_data.h"

namespace fincept::mna {

class FairnessPanel {
public:
    void render(MNAData& data);

private:
    int sub_tab_ = 0;  // 0=Fairness, 1=Premium, 2=Process
    bool inputs_collapsed_ = false;

    // Fairness inputs
    double fo_offer_ = 50;
    ValuationResult fo_methods_[3] = {
        {"DCF", 45, 40, 55},
        {"Trading Comps", 48, 42, 54},
        {"Precedent Txn", 52, 46, 58},
    };

    // Premium analysis
    double pm_offer_ = 50;
    char pm_prices_[256] = "42,43,44,43,45,44,46,45,47,46,48,47,49,48,50,49,51,50,52,51";

    // Process quality (8 factors, 1-5 scale)
    int pq_scores_[8] = {3,3,3,3,3,3,3,3};

    nlohmann::json result_;
    std::string status_;
    double status_time_ = 0;

    void render_fairness(MNAData& data);
    void render_premium(MNAData& data);
    void render_process(MNAData& data);
};

} // namespace fincept::mna
