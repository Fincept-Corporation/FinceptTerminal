#include "industry_panel.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nlohmann/json.hpp>

namespace fincept::mna {

using json = nlohmann::json;

// =============================================================================
// Main render
// =============================================================================
void IndustryPanel::render(MNAData& data) {
    const char* tabs[] = {"Technology", "Healthcare", "Financial Services"};
    for (int i = 0; i < 3; i++) {
        if (ColoredButton(tabs[i], ma_colors::INDUSTRY, industry_tab_ == i)) {
            industry_tab_ = i;
            result_ = json();
            status_.clear();
        }
        if (i < 2) ImGui::SameLine(0, 2);
    }
    ImGui::Separator();
    ImGui::Spacing();

    switch (industry_tab_) {
        case 0: render_tech(data);       break;
        case 1: render_healthcare(data); break;
        case 2: render_financial(data);  break;
        default: break;
    }

    ImGui::Spacing();
    StatusMessage(status_, status_time_);

    // Generic result display
    if (!result_.is_null()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::INDUSTRY, "RESULTS");
        ImGui::Spacing();

        // Display valuation if present
        if (result_.contains("implied_valuation"))
            DataRow("Implied Valuation", format_currency(result_["implied_valuation"].get<double>()).c_str(), theme::colors::SUCCESS);
        if (result_.contains("valuation_range") && result_["valuation_range"].is_object()) {
            auto& vr = result_["valuation_range"];
            if (vr.contains("low") && vr.contains("high")) {
                std::string range = format_currency(vr["low"].get<double>()) + " - " +
                                    format_currency(vr["high"].get<double>());
                DataRow("Valuation Range", range.c_str(), theme::colors::TEXT_SECONDARY);
            }
        }

        // Key multiples
        if (result_.contains("multiples") && result_["multiples"].is_object()) {
            ImGui::Spacing();
            ImGui::TextColored(theme::colors::TEXT_DIM, "Industry Multiples");
            for (auto& [k, v] : result_["multiples"].items()) {
                if (v.is_number()) {
                    DataRow(k.c_str(), format_multiple(v.get<double>()).c_str(), theme::colors::TEXT_PRIMARY);
                }
            }
        }

        // Benchmarks
        if (result_.contains("benchmarks") && result_["benchmarks"].is_object()) {
            ImGui::Spacing();
            ImGui::TextColored(theme::colors::TEXT_DIM, "Industry Benchmarks");
            for (auto& [k, v] : result_["benchmarks"].items()) {
                if (v.is_number()) {
                    bool is_pct = k.find("margin") != std::string::npos || k.find("growth") != std::string::npos
                               || k.find("rate") != std::string::npos || k.find("retention") != std::string::npos;
                    if (is_pct)
                        DataRow(k.c_str(), format_percent(v.get<double>()).c_str(), theme::colors::TEXT_SECONDARY);
                    else {
                        char buf[32];
                        std::snprintf(buf, sizeof(buf), "%.2f", v.get<double>());
                        DataRow(k.c_str(), buf, theme::colors::TEXT_SECONDARY);
                    }
                }
            }
        }

        // Other top-level fields
        for (auto& [k, v] : result_.items()) {
            if (k == "implied_valuation" || k == "valuation_range" || k == "multiples"
                || k == "benchmarks") continue;
            if (v.is_number()) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.2f", v.get<double>());
                DataRow(k.c_str(), buf, theme::colors::TEXT_PRIMARY);
            } else if (v.is_string()) {
                DataRow(k.c_str(), v.get<std::string>().c_str(), theme::colors::TEXT_SECONDARY);
            }
        }
    }
}

// =============================================================================
// Technology
// =============================================================================
void IndustryPanel::render_tech(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("TECHNOLOGY INPUTS", &open, ma_colors::INDUSTRY)) {
        ImGui::Spacing();

        const char* sectors[] = {"SaaS", "Marketplace", "Semiconductor"};
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Sector");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(140);
        ImGui::Combo("##tech_sector", &tech_sector_, sectors, 3);

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::INDUSTRY, "Company Metrics");
        ImGui::Spacing();
        InputRow("ARR ($M)",           &tech_inputs_[0], "%.0f");
        InputRow("Revenue Growth (%)", &tech_inputs_[1], "%.1f");
        InputRow("Gross Margin (%)",   &tech_inputs_[2], "%.1f");
        InputRow("Net Retention (%)",  &tech_inputs_[3], "%.1f");
        InputRow("Rule of 40 Score",   &tech_inputs_[4], "%.1f");
        InputRow("CAC Payback (mo)",   &tech_inputs_[5], "%.1f");
        InputRow("LTV/CAC Ratio",      &tech_inputs_[6], "%.1f");
        InputRow("EBITDA Margin (%)",  &tech_inputs_[7], "%.1f");
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("ANALYZE TECH", data.is_loading(), ma_colors::INDUSTRY)) {
        const char* sector_names[] = {"saas", "marketplace", "semiconductor"};
        json company = {
            {"arr",              tech_inputs_[0]},
            {"revenue_growth",   tech_inputs_[1]},
            {"gross_margin",     tech_inputs_[2]},
            {"net_retention",    tech_inputs_[3]},
            {"rule_of_40",       tech_inputs_[4]},
            {"cac_payback",      tech_inputs_[5]},
            {"ltv_cac",          tech_inputs_[6]},
            {"ebitda_margin",    tech_inputs_[7]}
        };
        data.run_analysis("industry_metrics/technology.py", {
            "tech", sector_names[tech_sector_], company.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Tech analysis complete";
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
// Healthcare
// =============================================================================
void IndustryPanel::render_healthcare(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("HEALTHCARE INPUTS", &open, ma_colors::INDUSTRY)) {
        ImGui::Spacing();

        const char* sectors[] = {"Pharma", "Biotech", "Medical Devices", "Healthcare Services"};
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Sector");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(160);
        ImGui::Combo("##hc_sector", &hc_sector_, sectors, 4);

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::INDUSTRY, "Company Metrics");
        ImGui::Spacing();
        InputRow("Revenue ($M)",       &hc_inputs_[0], "%.0f");
        InputRow("EBITDA ($M)",        &hc_inputs_[1], "%.0f");
        InputRow("R&D Spend ($M)",     &hc_inputs_[2], "%.0f");
        InputRow("Pipeline Value ($M)",&hc_inputs_[3], "%.0f");
        InputRow("Revenue Growth (%)", &hc_inputs_[4], "%.1f");
        InputRow("Gross Margin (%)",   &hc_inputs_[5], "%.1f");
        InputRow("Market Share (%)",   &hc_inputs_[6], "%.1f");
        InputRow("Patent Expiry (yrs)",&hc_inputs_[7], "%.0f");
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("ANALYZE HEALTHCARE", data.is_loading(), ma_colors::INDUSTRY)) {
        const char* sector_names[] = {"pharma", "biotech", "devices", "services"};
        json company = {
            {"revenue",         hc_inputs_[0]},
            {"ebitda",          hc_inputs_[1]},
            {"rd_spend",        hc_inputs_[2]},
            {"pipeline_value",  hc_inputs_[3]},
            {"revenue_growth",  hc_inputs_[4]},
            {"gross_margin",    hc_inputs_[5]},
            {"market_share",    hc_inputs_[6]},
            {"patent_expiry",   hc_inputs_[7]}
        };
        data.run_analysis("industry_metrics/healthcare.py", {
            "healthcare", sector_names[hc_sector_], company.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Healthcare analysis complete";
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
// Financial Services
// =============================================================================
void IndustryPanel::render_financial(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("FINANCIAL SERVICES INPUTS", &open, ma_colors::INDUSTRY)) {
        ImGui::Spacing();

        const char* sectors[] = {"Banking", "Insurance", "Asset Management"};
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Sector");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(160);
        ImGui::Combo("##fin_sector", &fin_sector_, sectors, 3);

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::INDUSTRY, "Institution Metrics");
        ImGui::Spacing();
        InputRow("Total Assets ($M)",  &fin_inputs_[0], "%.0f");
        InputRow("Revenue ($M)",       &fin_inputs_[1], "%.0f");
        InputRow("Net Income ($M)",    &fin_inputs_[2], "%.0f");
        InputRow("Book Value ($M)",    &fin_inputs_[3], "%.0f");
        InputRow("AUM ($M)",           &fin_inputs_[4], "%.0f");
        InputRow("ROE (%)",            &fin_inputs_[5], "%.1f");
        InputRow("NIM / Fee Ratio (%)", &fin_inputs_[6], "%.2f");
        InputRow("Cost/Income (%)",    &fin_inputs_[7], "%.1f");
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("ANALYZE FINANCIAL", data.is_loading(), ma_colors::INDUSTRY)) {
        const char* sector_names[] = {"banking", "insurance", "asset_management"};
        json institution = {
            {"total_assets",  fin_inputs_[0]},
            {"revenue",       fin_inputs_[1]},
            {"net_income",    fin_inputs_[2]},
            {"book_value",    fin_inputs_[3]},
            {"aum",           fin_inputs_[4]},
            {"roe",           fin_inputs_[5]},
            {"nim_fee_ratio", fin_inputs_[6]},
            {"cost_income",   fin_inputs_[7]}
        };
        data.run_analysis("industry_metrics/financial_services.py", {
            "financial", sector_names[fin_sector_], institution.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Financial analysis complete";
        status_time_ = ImGui::GetTime();
        data.clear();
    }
    if (!data.is_loading() && !data.error().empty()) {
        status_ = "Error: " + data.error();
        status_time_ = ImGui::GetTime();
        data.clear();
    }
}

} // namespace fincept::mna
