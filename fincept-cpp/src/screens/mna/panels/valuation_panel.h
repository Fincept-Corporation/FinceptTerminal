#pragma once
#include "../mna_types.h"
#include "../mna_constants.h"
#include "../mna_data.h"

namespace fincept::mna {

class ValuationPanel {
public:
    void render(MNAData& data);

private:
    // Sub-tab: 0=DCF, 1=LBO Returns, 2=LBO Model, 3=Debt Schedule, 4=LBO Sensitivity, 5=Comps, 6=Precedent
    int sub_tab_ = 0;
    bool inputs_collapsed_ = false;

    // DCF inputs
    double dcf_ebit_ = 500, dcf_tax_ = 0.21, dcf_rfr_ = 0.045, dcf_beta_ = 1.2;
    double dcf_term_growth_ = 0.025, dcf_shares_ = 100, dcf_mcap_ = 5000, dcf_debt_ = 1000;
    double dcf_cod_ = 0.05, dcf_cash_ = 500, dcf_depr_ = 100, dcf_capex_ = 150, dcf_nwc_ = 20;

    // LBO returns (simple)
    double lbo_entry_ = 1000, lbo_exit_ = 1500, lbo_equity_ = 300;
    int lbo_period_ = 5;

    // LBO model
    LBOInputs lbo_model_;

    // Debt schedule
    double ds_senior_ = 300, ds_senior_rate_ = 0.06;
    int ds_senior_term_ = 7;
    double ds_sub_ = 100, ds_sub_rate_ = 0.10;
    int ds_sub_term_ = 8;
    double ds_revolver_ = 50, ds_revolver_rate_ = 0.055;
    double ds_sweep_ = 0.75;

    // Sensitivity
    double sens_revenue_ = 500, sens_margin_ = 0.20, sens_exit_ = 9.0;
    double sens_debt_pct_ = 0.60;
    int sens_period_ = 5;

    // Comps
    char comps_target_[32] = "";
    char comps_tickers_[128] = "AAPL,MSFT,GOOGL";

    // Precedent
    double prec_revenue_ = 500, prec_ebitda_ = 100;

    // Results
    bool has_result_ = false;
    nlohmann::json result_;
    std::string status_;
    double status_time_ = 0;

    void render_sub_tabs();
    void render_dcf(MNAData& data);
    void render_lbo_returns(MNAData& data);
    void render_lbo_model(MNAData& data);
    void render_debt_schedule(MNAData& data);
    void render_lbo_sensitivity(MNAData& data);
    void render_comps(MNAData& data);
    void render_precedent(MNAData& data);
    void render_result_section();
};

} // namespace fincept::mna
