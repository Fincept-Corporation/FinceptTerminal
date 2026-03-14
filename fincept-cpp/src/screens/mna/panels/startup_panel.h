#pragma once
#include "../mna_types.h"
#include "../mna_constants.h"
#include "../mna_data.h"

namespace fincept::mna {

class StartupPanel {
public:
    void render(MNAData& data);

private:
    // 0=Berkus, 1=Scorecard, 2=VC, 3=First Chicago, 4=Risk Factor, 5=Comprehensive
    int method_ = 0;
    bool inputs_collapsed_ = false;

    // Berkus (0-100 per factor, mapped to $0-$500K)
    double berkus_idea_ = 70, berkus_proto_ = 50, berkus_team_ = 80;
    double berkus_strategic_ = 40, berkus_sales_ = 30;

    // Scorecard
    int sc_stage_ = 0;  // 0=seed, 1=pre-seed, 2=series-a
    int sc_region_ = 0; // 0=us, 1=europe, 2=asia
    double sc_team_ = 1.2, sc_market_ = 1.1, sc_product_ = 1.0;
    double sc_competitive_ = 0.9, sc_marketing_ = 1.0, sc_funding_ = 0.8, sc_other_ = 1.0;

    // VC Method
    double vc_exit_metric_ = 50, vc_exit_multiple_ = 8;
    int vc_years_ = 5;
    double vc_investment_ = 5;
    int vc_stage_ = 0;

    // First Chicago (3 scenarios)
    StartupScenario scenarios_[3];

    // Risk Factor
    double rf_base_ = 3;  // $3M
    int rf_scores_[12] = {};  // -2 to +2

    nlohmann::json result_;
    std::string status_;
    double status_time_ = 0;

    void init_scenarios();
    void render_berkus(MNAData& data);
    void render_scorecard(MNAData& data);
    void render_vc(MNAData& data);
    void render_first_chicago(MNAData& data);
    void render_risk_factor(MNAData& data);
    void render_comprehensive(MNAData& data);
};

} // namespace fincept::mna
