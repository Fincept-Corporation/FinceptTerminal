#include "advanced_panel.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nlohmann/json.hpp>

namespace fincept::mna {

using json = nlohmann::json;

// =============================================================================
// Main render
// =============================================================================
void AdvancedPanel::render(MNAData& data) {
    const char* tabs[] = {"Monte Carlo Simulation", "Regression Analysis"};
    for (int i = 0; i < 2; i++) {
        if (ColoredButton(tabs[i], ma_colors::ADVANCED, sub_tab_ == i)) {
            sub_tab_ = i;
            result_ = json();
            status_.clear();
        }
        if (i == 0) ImGui::SameLine(0, 2);
    }
    ImGui::Separator();
    ImGui::Spacing();

    switch (sub_tab_) {
        case 0: render_monte_carlo(data); break;
        case 1: render_regression(data);  break;
        default: break;
    }

    ImGui::Spacing();
    StatusMessage(status_, status_time_);

    // Result display
    if (!result_.is_null()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::ADVANCED, "RESULTS");
        ImGui::Spacing();

        // Monte Carlo results
        if (result_.contains("mean_valuation"))
            DataRow("Mean Valuation", format_currency(result_["mean_valuation"].get<double>()).c_str(), theme::colors::SUCCESS);
        if (result_.contains("median_valuation"))
            DataRow("Median Valuation", format_currency(result_["median_valuation"].get<double>()).c_str(), theme::colors::TEXT_PRIMARY);
        if (result_.contains("std_dev"))
            DataRow("Std Deviation", format_currency(result_["std_dev"].get<double>()).c_str(), theme::colors::TEXT_SECONDARY);

        // Confidence intervals
        if (result_.contains("confidence_intervals") && result_["confidence_intervals"].is_object()) {
            ImGui::Spacing();
            ImGui::TextColored(theme::colors::TEXT_DIM, "Confidence Intervals");
            for (auto& [k, v] : result_["confidence_intervals"].items()) {
                if (v.is_array() && v.size() == 2) {
                    std::string range = format_currency(v[0].get<double>()) + " - " +
                                        format_currency(v[1].get<double>());
                    DataRow(k.c_str(), range.c_str(), theme::colors::TEXT_SECONDARY);
                } else if (v.is_object()) {
                    if (v.contains("low") && v.contains("high")) {
                        std::string range = format_currency(v["low"].get<double>()) + " - " +
                                            format_currency(v["high"].get<double>());
                        DataRow(k.c_str(), range.c_str(), theme::colors::TEXT_SECONDARY);
                    }
                }
            }
        }

        // Percentiles
        if (result_.contains("percentiles") && result_["percentiles"].is_object()) {
            ImGui::Spacing();
            ImGui::TextColored(theme::colors::TEXT_DIM, "Percentiles");
            for (auto& [k, v] : result_["percentiles"].items()) {
                if (v.is_number())
                    DataRow(k.c_str(), format_currency(v.get<double>()).c_str(), theme::colors::TEXT_SECONDARY);
            }
        }

        // Regression results
        if (result_.contains("r_squared")) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.4f", result_["r_squared"].get<double>());
            DataRow("R-Squared", buf, theme::colors::TEXT_PRIMARY);
        }
        if (result_.contains("adjusted_r_squared")) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.4f", result_["adjusted_r_squared"].get<double>());
            DataRow("Adjusted R²", buf, theme::colors::TEXT_PRIMARY);
        }
        if (result_.contains("predicted_valuation"))
            DataRow("Predicted Valuation", format_currency(result_["predicted_valuation"].get<double>()).c_str(), theme::colors::SUCCESS);
        if (result_.contains("coefficients") && result_["coefficients"].is_object()) {
            ImGui::Spacing();
            ImGui::TextColored(theme::colors::TEXT_DIM, "Coefficients");
            for (auto& [k, v] : result_["coefficients"].items()) {
                if (v.is_number()) {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "%.4f", v.get<double>());
                    DataRow(k.c_str(), buf, theme::colors::TEXT_SECONDARY);
                }
            }
        }

        // Simulation count
        if (result_.contains("num_simulations")) {
            DataRow("Simulations", std::to_string(result_["num_simulations"].get<int>()).c_str(), theme::colors::TEXT_DIM);
        }
    }
}

// =============================================================================
// Monte Carlo Simulation
// =============================================================================
void AdvancedPanel::render_monte_carlo(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("MONTE CARLO INPUTS", &open, ma_colors::ADVANCED)) {
        ImGui::Spacing();
        InputRow("Base Valuation ($M)",  &mc_base_,        "%.0f");
        InputRow("Growth Mean (%)",      &mc_growth_mean_,  "%.1f");
        InputRow("Growth Std Dev (%)",   &mc_growth_std_,   "%.1f");
        InputRow("Margin Mean (%)",      &mc_margin_mean_,  "%.1f");
        InputRow("Margin Std Dev (%)",   &mc_margin_std_,   "%.1f");
        InputRow("Discount Rate (%)",    &mc_discount_,     "%.1f");
        InputRowInt("Simulations",       &mc_sims_);

        ImGui::TextColored(theme::colors::TEXT_DIM, "(1,000 - 100,000 recommended)");
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("RUN MONTE CARLO", data.is_loading(), ma_colors::ADVANCED)) {
        // Clamp simulations
        if (mc_sims_ < 100) mc_sims_ = 100;
        if (mc_sims_ > 100000) mc_sims_ = 100000;

        json params = {
            {"base_valuation",  mc_base_ * 1e6},
            {"growth_mean",     mc_growth_mean_ / 100.0},
            {"growth_std",      mc_growth_std_ / 100.0},
            {"margin_mean",     mc_margin_mean_ / 100.0},
            {"margin_std",      mc_margin_std_ / 100.0},
            {"discount_rate",   mc_discount_ / 100.0},
            {"num_simulations", mc_sims_}
        };

        data.run_analysis("advanced_analytics/monte_carlo_valuation.py", {
            "monte_carlo", params.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Monte Carlo simulation complete";
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
// Regression Analysis
// =============================================================================
void AdvancedPanel::render_regression(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("REGRESSION INPUTS", &open, ma_colors::ADVANCED)) {
        ImGui::Spacing();

        const char* types[] = {"OLS (Single)", "Multiple Regression"};
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Type");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(160);
        ImGui::Combo("##reg_type", &reg_type_, types, 2);

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::ADVANCED, "Subject Company");
        ImGui::Spacing();
        InputRow("Revenue ($M)",    &reg_subject_rev_,    "%.0f");
        InputRow("EBITDA ($M)",     &reg_subject_ebitda_, "%.0f");
        InputRow("Growth Rate (%)", &reg_subject_growth_, "%.1f");
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("RUN REGRESSION", data.is_loading(), ma_colors::ADVANCED)) {
        const char* type_names[] = {"ols", "multiple"};
        json params = {
            {"regression_type",  type_names[reg_type_]},
            {"subject_revenue",  reg_subject_rev_},
            {"subject_ebitda",   reg_subject_ebitda_},
            {"subject_growth",   reg_subject_growth_ / 100.0}
        };

        data.run_analysis("advanced_analytics/regression_analysis.py", {
            "regression", params.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Regression analysis complete";
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
