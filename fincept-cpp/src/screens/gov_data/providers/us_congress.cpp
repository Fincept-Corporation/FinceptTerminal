#include "us_congress.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstdio>

namespace fincept::gov_data {

using json = nlohmann::json;
static constexpr auto& COLOR = provider_colors::US_CONGRESS;

void USCongressProvider::render() {
    pickup_result();
    render_toolbar();

    ImGui::BeginChild("##cong_body", ImVec2(0, 0));
    switch (active_view_) {
        case 0: render_bills();        break;
        case 1: render_bill_detail();  break;
        case 2: render_summary_view(); break;
    }
    ImGui::EndChild();
}

// =============================================================================
// Toolbar
// =============================================================================
void USCongressProvider::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##cong_toolbar", ImVec2(0, 36), ImGuiChildFlags_Borders);

    // View tabs
    const char* tabs[] = {"BILLS", "BILL DETAIL", "SUMMARY"};
    for (int i = 0; i < 3; i++) {
        if (ProviderTab(tabs[i], COLOR, active_view_ == i))
            active_view_ = i;
        ImGui::SameLine(0, 2);
    }

    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);

    // Congress number
    ImGui::TextColored(TEXT_DIM, "Congress:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(50);
    ImGui::InputInt("##cong_num", &congress_number_, 0, 0);
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 8);

    // Bill type filter
    ImGui::TextColored(TEXT_DIM, "Type:");
    ImGui::SameLine(0, 4);
    auto& types = congress_bill_types();
    ImGui::PushItemWidth(130);
    if (ImGui::BeginCombo("##cong_type", types[bill_type_idx_].label)) {
        for (size_t i = 0; i < types.size(); i++) {
            if (ImGui::Selectable(types[i].label, (int)i == bill_type_idx_))
                bill_type_idx_ = (int)i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 8);
    if (FetchButton("FETCH", data_.is_loading(), COLOR)) {
        if (active_view_ == 0) fetch_bills();
        else if (active_view_ == 2) fetch_summary();
    }

    ImGui::SameLine(0, 12);
    StatusMsg(status_, status_time_);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Fetch
// =============================================================================
void USCongressProvider::fetch_bills() {
    auto& types = congress_bill_types();
    std::vector<std::string> args = {"--action", "bills",
        "--congress", std::to_string(congress_number_),
        "--offset", std::to_string(bills_offset_),
        "--limit", "50"};
    if (bill_type_idx_ > 0)
        args.insert(args.end(), {"--bill-type", types[bill_type_idx_].value});
    data_.run_script("congress_gov_data.py", args);
    status_ = "Fetching bills...";
    status_time_ = ImGui::GetTime();
}

void USCongressProvider::fetch_bill_detail(const std::string& bill_path) {
    selected_bill_path_ = bill_path;
    auto& types = congress_bill_types();
    std::string bt = bill_type_idx_ > 0 ? types[bill_type_idx_].value : "hr";
    std::vector<std::string> args = {"--action", "bill-detail",
        "--congress", std::to_string(congress_number_),
        "--bill-type", bt,
        "--bill-number", bill_path};
    data_.run_script("congress_gov_data.py", args);
    status_ = "Fetching bill detail...";
    status_time_ = ImGui::GetTime();
}

void USCongressProvider::fetch_summary() {
    std::vector<std::string> args = {"--action", "summary",
        "--congress", std::to_string(congress_number_)};
    data_.run_script("congress_gov_data.py", args);
    status_ = "Fetching summary...";
    status_time_ = ImGui::GetTime();
}

void USCongressProvider::pickup_result() {
    if (!data_.has_result() && data_.error().empty()) return;
    std::lock_guard<std::mutex> lock(data_.mutex());
    if (data_.has_result()) {
        const auto& r = data_.result();
        switch (active_view_) {
            case 0: bills_result_   = r; break;
            case 1: bill_detail_    = r; break;
            case 2: summary_result_ = r; break;
        }
        status_ = "Data loaded";
        status_time_ = ImGui::GetTime();
    } else if (!data_.error().empty()) {
        status_ = "Error: " + data_.error();
        status_time_ = ImGui::GetTime();
    }
    data_.clear();
}

// =============================================================================
// Bills table
// =============================================================================
void USCongressProvider::render_bills() {
    using namespace theme::colors;

    // Pagination
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "Offset: %d", bills_offset_);
    ImGui::SameLine(0, 12);
    if (ImGui::SmallButton("< Prev") && bills_offset_ >= 50) {
        bills_offset_ -= 50;
        fetch_bills();
    }
    ImGui::SameLine(0, 4);
    if (ImGui::SmallButton("Next >")) {
        bills_offset_ += 50;
        fetch_bills();
    }
    ImGui::Spacing();

    if (bills_result_.empty()) {
        EmptyState("No bills data loaded", "Click FETCH to load congressional bills");
        return;
    }

    // bills_result_ may be {bills: [...]} or an array
    json arr = bills_result_.is_array() ? bills_result_
        : (bills_result_.contains("bills") ? bills_result_["bills"] : json::array());

    if (arr.empty()) {
        EmptyState("No bills found", "Try changing congress number or bill type");
        return;
    }

    ImGui::TextColored(TEXT_DIM, "%zu bills", arr.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##cong_bills_tbl", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Number");
        ImGui::TableSetupColumn("Title");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Introduced");
        ImGui::TableSetupColumn("Sponsor");
        ImGui::TableSetupColumn("Action");
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& bill : arr) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            std::string num = bill.value("number", bill.value("bill_number", "-"));
            if (ImGui::Selectable(num.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                active_view_ = 1;
                fetch_bill_detail(num);
            }
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", truncate(bill.value("title", "-"), 80).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", bill.value("type", bill.value("bill_type", "-")).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s",
                format_date(bill.value("introduced_date", bill.value("introducedDate", ""))).c_str());
            ImGui::TableNextColumn();
            // Sponsor may be nested
            std::string sponsor = "-";
            if (bill.contains("sponsor") && bill["sponsor"].is_object())
                sponsor = bill["sponsor"].value("name", bill["sponsor"].value("fullName", "-"));
            else
                sponsor = bill.value("sponsor", "-");
            ImGui::Text("%s", truncate(sponsor, 30).c_str());
            ImGui::TableNextColumn();
            std::string action = "-";
            if (bill.contains("latestAction") && bill["latestAction"].is_object())
                action = bill["latestAction"].value("text", "-");
            ImGui::TextColored(TEXT_DIM, "%s", truncate(action, 40).c_str());
        }
        ImGui::EndTable();
    }
}

// =============================================================================
// Bill detail
// =============================================================================
void USCongressProvider::render_bill_detail() {
    using namespace theme::colors;

    if (bill_detail_.empty()) {
        EmptyState("No bill selected", "Click a bill number in the BILLS tab");
        return;
    }

    ImGui::Spacing();
    ImGui::TextColored(COLOR, "BILL DETAIL: %s", selected_bill_path_.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Show all fields as key-value pairs
    if (bill_detail_.is_object()) {
        for (auto it = bill_detail_.begin(); it != bill_detail_.end(); ++it) {
            const std::string& key = it.key();
            const auto& val = it.value();
            if (val.is_string()) {
                InfoRow(key.c_str(), val.get<std::string>().c_str());
            } else if (val.is_number()) {
                InfoRow(key.c_str(), val.dump().c_str());
            } else if (val.is_object()) {
                ImGui::Spacing();
                ImGui::TextColored(COLOR, "%s", key.c_str());
                ImGui::Indent(12);
                for (auto it2 = val.begin(); it2 != val.end(); ++it2) {
                    std::string vs = it2.value().is_string() ? it2.value().get<std::string>() : it2.value().dump();
                    InfoRow(it2.key().c_str(), vs.c_str());
                }
                ImGui::Unindent(12);
            } else if (val.is_array() && !val.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(COLOR, "%s (%zu)", key.c_str(), val.size());
                ImGui::Indent(12);
                for (size_t i = 0; i < val.size() && i < 20; i++) {
                    if (val[i].is_string()) {
                        ImGui::BulletText("%s", val[i].get<std::string>().c_str());
                    } else if (val[i].is_object()) {
                        std::string label = val[i].value("text",
                            val[i].value("name",
                            val[i].value("title", val[i].dump())));
                        ImGui::BulletText("%s", truncate(label, 80).c_str());
                    }
                }
                ImGui::Unindent(12);
            }
        }
    }
}

// =============================================================================
// Summary grid
// =============================================================================
void USCongressProvider::render_summary_view() {
    using namespace theme::colors;

    if (summary_result_.empty()) {
        EmptyState("No summary loaded", "Click FETCH to load congress summary");
        return;
    }

    ImGui::Spacing();
    ImGui::TextColored(COLOR, "CONGRESS SUMMARY — %dth", congress_number_);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (summary_result_.is_object()) {
        for (auto it = summary_result_.begin(); it != summary_result_.end(); ++it) {
            const auto& val = it.value();
            if (val.is_number() || val.is_string()) {
                std::string vs = val.is_string() ? val.get<std::string>() : val.dump();
                InfoRow(it.key().c_str(), vs.c_str(), COLOR);
            }
        }
    }
}

} // namespace fincept::gov_data
