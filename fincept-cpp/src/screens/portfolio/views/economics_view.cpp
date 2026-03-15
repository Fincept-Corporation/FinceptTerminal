// Economics Panel — Business cycle indicators, Equity Risk Premium
// Port of EconomicsPanel.tsx

#include "economics_view.h"
#include "python/python_runner.h"
#include "storage/cache_service.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <thread>

namespace fincept::portfolio {

void EconomicsView::render(const PortfolioSummary& /*summary*/) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char* tabs[] = {"Business Cycle", "Equity Risk Premium"};
    for (int i = 0; i < 2; i++) {
        if (i > 0) ImGui::SameLine();
        bool active = (sub_tab_ == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT_BG);
        if (ImGui::Button(tabs[i], ImVec2(150, 0))) sub_tab_ = i;
        if (active) ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 20);
    if (loading_) {
        theme::LoadingSpinner("Fetching...");
    } else {
        if (theme::AccentButton("Run")) {
            if (sub_tab_ == 0) fetch_business_cycle();
            else fetch_erp();
        }
    }

    ImGui::Separator();

    if (!error_.empty()) {
        ImGui::TextColored(theme::colors::MARKET_RED, "[Error] %s", error_.c_str());
        ImGui::Spacing();
    }

    if (sub_tab_ == 0) {
        if (!cycle_loaded_) {
            ImGui::TextColored(theme::colors::TEXT_DIM,
                "Click Run to fetch business cycle indicators (GDP, unemployment, inflation, etc.)");
        } else if (cycle_result_.is_object()) {
            theme::SectionHeader("Business Cycle Indicators");
            if (ImGui::BeginTable("##cycle", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Indicator", ImGuiTableColumnFlags_WidthFixed, 280);
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();
                for (auto& [k, v] : cycle_result_.items()) {
                    if (v.is_object() || v.is_array()) continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", k.c_str());
                    ImGui::TableNextColumn();
                    if (v.is_number()) ImGui::Text("%.4f", v.get<double>());
                    else if (v.is_string()) ImGui::TextUnformatted(v.get<std::string>().c_str());
                    else ImGui::TextUnformatted(v.dump().c_str());
                }
                ImGui::EndTable();
            }
        }
    } else {
        if (!erp_loaded_) {
            ImGui::TextColored(theme::colors::TEXT_DIM,
                "Click Run to compute equity risk premium estimates");
        } else if (erp_result_.is_object()) {
            theme::SectionHeader("Equity Risk Premium");
            if (ImGui::BeginTable("##erp", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthFixed, 280);
                ImGui::TableSetupColumn("ERP Estimate");
                ImGui::TableHeadersRow();
                for (auto& [k, v] : erp_result_.items()) {
                    if (v.is_object() || v.is_array()) continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", k.c_str());
                    ImGui::TableNextColumn();
                    if (v.is_number()) {
                        double val = v.get<double>();
                        ImVec4 col = val > 0.05 ? theme::colors::MARKET_GREEN : theme::colors::ACCENT;
                        ImGui::TextColored(col, "%.2f%%", val * 100);
                    } else {
                        ImGui::TextUnformatted(v.dump().c_str());
                    }
                }
                ImGui::EndTable();
            }
        }
    }
}

void EconomicsView::fetch_business_cycle() {
    if (loading_) return;
    loading_ = true;
    error_.clear();
    std::thread([this]() {
        try {
            auto& cache = CacheService::instance();
            std::string ck = CacheService::make_key("economic", "business-cycle", "indicators");

            std::string output;
            auto cached = cache.get(ck);
            if (cached && !cached->empty()) { output = *cached; }
            else {
                auto result = python::execute_with_stdin(
                    "Analytics/economics/business_cycle.py",
                    {"get_indicators"}, "{}");
                if (result.success && !result.output.empty()) {
                    output = result.output;
                    auto check = nlohmann::json::parse(output, nullptr, false);
                    if (!check.is_discarded() && !check.contains("error"))
                        cache.set(ck, output, "economic", CacheTTL::FIFTEEN_MIN);
                } else if (!result.error.empty()) {
                    error_ = result.error;
                }
            }

            if (!output.empty()) {
                auto j = nlohmann::json::parse(output, nullptr, false);
                if (!j.is_discarded()) {
                    if (j.contains("error"))
                        error_ = j["error"].get<std::string>();
                    else { cycle_result_ = j; cycle_loaded_ = true; }
                }
            } else if (error_.empty()) {
                error_ = "Script returned empty output";
            }
        } catch (const std::exception& e) {
            error_ = e.what();
            LOG_ERROR("EconomicsView", "Business cycle fetch failed: %s", e.what());
        }
        loading_ = false;
    }).detach();
}

void EconomicsView::fetch_erp() {
    if (loading_) return;
    loading_ = true;
    error_.clear();
    std::thread([this]() {
        try {
            auto& cache = CacheService::instance();
            std::string ck = CacheService::make_key("economic", "equity-risk-premium", "erp");

            std::string output;
            auto cached = cache.get(ck);
            if (cached && !cached->empty()) { output = *cached; }
            else {
                auto result = python::execute_with_stdin(
                    "Analytics/economics/equity_risk_premium.py",
                    {"estimate_erp"}, "{}");
                if (result.success && !result.output.empty()) {
                    output = result.output;
                    auto check = nlohmann::json::parse(output, nullptr, false);
                    if (!check.is_discarded() && !check.contains("error"))
                        cache.set(ck, output, "economic", CacheTTL::FIFTEEN_MIN);
                } else if (!result.error.empty()) {
                    error_ = result.error;
                }
            }

            if (!output.empty()) {
                auto j = nlohmann::json::parse(output, nullptr, false);
                if (!j.is_discarded()) {
                    if (j.contains("error"))
                        error_ = j["error"].get<std::string>();
                    else { erp_result_ = j; erp_loaded_ = true; }
                }
            } else if (error_.empty()) {
                error_ = "Script returned empty output";
            }
        } catch (const std::exception& e) {
            error_ = e.what();
            LOG_ERROR("EconomicsView", "ERP fetch failed: %s", e.what());
        }
        loading_ = false;
    }).detach();
}

} // namespace fincept::portfolio
