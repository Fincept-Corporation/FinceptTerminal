#include "comparison_panel.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstring>

namespace fincept::mna {

using json = nlohmann::json;

void ComparisonPanel::init_default_deals() {
    if (deals_initialized_) return;
    deals_initialized_ = true;

    deals_.resize(3);
    std::strncpy(deals_[0].target, "TargetA", sizeof(deals_[0].target) - 1);
    std::strncpy(deals_[0].acquirer, "AcquirerA", sizeof(deals_[0].acquirer) - 1);
    deals_[0].value = 2500; deals_[0].premium = 25; deals_[0].ev_revenue = 3.1;
    deals_[0].ev_ebitda = 15.6; deals_[0].synergies = 200; deals_[0].cash_pct = 60;
    deals_[0].stock_pct = 40;

    std::strncpy(deals_[1].target, "TargetB", sizeof(deals_[1].target) - 1);
    std::strncpy(deals_[1].acquirer, "AcquirerB", sizeof(deals_[1].acquirer) - 1);
    deals_[1].value = 1800; deals_[1].premium = 30; deals_[1].ev_revenue = 3.0;
    deals_[1].ev_ebitda = 15.0; deals_[1].synergies = 150; deals_[1].cash_pct = 100;

    std::strncpy(deals_[2].target, "TargetC", sizeof(deals_[2].target) - 1);
    std::strncpy(deals_[2].acquirer, "AcquirerC", sizeof(deals_[2].acquirer) - 1);
    deals_[2].value = 3200; deals_[2].premium = 22; deals_[2].ev_revenue = 3.2;
    deals_[2].ev_ebitda = 16.0; deals_[2].synergies = 300; deals_[2].cash_pct = 50;
    deals_[2].stock_pct = 50;
}

std::vector<MADeal> ComparisonPanel::build_deal_list() const {
    std::vector<MADeal> list;
    for (auto& d : deals_) {
        MADeal deal;
        deal.target_name = d.target;
        deal.acquirer_name = d.acquirer;
        deal.deal_value = d.value;
        deal.premium_1day = d.premium;
        deal.ev_revenue = d.ev_revenue;
        deal.ev_ebitda = d.ev_ebitda;
        deal.synergies = d.synergies;
        deal.payment_cash_pct = d.cash_pct;
        deal.payment_stock_pct = d.stock_pct;
        deal.industry = d.industry;
        deal.status = d.status;
        list.push_back(deal);
    }
    return list;
}

// =============================================================================
// Main render
// =============================================================================
void ComparisonPanel::render(MNAData& data) {
    init_default_deals();

    const char* tabs[] = {"Compare", "Rank", "Benchmark", "Payment", "Industry"};
    for (int i = 0; i < 5; i++) {
        if (ColoredButton(tabs[i], ma_colors::COMPARISON, sub_tab_ == i)) {
            sub_tab_ = i;
            result_ = json();
            status_.clear();
        }
        if (i < 4) ImGui::SameLine(0, 2);
    }
    ImGui::Separator();
    ImGui::Spacing();

    // Deal editor (shared across sub-tabs)
    bool open = !inputs_collapsed_;
    if (CollapsibleHeader("DEAL DATA (editable)", &open, ma_colors::COMPARISON)) {
        render_deal_editor();
    }
    inputs_collapsed_ = !open;

    ImGui::Spacing();

    // Build JSON deals for Python
    auto deal_list = build_deal_list();
    json deals_json = json::array();
    for (auto& d : deal_list) {
        deals_json.push_back(d.to_json());
    }

    // Sub-tab specific run buttons and Python calls
    switch (sub_tab_) {
    case 0: { // Compare
        if (RunButton("COMPARE DEALS", data.is_loading(), ma_colors::COMPARISON)) {
            data.run_analysis("deal_comparison/deal_comparator.py", {
                "compare", deals_json.dump()
            });
        }
    } break;

    case 1: { // Rank
        const char* criteria[] = {"Premium", "Deal Value", "EV/Revenue", "EV/EBITDA", "Synergies"};
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Sort By");
        ImGui::SameLine(160);
        ImGui::SetNextItemWidth(140);
        ImGui::Combo("##rank_criteria", &rank_criteria_, criteria, 5);
        ImGui::Spacing();

        if (RunButton("RANK DEALS", data.is_loading(), ma_colors::COMPARISON)) {
            DealSortCriteria crit = static_cast<DealSortCriteria>(rank_criteria_);
            data.run_analysis("deal_comparison/deal_comparator.py", {
                "rank", deals_json.dump(), sort_label(crit)
            });
        }
    } break;

    case 2: { // Benchmark
        if (!deals_.empty()) {
            ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.75f, 1.0f), "Target Deal");
            ImGui::SameLine(160);
            ImGui::SetNextItemWidth(140);

            // Build combo items
            std::vector<std::string> labels;
            for (auto& d : deals_) labels.push_back(d.target);
            if (target_deal_idx_ >= static_cast<int>(labels.size()))
                target_deal_idx_ = 0;

            if (ImGui::BeginCombo("##bench_target", labels[target_deal_idx_].c_str())) {
                for (int i = 0; i < static_cast<int>(labels.size()); i++) {
                    if (ImGui::Selectable(labels[i].c_str(), i == target_deal_idx_))
                        target_deal_idx_ = i;
                }
                ImGui::EndCombo();
            }
        }
        ImGui::Spacing();

        if (RunButton("BENCHMARK DEAL", data.is_loading(), ma_colors::COMPARISON)) {
            json target_deal = deal_list[target_deal_idx_].to_json();
            // Comparable deals = all except the target
            json comp_deals = json::array();
            for (int i = 0; i < static_cast<int>(deal_list.size()); i++) {
                if (i != target_deal_idx_)
                    comp_deals.push_back(deal_list[i].to_json());
            }
            data.run_analysis("deal_comparison/deal_comparator.py", {
                "benchmark", target_deal.dump(), comp_deals.dump()
            });
        }
    } break;

    case 3: { // Payment Analysis
        if (RunButton("ANALYZE PAYMENT", data.is_loading(), ma_colors::COMPARISON)) {
            data.run_analysis("deal_comparison/deal_comparator.py", {
                "payment_analysis", deals_json.dump()
            });
        }
    } break;

    case 4: { // Industry Analysis
        if (RunButton("ANALYZE INDUSTRY", data.is_loading(), ma_colors::COMPARISON)) {
            data.run_analysis("deal_comparison/deal_comparator.py", {
                "industry_analysis", deals_json.dump()
            });
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

    ImGui::Spacing();
    StatusMessage(status_, status_time_);

    // Result display
    if (!result_.is_null()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::COMPARISON, "RESULTS");
        ImGui::Spacing();

        // Comparison table
        if (result_.contains("comparison") && result_["comparison"].is_object()) {
            ImGui::TextColored(theme::colors::TEXT_DIM, "Side-by-Side Comparison");
            for (auto& [metric, values] : result_["comparison"].items()) {
                if (values.is_array()) {
                    std::string row_val;
                    for (size_t i = 0; i < values.size(); i++) {
                        if (i > 0) row_val += " | ";
                        if (values[i].is_number())
                            row_val += format_currency(values[i].get<double>());
                        else if (values[i].is_string())
                            row_val += values[i].get<std::string>();
                    }
                    DataRow(metric.c_str(), row_val.c_str(), theme::colors::TEXT_PRIMARY);
                }
            }
        }

        // Rankings
        if (result_.contains("rankings") && result_["rankings"].is_array()) {
            ImGui::TextColored(theme::colors::TEXT_DIM, "Rankings");
            int rank = 1;
            for (auto& r : result_["rankings"]) {
                char label[64];
                std::snprintf(label, sizeof(label), "#%d %s",
                    rank++, r.value("target_name", "").c_str());
                // Show the sorted metric value
                if (r.contains("premium_1day"))
                    DataRow(label, format_percent(r["premium_1day"].get<double>()).c_str(), theme::colors::TEXT_PRIMARY);
                else if (r.contains("deal_value"))
                    DataRow(label, format_currency(r["deal_value"].get<double>()).c_str(), theme::colors::TEXT_PRIMARY);
                else
                    DataRow(label, r.value("acquirer_name", "").c_str(), theme::colors::TEXT_SECONDARY);
            }
        }

        // Premium comparison (benchmark)
        if (result_.contains("premium_comparison") && result_["premium_comparison"].is_object()) {
            auto& pc = result_["premium_comparison"];
            ImGui::TextColored(theme::colors::TEXT_DIM, "Premium Benchmark");
            if (pc.contains("target"))
                DataRow("Target Premium", format_percent(pc["target"].get<double>()).c_str(), theme::colors::TEXT_PRIMARY);
            if (pc.contains("median"))
                DataRow("Comp Median", format_percent(pc["median"].get<double>()).c_str(), theme::colors::TEXT_SECONDARY);
            if (pc.contains("percentile"))
                DataRow("Percentile Rank", format_percent(pc["percentile"].get<double>()).c_str(), theme::colors::TEXT_SECONDARY);
        }

        // Payment distribution
        if (result_.contains("distribution") && result_["distribution"].is_object()) {
            ImGui::TextColored(theme::colors::TEXT_DIM, "Payment Distribution");
            for (auto& [k, v] : result_["distribution"].items()) {
                if (v.is_number())
                    DataRow(k.c_str(), std::to_string(v.get<int>()).c_str(), theme::colors::TEXT_PRIMARY);
            }
        }

        // Industry breakdown
        if (result_.contains("by_industry") && result_["by_industry"].is_object()) {
            ImGui::TextColored(theme::colors::TEXT_DIM, "Industry Breakdown");
            for (auto& [industry, stats] : result_["by_industry"].items()) {
                if (stats.is_object()) {
                    std::string info = "Count: " + std::to_string(stats.value("count", 0));
                    if (stats.contains("avg_premium"))
                        info += " | Avg Premium: " + format_percent(stats["avg_premium"].get<double>());
                    if (stats.contains("total_value"))
                        info += " | Total: " + format_currency(stats["total_value"].get<double>());
                    DataRow(industry.c_str(), info.c_str(), theme::colors::TEXT_PRIMARY);
                }
            }
        }

        // Insight text
        if (result_.contains("insight") && result_["insight"].is_string()) {
            ImGui::Spacing();
            ImGui::TextColored(theme::colors::TEXT_DIM, "%s", result_["insight"].get<std::string>().c_str());
        }
    }
}

// =============================================================================
// Deal Editor
// =============================================================================
void ComparisonPanel::render_deal_editor() {
    using namespace theme::colors;

    // Add/Remove buttons
    if (ImGui::SmallButton("+ Add Deal")) {
        deals_.push_back({});
        std::snprintf(deals_.back().target, sizeof(deals_.back().target), "Target%d", static_cast<int>(deals_.size()));
        std::snprintf(deals_.back().acquirer, sizeof(deals_.back().acquirer), "Acquirer%d", static_cast<int>(deals_.size()));
    }
    if (deals_.size() > 1) {
        ImGui::SameLine(0, 8);
        if (ImGui::SmallButton("- Remove Last")) {
            deals_.pop_back();
        }
    }
    ImGui::Spacing();

    if (ImGui::BeginTable("##deal_editor", 9,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Acquirer", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Value ($M)", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Premium %", ImGuiTableColumnFlags_WidthFixed, 65);
        ImGui::TableSetupColumn("EV/Rev", ImGuiTableColumnFlags_WidthFixed, 55);
        ImGui::TableSetupColumn("EV/EBITDA", ImGuiTableColumnFlags_WidthFixed, 65);
        ImGui::TableSetupColumn("Synergies", ImGuiTableColumnFlags_WidthFixed, 65);
        ImGui::TableSetupColumn("Cash%", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Industry", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(deals_.size()); i++) {
            auto& d = deals_[i];
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            char id[32];
            std::snprintf(id, sizeof(id), "##t_%d", i);
            ImGui::InputText(id, d.target, sizeof(d.target));

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            std::snprintf(id, sizeof(id), "##a_%d", i);
            ImGui::InputText(id, d.acquirer, sizeof(d.acquirer));

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            std::snprintf(id, sizeof(id), "##v_%d", i);
            ImGui::InputDouble(id, &d.value, 0, 0, "%.0f");

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            std::snprintf(id, sizeof(id), "##p_%d", i);
            ImGui::InputDouble(id, &d.premium, 0, 0, "%.1f");

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            std::snprintf(id, sizeof(id), "##er_%d", i);
            ImGui::InputDouble(id, &d.ev_revenue, 0, 0, "%.1f");

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            std::snprintf(id, sizeof(id), "##ee_%d", i);
            ImGui::InputDouble(id, &d.ev_ebitda, 0, 0, "%.1f");

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            std::snprintf(id, sizeof(id), "##s_%d", i);
            ImGui::InputDouble(id, &d.synergies, 0, 0, "%.0f");

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            std::snprintf(id, sizeof(id), "##c_%d", i);
            ImGui::InputDouble(id, &d.cash_pct, 0, 0, "%.0f");

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            std::snprintf(id, sizeof(id), "##ind_%d", i);
            ImGui::InputText(id, d.industry, sizeof(d.industry));
        }
        ImGui::EndTable();
    }

    // Load from database
    ImGui::Spacing();
    if (ImGui::SmallButton("Load from DB")) {
        auto db_deals = db::ops::get_all_ma_deals();
        deals_.clear();
        for (auto& row : db_deals) {
            CompDeal cd;
            std::strncpy(cd.target, row.target_name.c_str(), sizeof(cd.target) - 1);
            std::strncpy(cd.acquirer, row.acquirer_name.c_str(), sizeof(cd.acquirer) - 1);
            cd.value = row.deal_value;
            cd.premium = row.premium_1day;
            cd.ev_revenue = row.ev_revenue;
            cd.ev_ebitda = row.ev_ebitda;
            cd.synergies = row.synergies;
            cd.cash_pct = row.payment_cash_pct;
            cd.stock_pct = row.payment_stock_pct;
            std::strncpy(cd.industry, row.industry.c_str(), sizeof(cd.industry) - 1);
            std::strncpy(cd.status, row.status.c_str(), sizeof(cd.status) - 1);
            deals_.push_back(cd);
        }
        if (deals_.empty()) {
            status_ = "No deals in database";
            status_time_ = ImGui::GetTime();
        } else {
            status_ = "Loaded " + std::to_string(deals_.size()) + " deals from DB";
            status_time_ = ImGui::GetTime();
        }
    }
}

} // namespace fincept::mna
