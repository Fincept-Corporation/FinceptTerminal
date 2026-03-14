#include "valuation_panel.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstdio>

namespace fincept::mna {

using json = nlohmann::json;

// =============================================================================
// Main render — sub-tab bar + dispatch
// =============================================================================
void ValuationPanel::render(MNAData& data) {
    render_sub_tabs();
    ImGui::Separator();
    ImGui::Spacing();

    switch (sub_tab_) {
        case 0: render_dcf(data);             break;
        case 1: render_lbo_returns(data);      break;
        case 2: render_lbo_model(data);        break;
        case 3: render_debt_schedule(data);    break;
        case 4: render_lbo_sensitivity(data);  break;
        case 5: render_comps(data);            break;
        case 6: render_precedent(data);        break;
        default: break;
    }

    ImGui::Spacing();
    render_result_section();
}

// =============================================================================
// Sub-tab bar
// =============================================================================
void ValuationPanel::render_sub_tabs() {
    const char* tabs[] = {
        "DCF", "LBO Returns", "LBO Model", "Debt Schedule",
        "LBO Sensitivity", "Trading Comps", "Precedent Txn"
    };
    for (int i = 0; i < 7; i++) {
        if (ColoredButton(tabs[i], ma_colors::VALUATION, sub_tab_ == i)) {
            sub_tab_ = i;
            has_result_ = false;
            result_ = json();
            status_.clear();
        }
        if (i < 6) ImGui::SameLine(0, 2);
    }
}

// =============================================================================
// DCF Analysis
// =============================================================================
void ValuationPanel::render_dcf(MNAData& data) {
    using namespace theme::colors;

    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("DCF INPUTS", &open, ma_colors::VALUATION)) {
        ImGui::Spacing();

        // WACC inputs
        ImGui::TextColored(ma_colors::VALUATION, "WACC Parameters");
        ImGui::Spacing();
        InputRow("Risk-Free Rate",   &dcf_rfr_,  "%.4f");
        InputRow("Beta",             &dcf_beta_,  "%.2f");
        InputRow("Market Cap ($M)",  &dcf_mcap_,  "%.0f");
        InputRow("Debt ($M)",        &dcf_debt_,  "%.0f");
        InputRow("Cost of Debt",     &dcf_cod_,   "%.4f");
        InputRow("Tax Rate",         &dcf_tax_,   "%.4f");

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::VALUATION, "FCF Parameters");
        ImGui::Spacing();
        InputRow("EBIT ($M)",        &dcf_ebit_,  "%.0f");
        InputRow("Depreciation ($M)",&dcf_depr_,  "%.0f");
        InputRow("CapEx ($M)",       &dcf_capex_, "%.0f");
        InputRow("Change NWC ($M)",  &dcf_nwc_,   "%.0f");

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::VALUATION, "Valuation");
        ImGui::Spacing();
        InputRow("Terminal Growth",  &dcf_term_growth_, "%.4f");
        InputRow("Cash ($M)",        &dcf_cash_,  "%.0f");
        InputRow("Shares Out (M)",   &dcf_shares_,"%.1f");
    }
    inputs_collapsed_ = !open;

    ImGui::Spacing();

    if (RunButton("RUN DCF ANALYSIS", data.is_loading(), ma_colors::VALUATION)) {
        // Build JSON args for dcf_model.py dcf command
        json wacc_inputs = {
            {"risk_free_rate",       dcf_rfr_},
            {"market_risk_premium",  0.06},
            {"beta",                 dcf_beta_},
            {"cost_of_debt",         dcf_cod_},
            {"tax_rate",             dcf_tax_},
            {"market_value_equity",  dcf_mcap_},
            {"market_value_debt",    dcf_debt_}
        };
        json fcf_inputs = {
            {"ebit",          dcf_ebit_},
            {"tax_rate",      dcf_tax_},
            {"depreciation",  dcf_depr_},
            {"capex",         dcf_capex_},
            {"change_in_nwc", dcf_nwc_}
        };
        json growth_rates = {0.08, 0.07, 0.06, 0.05, 0.04};
        json balance_sheet = {
            {"cash", dcf_cash_},
            {"debt", dcf_debt_}
        };

        data.run_analysis("valuation/dcf_model.py", {
            "dcf",
            wacc_inputs.dump(),
            fcf_inputs.dump(),
            growth_rates.dump(),
            std::to_string(dcf_term_growth_),
            balance_sheet.dump(),
            std::to_string(dcf_shares_)
        });
    }

    // Check for results
    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        has_result_ = true;
        status_ = "DCF analysis complete";
        status_time_ = ImGui::GetTime();
        data.clear();
    }
    if (!data.is_loading() && !data.error().empty()) {
        status_ = "Error: " + data.error();
        status_time_ = ImGui::GetTime();
        data.clear();
    }
}

// =============================================================================
// LBO Returns (simple)
// =============================================================================
void ValuationPanel::render_lbo_returns(MNAData& data) {
    using namespace theme::colors;

    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("LBO RETURNS INPUTS", &open, ma_colors::VALUATION)) {
        ImGui::Spacing();
        InputRow("Entry Value ($M)",  &lbo_entry_,  "%.0f");
        InputRow("Exit Value ($M)",   &lbo_exit_,   "%.0f");
        InputRow("Equity Invested ($M)", &lbo_equity_, "%.0f");
        InputRowInt("Holding Period (yrs)", &lbo_period_);
    }
    inputs_collapsed_ = !open;

    ImGui::Spacing();

    // Calculate locally — no Python needed for simple IRR
    if (RunButton("CALCULATE RETURNS", false, ma_colors::VALUATION)) {
        double moic = (lbo_equity_ > 0) ? lbo_exit_ / lbo_equity_ : 0;
        double irr = (lbo_equity_ > 0 && lbo_period_ > 0)
            ? (std::pow(lbo_exit_ / lbo_equity_, 1.0 / lbo_period_) - 1.0) * 100.0
            : 0;
        double cash_return = lbo_exit_ - lbo_equity_;

        result_ = json{
            {"moic", moic},
            {"irr", irr},
            {"cash_return", cash_return},
            {"entry_value", lbo_entry_},
            {"exit_value", lbo_exit_},
            {"equity_invested", lbo_equity_},
            {"holding_period", lbo_period_}
        };
        has_result_ = true;
        status_ = "LBO returns calculated";
        status_time_ = ImGui::GetTime();
    }
}

// =============================================================================
// LBO Full Model
// =============================================================================
void ValuationPanel::render_lbo_model(MNAData& data) {
    using namespace theme::colors;

    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("LBO MODEL INPUTS", &open, ma_colors::VALUATION)) {
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::VALUATION, "Company Data");
        ImGui::Spacing();
        InputRow("Revenue ($M)",       &lbo_model_.revenue,       "%.0f");
        InputRow("EBITDA ($M)",        &lbo_model_.ebitda,        "%.0f");
        InputRow("EBIT ($M)",         &lbo_model_.ebit,          "%.0f");
        InputRow("CapEx ($M)",        &lbo_model_.capex,         "%.0f");
        InputRow("NWC ($M)",          &lbo_model_.nwc,           "%.0f");

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::VALUATION, "Transaction");
        ImGui::Spacing();
        InputRow("Purchase Price ($M)",&lbo_model_.purchase_price,"%.0f");
        InputRow("Entry Multiple",     &lbo_model_.entry_multiple,"%.1f");
        InputRow("Exit Multiple",      &lbo_model_.exit_multiple, "%.1f");
        InputRow("Debt %",            &lbo_model_.debt_percent,  "%.2f");
        InputRow("Rev Growth",         &lbo_model_.revenue_growth,"%.4f");
        InputRow("EBITDA Margin",      &lbo_model_.ebitda_margin, "%.4f");
        InputRow("Tax Rate",           &lbo_model_.tax_rate,      "%.4f");
        InputRow("Hurdle IRR",         &lbo_model_.hurdle_irr,    "%.4f");
        InputRowInt("Projection Years", &lbo_model_.projection_years);
    }
    inputs_collapsed_ = !open;

    ImGui::Spacing();

    if (RunButton("BUILD LBO MODEL", data.is_loading(), ma_colors::VALUATION)) {
        json target = {
            {"company_name", std::string(lbo_model_.company_name)},
            {"revenue",      lbo_model_.revenue},
            {"ebitda",       lbo_model_.ebitda},
            {"ebit",         lbo_model_.ebit},
            {"capex",        lbo_model_.capex},
            {"nwc",          lbo_model_.nwc}
        };
        json assumptions = {
            {"entry_multiple",    lbo_model_.entry_multiple},
            {"exit_multiples",    {lbo_model_.exit_multiple}},
            {"target_leverage",   lbo_model_.debt_percent * lbo_model_.entry_multiple},
            {"revenue_growth",    lbo_model_.revenue_growth},
            {"ebitda_margin",     lbo_model_.ebitda_margin},
            {"capex_pct_revenue", lbo_model_.capex_pct_revenue},
            {"nwc_pct_revenue",   lbo_model_.nwc_pct_revenue},
            {"tax_rate",          lbo_model_.tax_rate},
            {"hurdle_irr",        lbo_model_.hurdle_irr},
            {"exit_year",         lbo_model_.projection_years},
            {"projection_years",  lbo_model_.projection_years}
        };

        data.run_analysis("lbo/lbo_model.py", {
            "build",
            target.dump(),
            assumptions.dump(),
            std::to_string(lbo_model_.projection_years)
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        has_result_ = true;
        status_ = "LBO model complete";
        status_time_ = ImGui::GetTime();
        data.clear();
    }
    if (!data.is_loading() && !data.error().empty()) {
        status_ = "Error: " + data.error();
        status_time_ = ImGui::GetTime();
        data.clear();
    }
}

// =============================================================================
// Debt Schedule (uses LBO model inputs for context)
// =============================================================================
void ValuationPanel::render_debt_schedule(MNAData& /*data*/) {
    using namespace theme::colors;

    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("DEBT SCHEDULE INPUTS", &open, ma_colors::VALUATION)) {
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::VALUATION, "Senior Debt");
        ImGui::Spacing();
        InputRow("Amount ($M)",     &ds_senior_,      "%.0f");
        InputRow("Rate",            &ds_senior_rate_,  "%.4f");
        InputRowInt("Term (yrs)",   &ds_senior_term_);

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::VALUATION, "Subordinated Debt");
        ImGui::Spacing();
        InputRow("Amount ($M)",     &ds_sub_,          "%.0f");
        InputRow("Rate",            &ds_sub_rate_,     "%.4f");
        InputRowInt("Term (yrs)",   &ds_sub_term_);

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::VALUATION, "Revolver");
        ImGui::Spacing();
        InputRow("Facility ($M)",   &ds_revolver_,     "%.0f");
        InputRow("Rate",            &ds_revolver_rate_, "%.4f");
        InputRow("Cash Sweep %",    &ds_sweep_,        "%.2f");
    }
    inputs_collapsed_ = !open;

    ImGui::Spacing();

    // Calculate debt schedule locally
    if (RunButton("CALCULATE SCHEDULE", false, ma_colors::VALUATION)) {
        json schedule = json::array();
        double sr_bal = ds_senior_, sub_bal = ds_sub_;
        double sr_amort = (ds_senior_term_ > 0) ? ds_senior_ / ds_senior_term_ : 0;
        double sub_amort = (ds_sub_term_ > 0) ? ds_sub_ / ds_sub_term_ : 0;

        for (int y = 1; y <= std::max(ds_senior_term_, ds_sub_term_); y++) {
            double sr_int = sr_bal * ds_senior_rate_;
            double sub_int = sub_bal * ds_sub_rate_;
            double rev_int = ds_revolver_ * ds_revolver_rate_;
            double sr_pay = (y <= ds_senior_term_) ? sr_amort : 0;
            double sub_pay = (y <= ds_sub_term_) ? sub_amort : 0;
            sr_bal -= sr_pay;
            sub_bal -= sub_pay;
            if (sr_bal < 0) sr_bal = 0;
            if (sub_bal < 0) sub_bal = 0;

            schedule.push_back({
                {"year", y},
                {"senior_balance", sr_bal + sr_pay},
                {"senior_interest", sr_int},
                {"senior_amortization", sr_pay},
                {"senior_ending", sr_bal},
                {"sub_balance", sub_bal + sub_pay},
                {"sub_interest", sub_int},
                {"sub_amortization", sub_pay},
                {"sub_ending", sub_bal},
                {"revolver_interest", rev_int},
                {"total_interest", sr_int + sub_int + rev_int},
                {"total_debt_ending", sr_bal + sub_bal + ds_revolver_}
            });
        }

        result_ = json{
            {"schedule", schedule},
            {"total_senior", ds_senior_},
            {"total_sub", ds_sub_},
            {"revolver", ds_revolver_}
        };
        has_result_ = true;
        status_ = "Debt schedule calculated";
        status_time_ = ImGui::GetTime();
    }
}

// =============================================================================
// LBO Sensitivity
// =============================================================================
void ValuationPanel::render_lbo_sensitivity(MNAData& data) {
    using namespace theme::colors;

    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("SENSITIVITY INPUTS", &open, ma_colors::VALUATION)) {
        ImGui::Spacing();
        InputRow("Revenue ($M)",       &sens_revenue_,  "%.0f");
        InputRow("EBITDA Margin",      &sens_margin_,   "%.4f");
        InputRow("Exit Multiple",      &sens_exit_,     "%.1f");
        InputRow("Debt %",            &sens_debt_pct_,  "%.2f");
        InputRowInt("Holding Period",  &sens_period_);
    }
    inputs_collapsed_ = !open;

    ImGui::Spacing();

    if (RunButton("RUN SENSITIVITY", data.is_loading(), ma_colors::VALUATION)) {
        json base_case = {
            {"company_name", "Target"},
            {"revenue",       sens_revenue_},
            {"ebitda_margin",  sens_margin_},
            {"exit_multiple",  sens_exit_},
            {"debt_percent",   sens_debt_pct_ * 100},
            {"entry_multiple", sens_exit_},
            {"holding_period", sens_period_}
        };
        json rev_growth = {0.02, 0.04, 0.06, 0.08, 0.10};
        json exit_mults = {7.0, 8.0, 9.0, 10.0, 11.0, 12.0};

        data.run_analysis("lbo/lbo_model.py", {
            "sensitivity",
            base_case.dump(),
            rev_growth.dump(),
            exit_mults.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        has_result_ = true;
        status_ = "Sensitivity analysis complete";
        status_time_ = ImGui::GetTime();
        data.clear();
    }
    if (!data.is_loading() && !data.error().empty()) {
        status_ = "Error: " + data.error();
        status_time_ = ImGui::GetTime();
        data.clear();
    }
}

// =============================================================================
// Trading Comps
// =============================================================================
void ValuationPanel::render_comps(MNAData& data) {
    using namespace theme::colors;

    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("TRADING COMPS INPUTS", &open, ma_colors::VALUATION)) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Target Ticker");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(120);
        ImGui::InputText("##comps_target", comps_target_, sizeof(comps_target_));

        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Comp Tickers (CSV)");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##comps_tickers", comps_tickers_, sizeof(comps_tickers_));
    }
    inputs_collapsed_ = !open;

    ImGui::Spacing();

    if (RunButton("FETCH COMPS", data.is_loading(), ma_colors::VALUATION)) {
        // Parse CSV tickers into JSON array
        json tickers = json::array();
        std::string csv(comps_tickers_);
        size_t pos = 0;
        while ((pos = csv.find(',')) != std::string::npos) {
            std::string t = csv.substr(0, pos);
            // Trim whitespace
            while (!t.empty() && t.front() == ' ') t.erase(t.begin());
            while (!t.empty() && t.back() == ' ') t.pop_back();
            if (!t.empty()) tickers.push_back(t);
            csv = csv.substr(pos + 1);
        }
        // Last token
        while (!csv.empty() && csv.front() == ' ') csv.erase(csv.begin());
        while (!csv.empty() && csv.back() == ' ') csv.pop_back();
        if (!csv.empty()) tickers.push_back(csv);

        std::string target(comps_target_);
        if (target.empty()) target = "AAPL";

        data.run_analysis("valuation/trading_comps.py", {
            "trading_comps",
            target,
            tickers.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        has_result_ = true;
        status_ = "Trading comps fetched";
        status_time_ = ImGui::GetTime();
        data.clear();
    }
    if (!data.is_loading() && !data.error().empty()) {
        status_ = "Error: " + data.error();
        status_time_ = ImGui::GetTime();
        data.clear();
    }
}

// =============================================================================
// Precedent Transactions
// =============================================================================
void ValuationPanel::render_precedent(MNAData& data) {
    using namespace theme::colors;

    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("PRECEDENT TXN INPUTS", &open, ma_colors::VALUATION)) {
        ImGui::Spacing();
        InputRow("Target Revenue ($M)", &prec_revenue_, "%.0f");
        InputRow("Target EBITDA ($M)",  &prec_ebitda_,  "%.0f");
    }
    inputs_collapsed_ = !open;

    ImGui::Spacing();

    if (RunButton("ANALYZE PRECEDENTS", data.is_loading(), ma_colors::VALUATION)) {
        json target_data = {
            {"revenue", prec_revenue_},
            {"ebitda",  prec_ebitda_}
        };
        // Sample precedent deals for analysis
        json comp_deals = json::array({
            {{"target_name", "CompanyA"}, {"acquirer_name", "AcquirerA"}, {"deal_value", 2500},
             {"revenue", 800}, {"ebitda", 160}, {"ev_revenue", 3.1}, {"ev_ebitda", 15.6},
             {"premium_1day", 25}, {"payment_method", "Cash"}, {"status", "Completed"}},
            {{"target_name", "CompanyB"}, {"acquirer_name", "AcquirerB"}, {"deal_value", 1800},
             {"revenue", 600}, {"ebitda", 120}, {"ev_revenue", 3.0}, {"ev_ebitda", 15.0},
             {"premium_1day", 30}, {"payment_method", "Stock"}, {"status", "Completed"}},
            {{"target_name", "CompanyC"}, {"acquirer_name", "AcquirerC"}, {"deal_value", 3200},
             {"revenue", 1000}, {"ebitda", 200}, {"ev_revenue", 3.2}, {"ev_ebitda", 16.0},
             {"premium_1day", 22}, {"payment_method", "Mixed"}, {"status", "Completed"}}
        });

        data.run_analysis("valuation/precedent_transactions.py", {
            "precedent",
            target_data.dump(),
            comp_deals.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        has_result_ = true;
        status_ = "Precedent analysis complete";
        status_time_ = ImGui::GetTime();
        data.clear();
    }
    if (!data.is_loading() && !data.error().empty()) {
        status_ = "Error: " + data.error();
        status_time_ = ImGui::GetTime();
        data.clear();
    }
}

// =============================================================================
// Result display — renders JSON result as formatted data rows
// =============================================================================
void ValuationPanel::render_result_section() {
    using namespace theme::colors;

    StatusMessage(status_, status_time_);

    if (!has_result_ || result_.is_null()) return;

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ma_colors::VALUATION, "RESULTS");
    ImGui::Spacing();

    // === DCF results ===
    if (result_.contains("summary")) {
        auto& s = result_["summary"];
        ImGui::TextColored(ma_colors::VALUATION, "DCF Valuation Summary");
        ImGui::Spacing();
        if (s.contains("enterprise_value"))
            DataRow("Enterprise Value", format_currency(s["enterprise_value"].get<double>()).c_str(), TEXT_PRIMARY);
        if (s.contains("equity_value"))
            DataRow("Equity Value", format_currency(s["equity_value"].get<double>()).c_str(), TEXT_PRIMARY);
        if (s.contains("price_per_share")) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "$%.2f", s["price_per_share"].get<double>());
            DataRow("Price Per Share", buf, SUCCESS);
        }
        if (s.contains("wacc")) {
            DataRow("WACC", format_percent(s["wacc"].get<double>()).c_str(), TEXT_SECONDARY);
        }
        if (s.contains("terminal_growth"))
            DataRow("Terminal Growth", format_percent(s["terminal_growth"].get<double>()).c_str(), TEXT_SECONDARY);

        // WACC breakdown
        if (result_.contains("wacc")) {
            ImGui::Spacing();
            ImGui::TextColored(TEXT_DIM, "WACC Breakdown");
            auto& w = result_["wacc"];
            if (w.contains("cost_of_equity"))
                DataRow("Cost of Equity", format_percent(w["cost_of_equity"].get<double>()).c_str(), TEXT_SECONDARY);
            if (w.contains("cost_of_debt_aftertax"))
                DataRow("After-Tax Cost of Debt", format_percent(w["cost_of_debt_aftertax"].get<double>()).c_str(), TEXT_SECONDARY);
            if (w.contains("equity_weight"))
                DataRow("Equity Weight", format_percent(w["equity_weight"].get<double>()).c_str(), TEXT_SECONDARY);
            if (w.contains("debt_weight"))
                DataRow("Debt Weight", format_percent(w["debt_weight"].get<double>()).c_str(), TEXT_SECONDARY);
        }

        // FCF projections
        if (result_.contains("fcf_projections") && result_["fcf_projections"].is_array()) {
            ImGui::Spacing();
            ImGui::TextColored(TEXT_DIM, "FCF Projections");
            for (auto& p : result_["fcf_projections"]) {
                char label[32], val[32];
                std::snprintf(label, sizeof(label), "Year %d", p.value("year", 0));
                std::snprintf(val, sizeof(val), "$%.1fM (%.1f%%)", p.value("fcf", 0.0), p.value("growth_rate", 0.0));
                DataRow(label, val, TEXT_SECONDARY);
            }
        }
    }

    // === LBO Returns results (local calc) ===
    if (result_.contains("moic") && result_.contains("irr") && !result_.contains("summary")) {
        ImGui::TextColored(ma_colors::VALUATION, "LBO Returns Summary");
        ImGui::Spacing();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2fx", result_["moic"].get<double>());
        DataRow("MOIC", buf, SUCCESS);
        DataRow("IRR", format_percent(result_["irr"].get<double>()).c_str(), SUCCESS);
        DataRow("Cash Return", format_currency(result_["cash_return"].get<double>()).c_str(), TEXT_PRIMARY);
        DataRow("Holding Period", (std::to_string(result_["holding_period"].get<int>()) + " years").c_str(), TEXT_SECONDARY);
    }

    // === LBO Model results ===
    if (result_.contains("transaction_summary") && result_.contains("returns")) {
        auto& ts = result_["transaction_summary"];
        auto& ret = result_["returns"];

        ImGui::TextColored(ma_colors::VALUATION, "LBO Model Results");
        ImGui::Spacing();

        if (ts.contains("entry_enterprise_value"))
            DataRow("Entry EV", format_currency(ts["entry_enterprise_value"].get<double>()).c_str(), TEXT_PRIMARY);
        if (ts.contains("entry_multiple")) {
            DataRow("Entry Multiple", format_multiple(ts["entry_multiple"].get<double>()).c_str(), TEXT_SECONDARY);
        }

        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "Returns");
        if (ret.contains("irr"))
            DataRow("IRR", format_percent(ret["irr"].get<double>() * 100).c_str(), SUCCESS);
        if (ret.contains("moic")) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.2fx", ret["moic"].get<double>());
            DataRow("MOIC", buf, SUCCESS);
        }
        if (ret.contains("entry_equity"))
            DataRow("Entry Equity", format_currency(ret["entry_equity"].get<double>()).c_str(), TEXT_SECONDARY);
        if (ret.contains("exit_equity"))
            DataRow("Exit Equity", format_currency(ret["exit_equity"].get<double>()).c_str(), TEXT_SECONDARY);

        // Sources & Uses
        if (result_.contains("sources_uses")) {
            ImGui::Spacing();
            ImGui::TextColored(TEXT_DIM, "Sources & Uses");
            auto& su = result_["sources_uses"];
            if (su.contains("sources")) {
                auto& src = su["sources"];
                if (src.contains("senior_debt"))
                    DataRow("Senior Debt", format_currency(src["senior_debt"].get<double>()).c_str(), TEXT_SECONDARY);
                if (src.contains("subordinated_debt"))
                    DataRow("Sub Debt", format_currency(src["subordinated_debt"].get<double>()).c_str(), TEXT_SECONDARY);
                if (src.contains("sponsor_equity"))
                    DataRow("Sponsor Equity", format_currency(src["sponsor_equity"].get<double>()).c_str(), TEXT_SECONDARY);
            }
        }
    }

    // === Debt Schedule results ===
    if (result_.contains("schedule") && result_["schedule"].is_array()) {
        ImGui::TextColored(ma_colors::VALUATION, "Debt Schedule");
        ImGui::Spacing();

        // Table header
        if (ImGui::BeginTable("##debt_sched", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Year", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Sr Balance", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("Sr Interest", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Sub Balance", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("Total Interest", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("Total Debt", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableHeadersRow();

            for (auto& row : result_["schedule"]) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%d", row.value("year", 0));
                ImGui::TableNextColumn(); ImGui::Text("$%.0f", row.value("senior_ending", 0.0));
                ImGui::TableNextColumn(); ImGui::Text("$%.1f", row.value("senior_interest", 0.0));
                ImGui::TableNextColumn(); ImGui::Text("$%.0f", row.value("sub_ending", 0.0));
                ImGui::TableNextColumn(); ImGui::Text("$%.1f", row.value("total_interest", 0.0));
                ImGui::TableNextColumn(); ImGui::Text("$%.0f", row.value("total_debt_ending", 0.0));
            }
            ImGui::EndTable();
        }
    }

    // === Sensitivity results (LBO) ===
    if (result_.contains("irr_matrix") && result_["irr_matrix"].is_array()) {
        ImGui::TextColored(ma_colors::VALUATION, "IRR Sensitivity Matrix");
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "Rows: Revenue Growth  |  Columns: Exit Multiple");
        ImGui::Spacing();

        auto& irr_m = result_["irr_matrix"];
        auto& exit_range = result_["exit_multiple_range"];
        int cols = exit_range.is_array() ? static_cast<int>(exit_range.size()) : 0;
        auto& rev_range = result_["revenue_growth_range"];

        if (cols > 0 && ImGui::BeginTable("##irr_sens", cols + 1,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Rev Growth", ImGuiTableColumnFlags_WidthFixed, 80);
            for (int c = 0; c < cols; c++) {
                char hdr[16];
                std::snprintf(hdr, sizeof(hdr), "%.1fx", exit_range[c].get<double>());
                ImGui::TableSetupColumn(hdr, ImGuiTableColumnFlags_WidthFixed, 60);
            }
            ImGui::TableHeadersRow();

            for (size_t r = 0; r < irr_m.size(); r++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (rev_range.is_array() && r < rev_range.size())
                    ImGui::Text("%.0f%%", rev_range[r].get<double>() * 100);
                else
                    ImGui::Text("Row %zu", r);

                auto& row = irr_m[r];
                for (size_t c = 0; c < row.size(); c++) {
                    ImGui::TableNextColumn();
                    double val = row[c].get<double>() * 100;
                    ImVec4 color = (val >= 20) ? SUCCESS :
                                   (val >= 10) ? ImVec4(1.0f, 0.77f, 0.0f, 1.0f) :
                                                 ImVec4(0.96f, 0.30f, 0.30f, 1.0f);
                    ImGui::TextColored(color, "%.1f%%", val);
                }
            }
            ImGui::EndTable();
        }
    }

    // === Trading Comps results ===
    if (result_.contains("comparables") && result_["comparables"].is_array()) {
        ImGui::TextColored(ma_colors::VALUATION, "Trading Comparables");
        ImGui::Spacing();

        auto& comps = result_["comparables"];
        if (!comps.empty() && ImGui::BeginTable("##comps_table", 8,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) {
            ImGui::TableSetupColumn("Ticker", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Mkt Cap ($M)", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("EV/Rev", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("EV/EBITDA", ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableSetupColumn("P/E", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Rev Growth", ImGuiTableColumnFlags_WidthFixed, 75);
            ImGui::TableSetupColumn("EBITDA Mgn", ImGuiTableColumnFlags_WidthFixed, 75);
            ImGui::TableSetupColumn("ROE", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableHeadersRow();

            for (auto& c : comps) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", c.value("Ticker", "").c_str());
                ImGui::TableNextColumn(); ImGui::Text("%.0f", c.value("Market Cap ($M)", 0.0));
                ImGui::TableNextColumn(); ImGui::Text("%.1fx", c.value("EV/Revenue", 0.0));
                ImGui::TableNextColumn(); ImGui::Text("%.1fx", c.value("EV/EBITDA", 0.0));
                ImGui::TableNextColumn(); ImGui::Text("%.1f", c.value("P/E", 0.0));
                ImGui::TableNextColumn(); ImGui::Text("%.1f%%", c.value("Rev Growth (%)", 0.0));
                ImGui::TableNextColumn(); ImGui::Text("%.1f%%", c.value("EBITDA Margin (%)", 0.0));
                ImGui::TableNextColumn(); ImGui::Text("%.1f%%", c.value("ROE (%)", 0.0));
            }
            ImGui::EndTable();
        }

        // Target valuation from comps
        if (result_.contains("target_valuation")) {
            auto& tv = result_["target_valuation"];
            if (tv.contains("valuations")) {
                ImGui::Spacing();
                ImGui::TextColored(TEXT_DIM, "Implied Valuations");
                auto& v = tv["valuations"];
                if (v.contains("ev_ebitda_median"))
                    DataRow("EV/EBITDA (Median)", format_currency(v["ev_ebitda_median"].get<double>()).c_str(), TEXT_PRIMARY);
                if (v.contains("ev_revenue_median"))
                    DataRow("EV/Revenue (Median)", format_currency(v["ev_revenue_median"].get<double>()).c_str(), TEXT_PRIMARY);
                if (v.contains("blended_median"))
                    DataRow("Blended Median", format_currency(v["blended_median"].get<double>()).c_str(), SUCCESS);
            }
        }
    }

    // === Precedent Transaction results ===
    if (result_.contains("comp_count") && result_.contains("summary_statistics")
        && !result_.contains("target_ticker")) {
        // This distinguishes precedent from trading comps
        if (result_.contains("target_valuation")) {
            auto& tv = result_["target_valuation"];
            if (tv.contains("valuations")) {
                ImGui::TextColored(ma_colors::VALUATION, "Precedent Transaction Valuation");
                ImGui::Spacing();
                auto& v = tv["valuations"];
                if (v.contains("ev_ebitda_median"))
                    DataRow("EV/EBITDA (Median)", format_currency(v["ev_ebitda_median"].get<double>()).c_str(), TEXT_PRIMARY);
                if (v.contains("ev_revenue_median"))
                    DataRow("EV/Revenue (Median)", format_currency(v["ev_revenue_median"].get<double>()).c_str(), TEXT_PRIMARY);
                if (v.contains("blended_median"))
                    DataRow("Blended Valuation", format_currency(v["blended_median"].get<double>()).c_str(), SUCCESS);
            }
        }
    }
}

} // namespace fincept::mna
