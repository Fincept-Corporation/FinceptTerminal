#include "merger_panel.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nlohmann/json.hpp>

namespace fincept::mna {

using json = nlohmann::json;

// =============================================================================
// Main render
// =============================================================================
void MergerPanel::render(MNAData& data) {
    const char* tabs[] = {"Accretion/Dilution", "Synergies", "Deal Structure", "Pro Forma"};
    for (int i = 0; i < 4; i++) {
        if (ColoredButton(tabs[i], ma_colors::MERGER, sub_tab_ == i)) {
            sub_tab_ = i;
            result_ = json();
            status_.clear();
        }
        if (i < 3) ImGui::SameLine(0, 2);
    }
    ImGui::Separator();
    ImGui::Spacing();

    switch (sub_tab_) {
        case 0: render_accretion(data);  break;
        case 1: render_synergies(data);  break;
        case 2: render_structure(data);  break;
        case 3: render_proforma(data);   break;
        default: break;
    }

    ImGui::Spacing();
    StatusMessage(status_, status_time_);
}

// =============================================================================
// Accretion / Dilution Analysis
// =============================================================================
void MergerPanel::render_accretion(MNAData& data) {
    using namespace theme::colors;

    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("MERGER MODEL INPUTS", &open, ma_colors::MERGER)) {
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::MERGER, "Acquirer");
        ImGui::Spacing();
        InputRow("Revenue ($M)",     &merger_.acq_revenue,     "%.0f");
        InputRow("EBITDA ($M)",      &merger_.acq_ebitda,      "%.0f");
        InputRow("EBIT ($M)",        &merger_.acq_ebit,        "%.0f");
        InputRow("Net Income ($M)",  &merger_.acq_net_income,  "%.0f");
        InputRow("Shares Out (M)",   &merger_.acq_shares,      "%.1f");
        InputRow("EPS",              &merger_.acq_eps,          "%.2f");

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::MERGER, "Target");
        ImGui::Spacing();
        InputRow("Revenue ($M)",     &merger_.tgt_revenue,     "%.0f");
        InputRow("EBITDA ($M)",      &merger_.tgt_ebitda,      "%.0f");
        InputRow("EBIT ($M)",        &merger_.tgt_ebit,        "%.0f");
        InputRow("Net Income ($M)",  &merger_.tgt_net_income,  "%.0f");

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::MERGER, "Deal Terms");
        ImGui::Spacing();
        InputRow("Purchase Price ($M)", &merger_.purchase_price, "%.0f");
        InputRow("Cash %",           &merger_.cash_pct,         "%.0f");
        InputRow("Stock %",          &merger_.stock_pct,        "%.0f");
        InputRow("Cost Synergies ($M)", &merger_.cost_synergies, "%.0f");
        InputRow("Rev Synergies ($M)",  &merger_.revenue_synergies, "%.0f");
        InputRow("Integration Costs ($M)", &merger_.integration_costs, "%.0f");
        InputRow("Tax Rate",         &merger_.tax_rate,          "%.4f");
    }
    inputs_collapsed_ = !open;

    ImGui::Spacing();

    if (RunButton("RUN ACCRETION/DILUTION", data.is_loading(), ma_colors::MERGER)) {
        json acquirer = {
            {"revenue",    merger_.acq_revenue},
            {"ebitda",     merger_.acq_ebitda},
            {"ebit",       merger_.acq_ebit},
            {"net_income", merger_.acq_net_income},
            {"shares_outstanding", merger_.acq_shares},
            {"eps",        merger_.acq_eps}
        };
        json target = {
            {"revenue",    merger_.tgt_revenue},
            {"ebitda",     merger_.tgt_ebitda},
            {"ebit",       merger_.tgt_ebit},
            {"net_income", merger_.tgt_net_income}
        };
        json deal_terms = {
            {"purchase_price",  merger_.purchase_price},
            {"cash_pct",        merger_.cash_pct},
            {"stock_pct",       merger_.stock_pct},
            {"cost_synergies",  merger_.cost_synergies},
            {"revenue_synergies", merger_.revenue_synergies},
            {"integration_costs", merger_.integration_costs},
            {"tax_rate",        merger_.tax_rate}
        };

        data.run_analysis("merger_models/merger_model.py", {
            "build",
            acquirer.dump(),
            target.dump(),
            deal_terms.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Accretion/Dilution analysis complete";
        status_time_ = ImGui::GetTime();
        data.clear();
    }
    if (!data.is_loading() && !data.error().empty()) {
        status_ = "Error: " + data.error();
        status_time_ = ImGui::GetTime();
        data.clear();
    }

    // Display results
    if (!result_.is_null()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::MERGER, "ACCRETION / DILUTION RESULTS");
        ImGui::Spacing();

        if (result_.contains("accretion_dilution")) {
            auto& ad = result_["accretion_dilution"];
            if (ad.contains("standalone_eps")) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "$%.2f", ad["standalone_eps"].get<double>());
                DataRow("Standalone EPS", buf, TEXT_PRIMARY);
            }
            if (ad.contains("pro_forma_eps")) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "$%.2f", ad["pro_forma_eps"].get<double>());
                DataRow("Pro Forma EPS", buf, TEXT_PRIMARY);
            }
            if (ad.contains("eps_change_pct")) {
                double chg = ad["eps_change_pct"].get<double>();
                ImVec4 col = (chg >= 0) ? SUCCESS : ImVec4(0.96f, 0.30f, 0.30f, 1.0f);
                DataRow("EPS Impact", format_percent(chg).c_str(), col);
                DataRow("Result", (chg >= 0) ? "ACCRETIVE" : "DILUTIVE", col);
            }
        }

        if (result_.contains("deal_summary")) {
            ImGui::Spacing();
            ImGui::TextColored(TEXT_DIM, "Deal Summary");
            auto& ds = result_["deal_summary"];
            if (ds.contains("purchase_price"))
                DataRow("Purchase Price", format_currency(ds["purchase_price"].get<double>()).c_str(), TEXT_SECONDARY);
            if (ds.contains("implied_ev_ebitda")) {
                DataRow("Implied EV/EBITDA", format_multiple(ds["implied_ev_ebitda"].get<double>()).c_str(), TEXT_SECONDARY);
            }
        }
    }
}

// =============================================================================
// Synergies Analysis
// =============================================================================
void MergerPanel::render_synergies(MNAData& data) {
    using namespace theme::colors;

    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("SYNERGY INPUTS", &open, ma_colors::MERGER)) {
        ImGui::Spacing();
        const char* types[] = {"Revenue Synergies", "Cost Synergies"};
        for (int i = 0; i < 2; i++) {
            if (ColoredButton(types[i], ma_colors::MERGER, synergy_type_ == i))
                synergy_type_ = i;
            if (i == 0) ImGui::SameLine(0, 4);
        }
        ImGui::Spacing();

        InputRow("Base Revenue ($M)", &syn_base_revenue_, "%.0f");
        InputRow("Cross-Sell Uplift", &syn_cross_sell_,   "%.4f");
        InputRowInt("Realization Years", &syn_years_);
    }
    inputs_collapsed_ = !open;

    ImGui::Spacing();

    if (RunButton("VALUE SYNERGIES", data.is_loading(), ma_colors::MERGER)) {
        // Build synergy projections
        json rev_syn = json::array();
        json cost_syn = json::array();
        json integ_costs = json::array();

        for (int y = 0; y < 10; y++) {
            double ramp = (y < syn_years_) ? (double)(y + 1) / syn_years_ : 1.0;
            double rev = syn_base_revenue_ * syn_cross_sell_ * ramp;
            double cost = syn_base_revenue_ * 0.05 * ramp;
            double integ = (y < 2) ? syn_base_revenue_ * 0.02 : 0;
            rev_syn.push_back(rev);
            cost_syn.push_back(cost);
            integ_costs.push_back(integ);
        }

        data.run_analysis("synergies/synergy_valuation.py", {
            "dcf",
            rev_syn.dump(),
            cost_syn.dump(),
            integ_costs.dump(),
            "0.10"
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Synergy valuation complete";
        status_time_ = ImGui::GetTime();
        data.clear();
    }
    if (!data.is_loading() && !data.error().empty()) {
        status_ = "Error: " + data.error();
        status_time_ = ImGui::GetTime();
        data.clear();
    }

    // Results
    if (!result_.is_null()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::MERGER, "SYNERGY VALUATION");
        ImGui::Spacing();

        if (result_.contains("total_synergy_pv"))
            DataRow("Total Synergy PV", format_currency(result_["total_synergy_pv"].get<double>()).c_str(), SUCCESS);
        if (result_.contains("revenue_synergy_pv"))
            DataRow("Revenue Synergy PV", format_currency(result_["revenue_synergy_pv"].get<double>()).c_str(), TEXT_PRIMARY);
        if (result_.contains("cost_synergy_pv"))
            DataRow("Cost Synergy PV", format_currency(result_["cost_synergy_pv"].get<double>()).c_str(), TEXT_PRIMARY);
        if (result_.contains("integration_cost_pv"))
            DataRow("Integration Cost PV", format_currency(result_["integration_cost_pv"].get<double>()).c_str(),
                     ImVec4(0.96f, 0.30f, 0.30f, 1.0f));
        if (result_.contains("net_synergy_pv"))
            DataRow("Net Synergy PV", format_currency(result_["net_synergy_pv"].get<double>()).c_str(), SUCCESS);
    }
}

// =============================================================================
// Deal Structure (5 sub-tabs: Payment, Earnout, Exchange, Collar, CVR)
// =============================================================================
void MergerPanel::render_structure(MNAData& data) {
    using namespace theme::colors;

    const char* st_tabs[] = {"Payment", "Earnout", "Exchange", "Collar", "CVR"};
    for (int i = 0; i < 5; i++) {
        if (ColoredButton(st_tabs[i], ma_colors::MERGER, structure_sub_ == i)) {
            structure_sub_ = i;
            result_ = json();
        }
        if (i < 4) ImGui::SameLine(0, 2);
    }
    ImGui::Spacing();

    bool open = !inputs_collapsed_;

    switch (structure_sub_) {
    case 0: { // Payment Structure
        if (CollapsibleHeader("PAYMENT INPUTS", &open, ma_colors::MERGER)) {
            ImGui::Spacing();
            InputRow("Purchase Price ($M)", &str_price_,    "%.0f");
            InputRow("Cash %",             &str_cash_pct_,  "%.0f");
            InputRow("Acquirer Cash ($M)",  &str_acq_cash_,  "%.0f");
            InputRow("Debt Capacity ($M)",  &str_debt_cap_,  "%.0f");
        }
        inputs_collapsed_ = !open;
        ImGui::Spacing();

        if (RunButton("ANALYZE PAYMENT", data.is_loading(), ma_colors::MERGER)) {
            json params = {
                {"purchase_price",   str_price_},
                {"cash_pct",         str_cash_pct_},
                {"acquirer_cash",    str_acq_cash_},
                {"debt_capacity",    str_debt_cap_}
            };
            data.run_analysis("deal_structure/payment_structure.py", {
                "payment", params.dump()
            });
        }
    } break;

    case 1: { // Earnout
        if (CollapsibleHeader("EARNOUT INPUTS", &open, ma_colors::MERGER)) {
            ImGui::Spacing();
            InputRow("Max Earnout ($M)",    &eo_max_,          "%.0f");
            InputRow("Rev Target ($M)",     &eo_rev_target_,   "%.0f");
            InputRow("EBITDA Target ($M)",  &eo_ebitda_target_, "%.0f");
            InputRowInt("Earnout Period",   &eo_period_);
            ImGui::Spacing();
            ImGui::TextColored(TEXT_DIM, "Financial Projections");
            InputRow("Base Revenue ($M)",   &eo_base_rev_,     "%.0f");
            InputRow("Rev Growth Rate",     &eo_rev_growth_,   "%.4f");
            InputRow("Base EBITDA ($M)",    &eo_base_ebitda_,  "%.0f");
            InputRow("EBITDA Growth Rate",  &eo_ebitda_growth_,"%.4f");
        }
        inputs_collapsed_ = !open;
        ImGui::Spacing();

        if (RunButton("CALCULATE EARNOUT", data.is_loading(), ma_colors::MERGER)) {
            json params = {
                {"max_earnout",    eo_max_},
                {"tranches", json::array({
                    {{"metric", "revenue"}, {"target", eo_rev_target_}, {"payout", eo_max_ * 0.5}},
                    {{"metric", "ebitda"}, {"target", eo_ebitda_target_}, {"payout", eo_max_ * 0.5}}
                })},
                {"earnout_period", eo_period_}
            };
            json projections = json::array();
            double rev = eo_base_rev_, ebitda = eo_base_ebitda_;
            for (int y = 1; y <= eo_period_; y++) {
                rev *= (1.0 + eo_rev_growth_);
                ebitda *= (1.0 + eo_ebitda_growth_);
                projections.push_back({{"year", y}, {"revenue", rev}, {"ebitda", ebitda}});
            }
            data.run_analysis("deal_structure/earnout_calculator.py", {
                "earnout", params.dump(), projections.dump()
            });
        }
    } break;

    case 2: { // Exchange Ratio
        if (CollapsibleHeader("EXCHANGE RATIO INPUTS", &open, ma_colors::MERGER)) {
            ImGui::Spacing();
            InputRow("Acquirer Price", &ex_acq_price_, "%.2f");
            InputRow("Target Price",   &ex_tgt_price_, "%.2f");
            InputRow("Offer Premium",  &ex_premium_,   "%.4f");
        }
        inputs_collapsed_ = !open;
        ImGui::Spacing();

        if (RunButton("CALCULATE EXCHANGE", data.is_loading(), ma_colors::MERGER)) {
            data.run_analysis("deal_structure/exchange_ratio.py", {
                "exchange_ratio",
                std::to_string(ex_acq_price_),
                std::to_string(ex_tgt_price_),
                std::to_string(ex_premium_)
            });
        }
    } break;

    case 3: { // Collar
        if (CollapsibleHeader("COLLAR INPUTS", &open, ma_colors::MERGER)) {
            ImGui::Spacing();
            InputRow("Base Ratio",     &col_base_ratio_, "%.4f");
            InputRow("Floor Price",    &col_floor_,      "%.2f");
            InputRow("Cap Price",      &col_cap_,        "%.2f");
            InputRow("Target Shares (M)", &col_tgt_shares_, "%.1f");
        }
        inputs_collapsed_ = !open;
        ImGui::Spacing();

        if (RunButton("ANALYZE COLLAR", data.is_loading(), ma_colors::MERGER)) {
            json params = {
                {"base_exchange_ratio", col_base_ratio_},
                {"floor_price",         col_floor_},
                {"cap_price",           col_cap_},
                {"target_shares",       col_tgt_shares_ * 1e6}
            };
            data.run_analysis("deal_structure/collar_mechanisms.py", {
                "collar", params.dump()
            });
        }
    } break;

    case 4: { // CVR
        if (CollapsibleHeader("CVR INPUTS", &open, ma_colors::MERGER)) {
            ImGui::Spacing();
            const char* cvr_types[] = {"Milestone", "Earnout", "Litigation"};
            for (int i = 0; i < 3; i++) {
                if (ColoredButton(cvr_types[i], ma_colors::MERGER, cvr_type_ == i))
                    cvr_type_ = i;
                if (i < 2) ImGui::SameLine(0, 4);
            }
            ImGui::Spacing();
            InputRow("Probability",    &cvr_prob_,    "%.4f");
            InputRow("Payout ($/share)", &cvr_payout_, "%.2f");
            InputRow("Discount Rate",  &cvr_discount_, "%.4f");
        }
        inputs_collapsed_ = !open;
        ImGui::Spacing();

        if (RunButton("VALUE CVR", data.is_loading(), ma_colors::MERGER)) {
            const char* type_names[] = {"milestone", "earnout", "litigation"};
            json params = {
                {"cvr_type",      type_names[cvr_type_]},
                {"probability",   cvr_prob_},
                {"max_payout",    cvr_payout_},
                {"discount_rate", cvr_discount_}
            };
            data.run_analysis("deal_structure/cvr_valuation.py", {
                "cvr", params.dump()
            });
        }
    } break;
    }

    // Shared result pickup for all structure sub-tabs
    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Analysis complete";
        status_time_ = ImGui::GetTime();
        data.clear();
    }
    if (!data.is_loading() && !data.error().empty()) {
        status_ = "Error: " + data.error();
        status_time_ = ImGui::GetTime();
        data.clear();
    }

    // Generic result display
    if (!result_.is_null()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::MERGER, "RESULTS");
        ImGui::Spacing();

        // Iterate top-level keys and display values
        for (auto& [key, val] : result_.items()) {
            if (val.is_number()) {
                bool is_pct = key.find("pct") != std::string::npos || key.find("ratio") != std::string::npos
                           || key.find("rate") != std::string::npos || key.find("premium") != std::string::npos;
                bool is_currency = key.find("value") != std::string::npos || key.find("price") != std::string::npos
                                || key.find("cost") != std::string::npos || key.find("payout") != std::string::npos;

                if (is_pct)
                    DataRow(key.c_str(), format_percent(val.get<double>()).c_str(), TEXT_PRIMARY);
                else if (is_currency)
                    DataRow(key.c_str(), format_currency(val.get<double>()).c_str(), TEXT_PRIMARY);
                else {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "%.2f", val.get<double>());
                    DataRow(key.c_str(), buf, TEXT_PRIMARY);
                }
            } else if (val.is_string()) {
                DataRow(key.c_str(), val.get<std::string>().c_str(), TEXT_SECONDARY);
            }
        }
    }
}

// =============================================================================
// Pro Forma (3 sub-tabs: Pro Forma, Sources & Uses, Contribution)
// =============================================================================
void MergerPanel::render_proforma(MNAData& data) {
    using namespace theme::colors;

    const char* pf_tabs[] = {"Pro Forma", "Sources & Uses", "Contribution"};
    for (int i = 0; i < 3; i++) {
        if (ColoredButton(pf_tabs[i], ma_colors::MERGER, proforma_sub_ == i)) {
            proforma_sub_ = i;
            result_ = json();
        }
        if (i < 2) ImGui::SameLine(0, 2);
    }
    ImGui::Spacing();

    bool open = !inputs_collapsed_;

    switch (proforma_sub_) {
    case 0: { // Pro Forma Financials
        if (CollapsibleHeader("PRO FORMA INPUTS", &open, ma_colors::MERGER)) {
            ImGui::Spacing();
            ImGui::TextColored(ma_colors::MERGER, "Acquirer");
            InputRow("Revenue ($M)",     &pf_acq_rev_,    "%.0f");
            InputRow("EBITDA ($M)",      &pf_acq_ebitda_,  "%.0f");
            InputRow("Net Income ($M)",  &pf_acq_ni_,      "%.0f");
            InputRow("Shares (M)",       &pf_acq_shares_,  "%.1f");
            ImGui::Spacing();
            ImGui::TextColored(ma_colors::MERGER, "Target");
            InputRow("Revenue ($M)",     &pf_tgt_rev_,     "%.0f");
            InputRow("EBITDA ($M)",      &pf_tgt_ebitda_,   "%.0f");
            InputRow("Net Income ($M)",  &pf_tgt_ni_,       "%.0f");
            InputRow("Deal EV ($M)",     &pf_tgt_ev_,       "%.0f");
            InputRowInt("Projection Year", &pf_year_);
        }
        inputs_collapsed_ = !open;
        ImGui::Spacing();

        // Calculate locally
        if (RunButton("BUILD PRO FORMA", false, ma_colors::MERGER)) {
            double pf_rev = pf_acq_rev_ + pf_tgt_rev_;
            double pf_ebitda = pf_acq_ebitda_ + pf_tgt_ebitda_;
            double pf_ni = pf_acq_ni_ + pf_tgt_ni_;
            double acq_eps = (pf_acq_shares_ > 0) ? pf_acq_ni_ / pf_acq_shares_ : 0;
            double pf_eps = (pf_acq_shares_ > 0) ? pf_ni / pf_acq_shares_ : 0;
            double eps_chg = (acq_eps != 0) ? ((pf_eps - acq_eps) / std::abs(acq_eps)) * 100 : 0;

            result_ = json{
                {"pro_forma_revenue", pf_rev},
                {"pro_forma_ebitda", pf_ebitda},
                {"pro_forma_net_income", pf_ni},
                {"standalone_eps", acq_eps},
                {"pro_forma_eps", pf_eps},
                {"eps_change_pct", eps_chg},
                {"result", eps_chg >= 0 ? "ACCRETIVE" : "DILUTIVE"},
                {"acq_revenue_pct", (pf_rev > 0) ? pf_acq_rev_ / pf_rev * 100 : 0},
                {"tgt_revenue_pct", (pf_rev > 0) ? pf_tgt_rev_ / pf_rev * 100 : 0}
            };
            status_ = "Pro forma built";
            status_time_ = ImGui::GetTime();
        }
    } break;

    case 1: { // Sources & Uses
        if (CollapsibleHeader("SOURCES & USES INPUTS", &open, ma_colors::MERGER)) {
            ImGui::Spacing();
            InputRow("Purchase Price ($M)", &su_price_,    "%.0f");
            InputRow("Target Debt ($M)",    &su_tgt_debt_, "%.0f");
            InputRow("Fees ($M)",           &su_fees_,     "%.0f");
        }
        inputs_collapsed_ = !open;
        ImGui::Spacing();

        if (RunButton("BUILD S&U", data.is_loading(), ma_colors::MERGER)) {
            json deal_structure = {
                {"purchase_price",        su_price_},
                {"target_debt_refinanced", su_tgt_debt_},
                {"transaction_fees",       su_fees_}
            };
            json financing = {
                {"acquirer_cash", su_price_ * 0.3},
                {"new_debt",     su_price_ * 0.4},
                {"new_equity",   su_price_ * 0.3}
            };
            data.run_analysis("merger_models/sources_uses.py", {
                "sources_uses", deal_structure.dump(), financing.dump()
            });
        }
    } break;

    case 2: { // Contribution Analysis
        if (CollapsibleHeader("CONTRIBUTION INPUTS", &open, ma_colors::MERGER)) {
            ImGui::Spacing();
            InputRow("Pro Forma Ownership %", &cont_ownership_, "%.1f");
        }
        inputs_collapsed_ = !open;
        ImGui::Spacing();

        // Calculate locally
        if (RunButton("ANALYZE CONTRIBUTION", false, ma_colors::MERGER)) {
            double acq_pct = cont_ownership_;
            double tgt_pct = 100.0 - cont_ownership_;
            double total_rev = pf_acq_rev_ + pf_tgt_rev_;
            double total_ebitda = pf_acq_ebitda_ + pf_tgt_ebitda_;

            result_ = json{
                {"acquirer_ownership_pct", acq_pct},
                {"target_ownership_pct", tgt_pct},
                {"acquirer_revenue_contribution", (total_rev > 0) ? pf_acq_rev_ / total_rev * 100 : 0},
                {"target_revenue_contribution", (total_rev > 0) ? pf_tgt_rev_ / total_rev * 100 : 0},
                {"acquirer_ebitda_contribution", (total_ebitda > 0) ? pf_acq_ebitda_ / total_ebitda * 100 : 0},
                {"target_ebitda_contribution", (total_ebitda > 0) ? pf_tgt_ebitda_ / total_ebitda * 100 : 0}
            };
            status_ = "Contribution analysis complete";
            status_time_ = ImGui::GetTime();
        }
    } break;
    }

    // Shared result pickup
    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Analysis complete";
        status_time_ = ImGui::GetTime();
        data.clear();
    }
    if (!data.is_loading() && !data.error().empty()) {
        status_ = "Error: " + data.error();
        status_time_ = ImGui::GetTime();
        data.clear();
    }

    // Generic result display
    if (!result_.is_null()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::MERGER, "RESULTS");
        ImGui::Spacing();

        for (auto& [key, val] : result_.items()) {
            if (val.is_number()) {
                bool is_pct = key.find("pct") != std::string::npos || key.find("contribution") != std::string::npos
                           || key.find("ownership") != std::string::npos;
                if (is_pct)
                    DataRow(key.c_str(), format_percent(val.get<double>()).c_str(), TEXT_PRIMARY);
                else
                    DataRow(key.c_str(), format_currency(val.get<double>()).c_str(), TEXT_PRIMARY);
            } else if (val.is_string()) {
                DataRow(key.c_str(), val.get<std::string>().c_str(), TEXT_SECONDARY);
            }
        }
    }
}

} // namespace fincept::mna
