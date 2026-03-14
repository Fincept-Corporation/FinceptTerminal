#pragma once
#include "../mna_types.h"
#include "../mna_constants.h"
#include "../mna_data.h"

namespace fincept::mna {

class MergerPanel {
public:
    void render(MNAData& data);

private:
    // Sub-tabs: 0=Accretion/Dilution, 1=Synergies, 2=Deal Structure, 3=Pro Forma
    int sub_tab_ = 0;
    int structure_sub_ = 0;  // Payment/Earnout/Exchange/Collar/CVR
    int proforma_sub_ = 0;   // ProForma/Sources&Uses/Contribution
    bool inputs_collapsed_ = false;

    // Accretion/Dilution
    MergerModelInputs merger_;

    // Synergies
    int synergy_type_ = 0;  // 0=revenue, 1=cost
    double syn_base_revenue_ = 5000, syn_cross_sell_ = 0.15;
    int syn_years_ = 5;

    // Payment structure
    double str_price_ = 1000, str_cash_pct_ = 60, str_acq_cash_ = 500, str_debt_cap_ = 400;

    // Earnout
    double eo_max_ = 200, eo_rev_target_ = 500, eo_ebitda_target_ = 100;
    int eo_period_ = 3;
    double eo_base_rev_ = 400, eo_rev_growth_ = 0.10, eo_base_ebitda_ = 80, eo_ebitda_growth_ = 0.08;

    // Exchange ratio
    double ex_acq_price_ = 50, ex_tgt_price_ = 30, ex_premium_ = 0.25;

    // Collar
    double col_base_ratio_ = 0.8, col_floor_ = 45, col_cap_ = 55;
    double col_tgt_shares_ = 50;

    // CVR
    int cvr_type_ = 0;  // 0=milestone, 1=earnout, 2=litigation
    double cvr_prob_ = 0.60, cvr_payout_ = 5.00, cvr_discount_ = 0.12;

    // Pro Forma
    double pf_acq_rev_ = 10000, pf_acq_ebitda_ = 2000, pf_acq_ni_ = 1000;
    double pf_acq_shares_ = 500;
    double pf_tgt_rev_ = 3000, pf_tgt_ebitda_ = 600, pf_tgt_ni_ = 300;
    double pf_tgt_ev_ = 4800;
    int pf_year_ = 1;

    // Sources & Uses
    double su_price_ = 5000, su_tgt_debt_ = 500, su_fees_ = 100;

    // Contribution
    double cont_ownership_ = 60;

    nlohmann::json result_;
    std::string status_;
    double status_time_ = 0;

    void render_accretion(MNAData& data);
    void render_synergies(MNAData& data);
    void render_structure(MNAData& data);
    void render_proforma(MNAData& data);
};

} // namespace fincept::mna
