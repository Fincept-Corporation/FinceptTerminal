#include "startup_panel.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstring>

namespace fincept::mna {

using json = nlohmann::json;

void StartupPanel::init_scenarios() {
    std::strncpy(scenarios_[0].name, "Bear Case", sizeof(scenarios_[0].name) - 1);
    scenarios_[0].probability = 0.25;
    scenarios_[0].exit_year = 5;
    scenarios_[0].exit_value = 5;
    std::strncpy(scenarios_[0].description, "Conservative exit", sizeof(scenarios_[0].description) - 1);

    std::strncpy(scenarios_[1].name, "Base Case", sizeof(scenarios_[1].name) - 1);
    scenarios_[1].probability = 0.50;
    scenarios_[1].exit_year = 5;
    scenarios_[1].exit_value = 20;
    std::strncpy(scenarios_[1].description, "Expected outcome", sizeof(scenarios_[1].description) - 1);

    std::strncpy(scenarios_[2].name, "Bull Case", sizeof(scenarios_[2].name) - 1);
    scenarios_[2].probability = 0.25;
    scenarios_[2].exit_year = 5;
    scenarios_[2].exit_value = 80;
    std::strncpy(scenarios_[2].description, "Upside scenario", sizeof(scenarios_[2].description) - 1);
}

// =============================================================================
// Main render
// =============================================================================
void StartupPanel::render(MNAData& data) {
    const char* tabs[] = {"Berkus", "Scorecard", "VC Method", "First Chicago", "Risk Factor", "Comprehensive"};
    for (int i = 0; i < 6; i++) {
        if (ColoredButton(tabs[i], ma_colors::STARTUP, method_ == i)) {
            method_ = i;
            result_ = json();
            status_.clear();
        }
        if (i < 5) ImGui::SameLine(0, 2);
    }
    ImGui::Separator();
    ImGui::Spacing();

    switch (method_) {
        case 0: render_berkus(data);          break;
        case 1: render_scorecard(data);       break;
        case 2: render_vc(data);              break;
        case 3: render_first_chicago(data);   break;
        case 4: render_risk_factor(data);     break;
        case 5: render_comprehensive(data);   break;
        default: break;
    }

    ImGui::Spacing();
    StatusMessage(status_, status_time_);

    // Generic result display
    if (!result_.is_null()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::STARTUP, "RESULTS");
        ImGui::Spacing();

        // Display valuation if present
        if (result_.contains("valuation")) {
            DataRow("Valuation", format_currency(result_["valuation"].get<double>()).c_str(), theme::colors::SUCCESS);
        }
        if (result_.contains("pre_money_valuation")) {
            DataRow("Pre-Money Valuation", format_currency(result_["pre_money_valuation"].get<double>()).c_str(), theme::colors::SUCCESS);
        }
        if (result_.contains("post_money_valuation")) {
            DataRow("Post-Money Valuation", format_currency(result_["post_money_valuation"].get<double>()).c_str(), theme::colors::TEXT_PRIMARY);
        }
        if (result_.contains("weighted_valuation")) {
            DataRow("Weighted Valuation", format_currency(result_["weighted_valuation"].get<double>()).c_str(), theme::colors::SUCCESS);
        }

        // Factor breakdown
        if (result_.contains("factors") && result_["factors"].is_object()) {
            ImGui::Spacing();
            ImGui::TextColored(theme::colors::TEXT_DIM, "Factor Breakdown");
            for (auto& [k, v] : result_["factors"].items()) {
                if (v.is_number()) {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "%.1f", v.get<double>());
                    DataRow(k.c_str(), buf, theme::colors::TEXT_SECONDARY);
                }
            }
        }

        // Scenario results
        if (result_.contains("scenarios") && result_["scenarios"].is_array()) {
            ImGui::Spacing();
            ImGui::TextColored(theme::colors::TEXT_DIM, "Scenarios");
            for (auto& s : result_["scenarios"]) {
                std::string label = s.value("name", "Scenario");
                std::string val = format_currency(s.value("valuation", s.value("exit_value", 0.0)));
                val += " (" + format_percent(s.value("probability", 0.0) * 100, 0) + ")";
                DataRow(label.c_str(), val.c_str(), theme::colors::TEXT_SECONDARY);
            }
        }

        // Other top-level numeric fields
        for (auto& [k, v] : result_.items()) {
            if (k == "valuation" || k == "pre_money_valuation" || k == "post_money_valuation"
                || k == "weighted_valuation" || k == "factors" || k == "scenarios") continue;

            if (v.is_number()) {
                bool is_pct = k.find("pct") != std::string::npos || k.find("rate") != std::string::npos
                           || k.find("irr") != std::string::npos;
                if (is_pct)
                    DataRow(k.c_str(), format_percent(v.get<double>()).c_str(), theme::colors::TEXT_PRIMARY);
                else {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "%.2f", v.get<double>());
                    DataRow(k.c_str(), buf, theme::colors::TEXT_PRIMARY);
                }
            } else if (v.is_string()) {
                DataRow(k.c_str(), v.get<std::string>().c_str(), theme::colors::TEXT_SECONDARY);
            }
        }
    }
}

// =============================================================================
// Berkus Method
// =============================================================================
void StartupPanel::render_berkus(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("BERKUS FACTORS (0-100 scale)", &open, ma_colors::STARTUP)) {
        ImGui::Spacing();
        InputRow("Sound Idea",          &berkus_idea_,       "%.0f");
        InputRow("Prototype",           &berkus_proto_,      "%.0f");
        InputRow("Management Team",     &berkus_team_,       "%.0f");
        InputRow("Strategic Relations", &berkus_strategic_,   "%.0f");
        InputRow("Product Sales",       &berkus_sales_,      "%.0f");
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("CALCULATE BERKUS", data.is_loading(), ma_colors::STARTUP)) {
        json factors = {
            {"sound_idea",          berkus_idea_ / 100.0},
            {"prototype",           berkus_proto_ / 100.0},
            {"management_team",     berkus_team_ / 100.0},
            {"strategic_relations", berkus_strategic_ / 100.0},
            {"product_sales",       berkus_sales_ / 100.0}
        };
        data.run_analysis("startup_valuation/berkus_method.py", {
            "berkus", factors.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Berkus valuation complete";
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
// Scorecard Method
// =============================================================================
void StartupPanel::render_scorecard(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("SCORECARD INPUTS", &open, ma_colors::STARTUP)) {
        ImGui::Spacing();
        const char* stages[] = {"Seed", "Pre-Seed", "Series A"};
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Stage");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(120);
        ImGui::Combo("##sc_stage", &sc_stage_, stages, 3);

        const char* regions[] = {"US", "Europe", "Asia"};
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Region");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(120);
        ImGui::Combo("##sc_region", &sc_region_, regions, 3);

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::STARTUP, "Factor Multipliers (0.5 - 1.5)");
        ImGui::Spacing();
        InputRow("Team",          &sc_team_,        "%.2f");
        InputRow("Market Size",   &sc_market_,      "%.2f");
        InputRow("Product/Tech",  &sc_product_,     "%.2f");
        InputRow("Competitive",   &sc_competitive_, "%.2f");
        InputRow("Marketing",     &sc_marketing_,   "%.2f");
        InputRow("Need for Funding", &sc_funding_,  "%.2f");
        InputRow("Other Factors", &sc_other_,       "%.2f");
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("CALCULATE SCORECARD", data.is_loading(), ma_colors::STARTUP)) {
        const char* stage_names[] = {"seed", "pre_seed", "series_a"};
        const char* region_names[] = {"us", "europe", "asia"};
        json factors = {
            {"team",          sc_team_},
            {"market",        sc_market_},
            {"product",       sc_product_},
            {"competitive",   sc_competitive_},
            {"marketing",     sc_marketing_},
            {"funding_need",  sc_funding_},
            {"other",         sc_other_}
        };
        data.run_analysis("startup_valuation/scorecard_method.py", {
            "scorecard",
            stage_names[sc_stage_],
            region_names[sc_region_],
            factors.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Scorecard valuation complete";
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
// VC Method
// =============================================================================
void StartupPanel::render_vc(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("VC METHOD INPUTS", &open, ma_colors::STARTUP)) {
        ImGui::Spacing();
        InputRow("Exit Year Metric ($M)", &vc_exit_metric_,   "%.1f");
        InputRow("Exit Multiple",         &vc_exit_multiple_,  "%.1f");
        InputRowInt("Years to Exit",      &vc_years_);
        InputRow("Investment ($M)",       &vc_investment_,     "%.1f");

        const char* vc_stages[] = {"Seed", "Series A", "Series B", "Growth"};
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Stage");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(120);
        ImGui::Combo("##vc_stage", &vc_stage_, vc_stages, 4);
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("CALCULATE VC", data.is_loading(), ma_colors::STARTUP)) {
        const char* stage_names[] = {"seed", "series_a", "series_b", "growth"};
        data.run_analysis("startup_valuation/vc_method.py", {
            "vc_method",
            std::to_string(vc_exit_metric_),
            std::to_string(vc_exit_multiple_),
            std::to_string(vc_years_),
            std::to_string(vc_investment_),
            stage_names[vc_stage_]
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "VC valuation complete";
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
// First Chicago
// =============================================================================
void StartupPanel::render_first_chicago(MNAData& data) {
    // Initialize scenarios on first call
    static bool inited = false;
    if (!inited) { init_scenarios(); inited = true; }

    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("FIRST CHICAGO SCENARIOS", &open, ma_colors::STARTUP)) {
        ImGui::Spacing();
        for (int i = 0; i < 3; i++) {
            char hdr[64];
            std::snprintf(hdr, sizeof(hdr), "Scenario %d: %s", i + 1, scenarios_[i].name);
            ImGui::TextColored(ma_colors::STARTUP, "%s", hdr);

            char lbl[64];
            std::snprintf(lbl, sizeof(lbl), "Probability##fc_%d", i);
            InputRow(lbl, &scenarios_[i].probability, "%.2f");
            std::snprintf(lbl, sizeof(lbl), "Exit Year##fc_%d", i);
            InputRowInt(lbl, &scenarios_[i].exit_year);
            std::snprintf(lbl, sizeof(lbl), "Exit Value ($M)##fc_%d", i);
            InputRow(lbl, &scenarios_[i].exit_value, "%.1f");
            ImGui::Spacing();
        }
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("CALCULATE FIRST CHICAGO", false, ma_colors::STARTUP)) {
        // Local calculation — weighted average
        double weighted_val = 0;
        json sc_results = json::array();
        for (int i = 0; i < 3; i++) {
            weighted_val += scenarios_[i].probability * scenarios_[i].exit_value;
            sc_results.push_back({
                {"name", scenarios_[i].name},
                {"probability", scenarios_[i].probability},
                {"exit_year", scenarios_[i].exit_year},
                {"exit_value", scenarios_[i].exit_value},
                {"valuation", scenarios_[i].probability * scenarios_[i].exit_value}
            });
        }

        result_ = json{
            {"weighted_valuation", weighted_val},
            {"scenarios", sc_results},
            {"method", "First Chicago"}
        };
        status_ = "First Chicago valuation complete";
        status_time_ = ImGui::GetTime();
    }
}

// =============================================================================
// Risk Factor Summation
// =============================================================================
void StartupPanel::render_risk_factor(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("RISK FACTOR INPUTS", &open, ma_colors::STARTUP)) {
        ImGui::Spacing();
        InputRow("Base Valuation ($M)", &rf_base_, "%.1f");
        ImGui::Spacing();

        const char* rf_names[] = {
            "Management", "Stage of Business", "Legislation/Political",
            "Manufacturing", "Sales/Marketing", "Funding/Capital",
            "Competition", "Technology", "Litigation",
            "International", "Reputation", "Exit Potential"
        };
        ImGui::TextColored(ma_colors::STARTUP, "Risk Scores (-2 to +2)");
        ImGui::Spacing();

        for (int i = 0; i < 12; i++) {
            char lbl[64];
            std::snprintf(lbl, sizeof(lbl), "%s##rf_%d", rf_names[i], i);
            ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "%s", rf_names[i]);
            ImGui::SameLine(160);
            ImGui::SetNextItemWidth(80);
            char id[32];
            std::snprintf(id, sizeof(id), "##rf_%d", i);
            ImGui::SliderInt(id, &rf_scores_[i], -2, 2);
        }
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("CALCULATE RISK FACTOR", false, ma_colors::STARTUP)) {
        // Local calc: each +1 = +$250K, each -1 = -$250K from base
        double adjustment = 0;
        json factors = json::object();
        const char* rf_names[] = {
            "management", "stage", "legislation", "manufacturing",
            "sales_marketing", "funding", "competition", "technology",
            "litigation", "international", "reputation", "exit_potential"
        };
        for (int i = 0; i < 12; i++) {
            adjustment += rf_scores_[i] * 0.25;  // $250K per point
            factors[rf_names[i]] = rf_scores_[i];
        }

        double valuation = rf_base_ + adjustment;
        if (valuation < 0) valuation = 0;

        result_ = json{
            {"valuation", valuation},
            {"base_valuation", rf_base_},
            {"adjustment", adjustment},
            {"factors", factors},
            {"method", "Risk Factor Summation"}
        };
        status_ = "Risk factor valuation complete";
        status_time_ = ImGui::GetTime();
    }
}

// =============================================================================
// Comprehensive (all methods combined)
// =============================================================================
void StartupPanel::render_comprehensive(MNAData& data) {
    using namespace theme::colors;

    ImGui::TextColored(ma_colors::STARTUP, "COMPREHENSIVE STARTUP VALUATION");
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM,
        "Runs all available methods and provides a weighted consensus. "
        "Configure individual method inputs in their respective tabs first.");
    ImGui::Spacing();

    if (RunButton("RUN ALL METHODS", data.is_loading(), ma_colors::STARTUP)) {
        json params = {
            {"idea_quality",      berkus_idea_ / 100.0},
            {"team_quality",      berkus_team_ / 100.0},
            {"prototype_status",  berkus_proto_ / 100.0},
            {"market_size",       berkus_strategic_ / 100.0}
        };
        data.run_analysis("startup_valuation/startup_summary.py", {
            "quick_pre_revenue",
            std::to_string(berkus_idea_ / 100.0),
            std::to_string(berkus_team_ / 100.0),
            std::to_string(berkus_proto_ / 100.0),
            std::to_string(berkus_strategic_ / 100.0)
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Comprehensive valuation complete";
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
