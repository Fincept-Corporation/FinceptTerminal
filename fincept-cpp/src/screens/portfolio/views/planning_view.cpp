// Planning Panel — Asset allocation, retirement planning
// Port of PlanningPanel.tsx

#include "planning_view.h"
#include "python/python_runner.h"
#include "storage/cache_service.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <thread>

namespace fincept::portfolio {

void PlanningView::render(const PortfolioSummary& summary) {
    if (loaded_for_portfolio_ != summary.portfolio.id) {
        allocation_loaded_ = false;
        loaded_for_portfolio_ = summary.portfolio.id;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char* tabs[] = {"Asset Allocation", "Retirement Planner", "Goals"};
    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine();
        bool active = (sub_tab_ == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT_BG);
        if (ImGui::Button(tabs[i], ImVec2(140, 0))) sub_tab_ = i;
        if (active) ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
    ImGui::Separator();

    if (!error_.empty()) {
        ImGui::TextColored(theme::colors::MARKET_RED, "[Error] %s", error_.c_str());
        ImGui::Spacing();
    }

    if (sub_tab_ == 0) {
        // Asset allocation from current holdings
        if (!allocation_loaded_ && !loading_) {
            if (theme::AccentButton("Analyze Allocation")) {
                fetch_allocation(summary);
            }
            ImGui::SameLine();
            ImGui::TextColored(theme::colors::TEXT_DIM, "Analyze current allocation vs targets");
        } else if (loading_) {
            theme::LoadingSpinner("Analyzing...");
        } else {
            theme::SectionHeader("Current Allocation by Asset Type");

            // Show allocation breakdown from holdings
            if (ImGui::BeginTable("##alloc", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Symbol");
                ImGui::TableSetupColumn("Weight");
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Bar");
                ImGui::TableHeadersRow();

                for (const auto& h : summary.holdings) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(h.asset.symbol.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("%.1f%%", h.weight);
                    ImGui::TableNextColumn(); ImGui::Text("$%.2f", h.market_value);
                    ImGui::TableNextColumn();
                    // Progress bar
                    float frac = static_cast<float>(h.weight / 100.0);
                    ImGui::ProgressBar(frac, ImVec2(-1, 14), "");
                }
                ImGui::EndTable();
            }

            if (allocation_result_.is_object()) {
                ImGui::Spacing();
                theme::SectionHeader("Allocation Analysis");
                if (ImGui::BeginTable("##alloc_analysis", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 250);
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableHeadersRow();
                    for (auto& [k, v] : allocation_result_.items()) {
                        if (v.is_object() || v.is_array()) continue;
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", k.c_str());
                        ImGui::TableNextColumn();
                        if (v.is_number()) ImGui::Text("%.4f", v.get<double>());
                        else ImGui::TextUnformatted(v.dump().c_str());
                    }
                    ImGui::EndTable();
                }
            }
        }

    } else if (sub_tab_ == 1) {
        // Retirement planner — client-side calculation
        theme::SectionHeader("Retirement Planner");

        ImGui::TextColored(theme::colors::TEXT_DIM, "Current Age"); ImGui::SameLine(200);
        ImGui::SetNextItemWidth(100); ImGui::InputText("##cur_age", current_age_, sizeof(current_age_));

        ImGui::TextColored(theme::colors::TEXT_DIM, "Retirement Age"); ImGui::SameLine(200);
        ImGui::SetNextItemWidth(100); ImGui::InputText("##ret_age", retire_age_, sizeof(retire_age_));

        ImGui::TextColored(theme::colors::TEXT_DIM, "Annual Contribution ($)"); ImGui::SameLine(200);
        ImGui::SetNextItemWidth(100); ImGui::InputText("##annual", annual_contribution_, sizeof(annual_contribution_));

        ImGui::TextColored(theme::colors::TEXT_DIM, "Expected Return (%%)"); ImGui::SameLine(200);
        ImGui::SetNextItemWidth(100); ImGui::InputText("##exp_ret", expected_return_, sizeof(expected_return_));

        ImGui::TextColored(theme::colors::TEXT_DIM, "Inflation (%%)"); ImGui::SameLine(200);
        ImGui::SetNextItemWidth(100); ImGui::InputText("##infl", inflation_, sizeof(inflation_));

        ImGui::Spacing();
        if (theme::AccentButton("Calculate")) {
            int cur = std::atoi(current_age_);
            int ret = std::atoi(retire_age_);
            double annual = std::atof(annual_contribution_);
            double rate = std::atof(expected_return_) / 100.0;
            double infl = std::atof(inflation_) / 100.0;
            double real_rate = rate - infl;
            int years = ret - cur;

            if (years > 0 && annual > 0) {
                double current_portfolio = summary.total_market_value;
                double fv_portfolio = current_portfolio * std::pow(1 + real_rate, years);
                double fv_contributions = annual * ((std::pow(1 + real_rate, years) - 1) / real_rate);
                double total = fv_portfolio + fv_contributions;

                retirement_result_ = nlohmann::json{
                    {"years_to_retirement", years},
                    {"current_portfolio_value", current_portfolio},
                    {"future_portfolio_value", fv_portfolio},
                    {"future_contributions", fv_contributions},
                    {"total_retirement_value", total},
                    {"real_return_rate", real_rate * 100},
                    {"monthly_income_4pct_rule", total * 0.04 / 12}
                };
                retirement_loaded_ = true;
            }
        }

        if (retirement_loaded_ && retirement_result_.is_object()) {
            ImGui::Spacing();
            theme::SectionHeader("Retirement Projection");
            if (ImGui::BeginTable("##retire", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 250);
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();

                auto row = [](const char* label, const char* value) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "%s", label);
                    ImGui::TableNextColumn(); ImGui::Text("%s", value);
                };

                char buf[64];
                std::snprintf(buf, sizeof(buf), "%d years", retirement_result_["years_to_retirement"].get<int>());
                row("Years to Retirement", buf);
                std::snprintf(buf, sizeof(buf), "$%.2f", retirement_result_["current_portfolio_value"].get<double>());
                row("Current Portfolio", buf);
                std::snprintf(buf, sizeof(buf), "$%.2f", retirement_result_["future_portfolio_value"].get<double>());
                row("Portfolio Growth", buf);
                std::snprintf(buf, sizeof(buf), "$%.2f", retirement_result_["future_contributions"].get<double>());
                row("Contribution Growth", buf);
                std::snprintf(buf, sizeof(buf), "$%.2f", retirement_result_["total_retirement_value"].get<double>());
                row("Total at Retirement", buf);
                std::snprintf(buf, sizeof(buf), "$%.2f/mo", retirement_result_["monthly_income_4pct_rule"].get<double>());
                row("Monthly Income (4% Rule)", buf);

                ImGui::EndTable();
            }
        }

    } else {
        // Goals — placeholder
        theme::SectionHeader("Financial Goals");
        ImGui::TextColored(theme::colors::TEXT_DIM, "Set financial goals and track progress — coming soon.");
    }
}

void PlanningView::fetch_allocation(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    nlohmann::json holdings = nlohmann::json::array();
    for (const auto& h : summary.holdings) {
        holdings.push_back({
            {"symbol", h.asset.symbol},
            {"weight", h.weight / 100.0},
            {"value", h.market_value}
        });
    }
    std::string sym_key;
    for (const auto& h : summary.holdings) sym_key += h.asset.symbol + "_";

    std::thread([this, holdings, sym_key]() {
        try {
            nlohmann::json req;
            req["holdings"] = holdings;

            auto& cache = CacheService::instance();
            std::string ck = CacheService::make_key("reference", "allocation-analysis",
                sym_key.substr(0, std::min((size_t)64, sym_key.size())));

            std::string output;
            auto cached = cache.get(ck);
            if (cached && !cached->empty()) { output = *cached; }
            else {
                auto result = python::execute_with_stdin(
                    "Analytics/portfolioManagement/portfolio_analytics.py",
                    {"allocation_analysis"}, req.dump());
                if (result.success && !result.output.empty()) {
                    output = result.output;
                    auto check = nlohmann::json::parse(output, nullptr, false);
                    if (!check.is_discarded() && !check.contains("error"))
                        cache.set(ck, output, "reference", CacheTTL::FIFTEEN_MIN);
                } else if (!result.error.empty()) {
                    error_ = result.error;
                }
            }

            if (!output.empty()) {
                auto j = nlohmann::json::parse(output, nullptr, false);
                if (!j.is_discarded()) {
                    if (j.contains("error"))
                        error_ = j["error"].get<std::string>();
                    else
                        allocation_result_ = j;
                }
            } else if (error_.empty()) {
                error_ = "Script returned empty output";
            }
            allocation_loaded_ = true;
        } catch (const std::exception& e) {
            error_ = e.what();
            LOG_ERROR("PlanningView", "Allocation analysis failed: %s", e.what());
            allocation_loaded_ = true;
        }
        loading_ = false;
    }).detach();
}

void PlanningView::fetch_retirement() {
    // Client-side only — no Python needed
}

} // namespace fincept::portfolio
