#include "financials_panel.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <set>

namespace fincept::research {

using namespace theme::colors;

static void fmt_large(char* buf, size_t sz, double v) {
    if (v == 0) { std::snprintf(buf, sz, "-"); return; }
    double av = std::abs(v);
    const char* sign = v < 0 ? "-" : "";
    if (av >= 1e12)      std::snprintf(buf, sz, "%s$%.2fT", sign, av / 1e12);
    else if (av >= 1e9)  std::snprintf(buf, sz, "%s$%.2fB", sign, av / 1e9);
    else if (av >= 1e6)  std::snprintf(buf, sz, "%s$%.2fM", sign, av / 1e6);
    else if (av >= 1e3)  std::snprintf(buf, sz, "%s$%.2fK", sign, av / 1e3);
    else                 std::snprintf(buf, sz, "%s$%.0f", sign, av);
}

// Make metric names more readable
static std::string format_metric_name(const std::string& key) {
    std::string result;
    for (size_t i = 0; i < key.size(); i++) {
        char c = key[i];
        if (i > 0 && std::isupper(c) && std::islower(key[i-1])) {
            result += ' ';
        }
        result += (char)std::toupper(c);
    }
    return result;
}

void FinancialsPanel::render(ResearchData& data) {
    std::shared_lock<std::shared_mutex> lock(data.mutex());
    auto& fin = data.financials();

    // Statement selector tabs
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 4));
    const char* labels[] = {"INCOME STATEMENT", "BALANCE SHEET", "CASH FLOW"};
    for (int i = 0; i < 3; i++) {
        bool active = (active_statement_ == i);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        }
        if (ImGui::Button(labels[i])) active_statement_ = i;
        if (active) ImGui::PopStyleColor(2);
        if (i < 2) ImGui::SameLine(0, 4);
    }
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Render the selected statement
    switch (active_statement_) {
        case 0: render_statement_table(fin.income_statement, "INCOME STATEMENT"); break;
        case 1: render_statement_table(fin.balance_sheet, "BALANCE SHEET"); break;
        case 2: render_statement_table(fin.cash_flow, "CASH FLOW STATEMENT"); break;
    }
}

void FinancialsPanel::render_statement_table(const FinancialStatement& stmt, const char* title) {
    if (stmt.data.empty()) {
        ImGui::TextColored(TEXT_DIM, "No %s data available.", title);
        return;
    }

    // Get sorted periods (most recent first)
    std::vector<std::string> periods;
    for (auto& [date, _] : stmt.data) periods.push_back(date);
    std::sort(periods.begin(), periods.end(), std::greater<>());

    // Get all unique metric keys
    std::set<std::string> metric_set;
    for (auto& [_, metrics] : stmt.data) {
        for (auto& [key, __] : metrics) {
            metric_set.insert(key);
        }
    }
    std::vector<std::string> metric_keys(metric_set.begin(), metric_set.end());
    std::sort(metric_keys.begin(), metric_keys.end());

    int num_cols = 1 + (int)periods.size(); // metric name + periods
    if (num_cols > 6) num_cols = 6; // cap at 5 periods
    int num_periods = num_cols - 1;

    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_SizingStretchProp;

    float table_h = ImGui::GetContentRegionAvail().y;

    if (ImGui::BeginTable(title, num_cols, flags, ImVec2(0, table_h))) {
        // Header
        ImGui::TableSetupColumn("METRIC", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        for (int i = 0; i < num_periods; i++) {
            // Extract year from date
            std::string year = periods[i].substr(0, 4);
            ImGui::TableSetupColumn(year.c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        }
        ImGui::TableSetupScrollFreeze(1, 1);

        // Custom header rendering
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        ImGui::TextUnformatted("METRIC");
        ImGui::PopStyleColor();

        for (int i = 0; i < num_periods; i++) {
            ImGui::TableSetColumnIndex(i + 1);
            std::string year = periods[i].substr(0, 4);
            ImGui::PushStyleColor(ImGuiCol_Text, INFO);
            ImGui::TextUnformatted(year.c_str());
            ImGui::PopStyleColor();
        }

        // Data rows
        for (auto& key : metric_keys) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            std::string display_name = format_metric_name(key);
            ImGui::TextColored(TEXT_SECONDARY, "%s", display_name.c_str());

            for (int i = 0; i < num_periods; i++) {
                ImGui::TableSetColumnIndex(i + 1);
                auto it = stmt.data.find(periods[i]);
                if (it != stmt.data.end()) {
                    auto mit = it->second.find(key);
                    if (mit != it->second.end()) {
                        char buf[64];
                        fmt_large(buf, sizeof(buf), mit->second);
                        ImVec4 color = mit->second < 0 ? MARKET_RED : TEXT_PRIMARY;
                        ImGui::TextColored(color, "%s", buf);
                    } else {
                        ImGui::TextColored(TEXT_DISABLED, "-");
                    }
                }
            }
        }

        ImGui::EndTable();
    }
}

} // namespace fincept::research
