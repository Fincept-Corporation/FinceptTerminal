#include "fairness_panel.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstring>
#include <sstream>

namespace fincept::mna {

using json = nlohmann::json;

// =============================================================================
// Main render
// =============================================================================
void FairnessPanel::render(MNAData& data) {
    const char* tabs[] = {"Fairness Analysis", "Premium Analysis", "Process Quality"};
    for (int i = 0; i < 3; i++) {
        if (ColoredButton(tabs[i], ma_colors::FAIRNESS, sub_tab_ == i)) {
            sub_tab_ = i;
            result_ = json();
            status_.clear();
        }
        if (i < 2) ImGui::SameLine(0, 2);
    }
    ImGui::Separator();
    ImGui::Spacing();

    switch (sub_tab_) {
        case 0: render_fairness(data); break;
        case 1: render_premium(data);  break;
        case 2: render_process(data);  break;
        default: break;
    }

    ImGui::Spacing();
    StatusMessage(status_, status_time_);

    // Result display
    if (!result_.is_null()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::FAIRNESS, "RESULTS");
        ImGui::Spacing();

        // Fairness conclusion
        if (result_.contains("conclusion")) {
            std::string concl = result_["conclusion"].get<std::string>();
            ImVec4 col = (concl.find("FAIR") != std::string::npos && concl.find("NOT") == std::string::npos)
                ? theme::colors::SUCCESS : ImVec4(0.96f, 0.30f, 0.30f, 1.0f);
            DataRow("Conclusion", concl.c_str(), col);
        }
        if (result_.contains("fairness_score")) {
            DataRow("Fairness Score", format_percent(result_["fairness_score"].get<double>()).c_str(), theme::colors::TEXT_PRIMARY);
        }

        // Premium results
        if (result_.contains("premium_1day"))
            DataRow("1-Day Premium", format_percent(result_["premium_1day"].get<double>()).c_str(), theme::colors::TEXT_PRIMARY);
        if (result_.contains("premium_1week"))
            DataRow("1-Week Premium", format_percent(result_["premium_1week"].get<double>()).c_str(), theme::colors::TEXT_PRIMARY);
        if (result_.contains("premium_4week"))
            DataRow("4-Week Premium", format_percent(result_["premium_4week"].get<double>()).c_str(), theme::colors::TEXT_PRIMARY);
        if (result_.contains("premium_52w_high"))
            DataRow("52W High Premium", format_percent(result_["premium_52w_high"].get<double>()).c_str(), theme::colors::TEXT_SECONDARY);

        // Process quality
        if (result_.contains("overall_score")) {
            double score = result_["overall_score"].get<double>();
            ImVec4 col = (score >= 4.0) ? theme::colors::SUCCESS :
                         (score >= 3.0) ? ImVec4(1.0f, 0.77f, 0.0f, 1.0f) :
                                          ImVec4(0.96f, 0.30f, 0.30f, 1.0f);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.1f / 5.0", score);
            DataRow("Overall Process Score", buf, col);
        }
        if (result_.contains("grade")) {
            DataRow("Grade", result_["grade"].get<std::string>().c_str(), theme::colors::TEXT_PRIMARY);
        }

        // Valuation ranges
        if (result_.contains("valuation_range")) {
            auto& vr = result_["valuation_range"];
            if (vr.contains("low") && vr.contains("high")) {
                std::string range = format_currency(vr["low"].get<double>()) + " - " +
                                    format_currency(vr["high"].get<double>());
                DataRow("Valuation Range", range.c_str(), theme::colors::TEXT_SECONDARY);
            }
        }

        // Category scores
        if (result_.contains("category_scores") && result_["category_scores"].is_object()) {
            ImGui::Spacing();
            ImGui::TextColored(theme::colors::TEXT_DIM, "Category Scores");
            for (auto& [k, v] : result_["category_scores"].items()) {
                if (v.is_number()) {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.1f", v.get<double>());
                    DataRow(k.c_str(), buf, theme::colors::TEXT_SECONDARY);
                }
            }
        }

        // Generic remaining fields
        for (auto& [k, v] : result_.items()) {
            if (k == "conclusion" || k == "fairness_score" || k == "overall_score"
                || k == "grade" || k == "valuation_range" || k == "category_scores"
                || k.find("premium") != std::string::npos) continue;

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
// Fairness Analysis
// =============================================================================
void FairnessPanel::render_fairness(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("FAIRNESS OPINION INPUTS", &open, ma_colors::FAIRNESS)) {
        ImGui::Spacing();
        InputRow("Offer Price ($)", &fo_offer_, "%.2f");

        ImGui::Spacing();
        ImGui::TextColored(ma_colors::FAIRNESS, "Valuation Methods");
        ImGui::Spacing();

        for (int i = 0; i < 3; i++) {
            char lbl[64];
            ImGui::TextColored(theme::colors::TEXT_DIM, "%s", fo_methods_[i].method.c_str());
            std::snprintf(lbl, sizeof(lbl), "Value##fo_%d", i);
            InputRow(lbl, &fo_methods_[i].valuation, "%.2f");
            std::snprintf(lbl, sizeof(lbl), "Low##fo_%d", i);
            InputRow(lbl, &fo_methods_[i].range_low, "%.2f");
            std::snprintf(lbl, sizeof(lbl), "High##fo_%d", i);
            InputRow(lbl, &fo_methods_[i].range_high, "%.2f");
            ImGui::Spacing();
        }
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("ANALYZE FAIRNESS", data.is_loading(), ma_colors::FAIRNESS)) {
        json methods = json::array();
        for (int i = 0; i < 3; i++) {
            methods.push_back({
                {"method", fo_methods_[i].method},
                {"valuation", fo_methods_[i].valuation},
                {"range_low", fo_methods_[i].range_low},
                {"range_high", fo_methods_[i].range_high}
            });
        }
        json qual_factors = {
            {"board_independence", 4},
            {"special_committee", 4},
            {"advisor_quality", 3},
            {"market_check", 3}
        };

        data.run_analysis("fairness_opinion/valuation_framework.py", {
            "fairness",
            methods.dump(),
            std::to_string(fo_offer_),
            qual_factors.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Fairness analysis complete";
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
// Premium Analysis
// =============================================================================
void FairnessPanel::render_premium(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("PREMIUM ANALYSIS INPUTS", &open, ma_colors::FAIRNESS)) {
        ImGui::Spacing();
        InputRow("Offer Price ($)", &pm_offer_, "%.2f");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Historical Prices (CSV)");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##pm_prices", pm_prices_, sizeof(pm_prices_));
        ImGui::TextColored(theme::colors::TEXT_DIM, "Enter comma-separated daily closing prices");
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("ANALYZE PREMIUM", data.is_loading(), ma_colors::FAIRNESS)) {
        // Parse CSV prices into JSON array
        json prices = json::array();
        std::istringstream ss(pm_prices_);
        std::string token;
        while (std::getline(ss, token, ',')) {
            try {
                double p = std::stod(token);
                prices.push_back(p);
            } catch (...) {}
        }

        data.run_analysis("fairness_opinion/premium_analysis.py", {
            "premium",
            prices.dump(),
            std::to_string(pm_offer_)
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Premium analysis complete";
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
// Process Quality Assessment
// =============================================================================
void FairnessPanel::render_process(MNAData& data) {
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("PROCESS QUALITY FACTORS (1-5)", &open, ma_colors::FAIRNESS)) {
        ImGui::Spacing();

        const char* factor_names[] = {
            "Board Independence", "Special Committee", "Financial Advisors",
            "Legal Counsel", "Market Check", "Negotiation Duration",
            "Shareholder Vote", "Disclosure Quality"
        };

        for (int i = 0; i < 8; i++) {
            ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "%s", factor_names[i]);
            ImGui::SameLine(160);
            ImGui::SetNextItemWidth(80);
            char id[32];
            std::snprintf(id, sizeof(id), "##pq_%d", i);
            ImGui::SliderInt(id, &pq_scores_[i], 1, 5);
        }
    }
    inputs_collapsed_ = !open;
    ImGui::Spacing();

    if (RunButton("ASSESS PROCESS", data.is_loading(), ma_colors::FAIRNESS)) {
        const char* factor_keys[] = {
            "board_independence", "special_committee", "financial_advisors",
            "legal_counsel", "market_check", "negotiation_duration",
            "shareholder_vote", "disclosure_quality"
        };
        json factors = json::object();
        for (int i = 0; i < 8; i++) {
            factors[factor_keys[i]] = pq_scores_[i];
        }

        data.run_analysis("fairness_opinion/process_quality.py", {
            "process", factors.dump()
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        result_ = data.result();
        status_ = "Process assessment complete";
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
