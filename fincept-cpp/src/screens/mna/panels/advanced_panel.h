#pragma once
#include "../mna_types.h"
#include "../mna_constants.h"
#include "../mna_data.h"

namespace fincept::mna {

class AdvancedPanel {
public:
    void render(MNAData& data);

private:
    int sub_tab_ = 0;  // 0=Monte Carlo, 1=Regression
    bool inputs_collapsed_ = false;

    // Monte Carlo
    double mc_base_ = 1000, mc_growth_mean_ = 15, mc_growth_std_ = 5;
    double mc_margin_mean_ = 25, mc_margin_std_ = 3, mc_discount_ = 10;
    int mc_sims_ = 10000;

    // Regression
    int reg_type_ = 0;  // 0=ols, 1=multiple
    double reg_subject_rev_ = 1000, reg_subject_ebitda_ = 300, reg_subject_growth_ = 14;

    nlohmann::json result_;
    std::string status_;
    double status_time_ = 0;

    void render_monte_carlo(MNAData& data);
    void render_regression(MNAData& data);
};

} // namespace fincept::mna
