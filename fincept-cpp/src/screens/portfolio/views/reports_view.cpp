// Reports & PME Panel — Port of ReportsPMEPanel.tsx
// Sub-tabs: Tax Report | PME Analysis

#include "reports_view.h"
#include "python/python_runner.h"
#include "storage/cache_service.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <thread>

namespace fincept::portfolio {

void ReportsView::render(const PortfolioSummary& summary) {
    if (loaded_for_portfolio_ != summary.portfolio.id) {
        tax_loaded_ = pme_loaded_ = false;
        loaded_for_portfolio_ = summary.portfolio.id;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char* tabs[] = {"Tax Report", "PME Analysis"};
    for (int i = 0; i < 2; i++) {
        if (i > 0) ImGui::SameLine();
        bool active = (sub_tab_ == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT_BG);
        if (ImGui::Button(tabs[i], ImVec2(130, 0))) sub_tab_ = i;
        if (active) ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 20);
    if (loading_) {
        theme::LoadingSpinner("Computing...");
    } else {
        if (theme::AccentButton("Run")) {
            if (sub_tab_ == 0) fetch_tax_report(summary);
            else fetch_pme(summary);
        }
    }

    ImGui::Separator();

    if (!error_.empty()) {
        ImGui::TextColored(theme::colors::MARKET_RED, "[Error] %s", error_.c_str());
        ImGui::Spacing();
    }

    if (sub_tab_ == 0) {
        if (!tax_loaded_) {
            ImGui::TextColored(theme::colors::TEXT_DIM,
                "Click Run to generate tax report (capital gains, wash sales, tax-loss harvesting)");
        } else if (tax_result_.is_object()) {
            theme::SectionHeader("Capital Gains Summary");
            if (ImGui::BeginTable("##tax", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthFixed, 250);
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();
                for (auto& [k, v] : tax_result_.items()) {
                    if (v.is_object() || v.is_array()) continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", k.c_str());
                    ImGui::TableNextColumn();
                    if (v.is_number())
                        ImGui::Text("%.2f", v.get<double>());
                    else if (v.is_string())
                        ImGui::TextUnformatted(v.get<std::string>().c_str());
                    else
                        ImGui::TextUnformatted(v.dump().c_str());
                }
                ImGui::EndTable();
            }
        }
    } else {
        if (!pme_loaded_) {
            ImGui::TextColored(theme::colors::TEXT_DIM,
                "Click Run to compute Public Market Equivalent (PME) analysis via pypme");
        } else if (pme_result_.is_object()) {
            theme::SectionHeader("PME Analysis Results");
            if (ImGui::BeginTable("##pme", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 250);
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();
                for (auto& [k, v] : pme_result_.items()) {
                    if (v.is_object() || v.is_array()) continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", k.c_str());
                    ImGui::TableNextColumn();
                    if (v.is_number())
                        ImGui::Text("%.4f", v.get<double>());
                    else
                        ImGui::TextUnformatted(v.dump().c_str());
                }
                ImGui::EndTable();
            }
        }
    }
}

void ReportsView::fetch_tax_report(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    nlohmann::json txns = nlohmann::json::array();
    for (const auto& h : summary.holdings) {
        txns.push_back({
            {"symbol", h.asset.symbol},
            {"quantity", h.asset.quantity},
            {"avg_price", h.asset.avg_buy_price},
            {"current_price", h.current_price}
        });
    }
    std::string pf_name = summary.portfolio.name;
    std::string sym_key;
    for (const auto& h : summary.holdings) sym_key += h.asset.symbol + "_";

    std::thread([this, txns, pf_name, sym_key]() {
        try {
            nlohmann::json req;
            req["holdings"] = txns;
            req["portfolio_name"] = pf_name;

            auto& cache = CacheService::instance();
            std::string ck = CacheService::make_key("reference", "tax-report",
                sym_key.substr(0, std::min((size_t)64, sym_key.size())));

            std::string output;
            auto cached = cache.get(ck);
            if (cached && !cached->empty()) { output = *cached; }
            else {
                auto result = python::execute_with_stdin(
                    "Analytics/portfolioManagement/portfolio_analytics.py",
                    {"tax_report"}, req.dump());
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
                    else { tax_result_ = j; tax_loaded_ = true; }
                }
            } else if (error_.empty()) {
                error_ = "Script returned empty output";
            }
        } catch (const std::exception& e) {
            error_ = e.what();
            LOG_ERROR("ReportsView", "Tax report failed: %s", e.what());
        }
        loading_ = false;
    }).detach();
}

void ReportsView::fetch_pme(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    std::vector<std::string> symbols;
    for (const auto& h : summary.holdings)
        symbols.push_back(h.asset.symbol);
    std::string pf_name = summary.portfolio.name;

    std::thread([this, symbols, pf_name]() {
        try {
            nlohmann::json req;
            req["symbols"] = symbols;
            req["portfolio_name"] = pf_name;

            auto& cache_p = CacheService::instance();
            std::string psym;
            for (const auto& s : symbols) psym += s + "_";
            std::string pck = CacheService::make_key("reference", "pme-analysis",
                psym.substr(0, std::min((size_t)64, psym.size())));

            std::string p_out;
            auto cached_p = cache_p.get(pck);
            if (cached_p && !cached_p->empty()) { p_out = *cached_p; }
            else {
                auto result = python::execute_with_stdin(
                    "Analytics/portfolioManagement/portfolio_analytics.py",
                    {"pme_analysis"}, req.dump());
                if (result.success && !result.output.empty()) {
                    p_out = result.output;
                    auto check = nlohmann::json::parse(p_out, nullptr, false);
                    if (!check.is_discarded() && !check.contains("error"))
                        cache_p.set(pck, p_out, "reference", CacheTTL::FIFTEEN_MIN);
                } else if (!result.error.empty()) {
                    error_ = result.error;
                }
            }

            if (!p_out.empty()) {
                auto j = nlohmann::json::parse(p_out, nullptr, false);
                if (!j.is_discarded()) {
                    if (j.contains("error"))
                        error_ = j["error"].get<std::string>();
                    else { pme_result_ = j; pme_loaded_ = true; }
                }
            } else if (error_.empty()) {
                error_ = "Script returned empty output";
            }
        } catch (const std::exception& e) {
            error_ = e.what();
            LOG_ERROR("ReportsView", "PME analysis failed: %s", e.what());
        }
        loading_ = false;
    }).detach();
}

} // namespace fincept::portfolio
