#include "us_treasury.h"
#include "ui/theme.h"
#include <imgui.h>
#include <ctime>
#include <cstdio>

namespace fincept::gov_data {

using json = nlohmann::json;
static constexpr auto& COLOR = provider_colors::US_TREASURY;

void USTreasuryProvider::render() {
    // Init default dates once
    if (start_date_[0] == '\0') init_dates();

    pickup_result();
    render_toolbar();

    ImGui::BeginChild("##ust_body", ImVec2(0, 0));
    switch (active_endpoint_) {
        case 0: render_prices();   break;
        case 1: render_auctions(); break;
        case 2: render_summary();  break;
    }
    ImGui::EndChild();
}

void USTreasuryProvider::init_dates() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    std::strftime(end_date_, sizeof(end_date_), "%Y-%m-%d", t);

    // 30 days ago
    time_t ago = now - 30 * 24 * 3600;
    struct tm* ta = localtime(&ago);
    std::strftime(start_date_, sizeof(start_date_), "%Y-%m-%d", ta);
}

// =============================================================================
// Toolbar
// =============================================================================
void USTreasuryProvider::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ust_toolbar", ImVec2(0, 36), ImGuiChildFlags_Borders);

    // Endpoint tabs
    const char* tabs[] = {"PRICES", "AUCTIONS", "SUMMARY"};
    for (int i = 0; i < 3; i++) {
        if (ProviderTab(tabs[i], COLOR, active_endpoint_ == i))
            active_endpoint_ = i;
        ImGui::SameLine(0, 2);
    }

    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);

    // Date range inputs
    ImGui::TextColored(TEXT_DIM, "From:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(90);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputText("##ust_from", start_date_, sizeof(start_date_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "To:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(90);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputText("##ust_to", end_date_, sizeof(end_date_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 8);

    // Fetch button
    if (FetchButton("FETCH", data_.is_loading(), COLOR)) {
        switch (active_endpoint_) {
            case 0: fetch_prices();   break;
            case 1: fetch_auctions(); break;
            case 2: fetch_summary();  break;
        }
    }

    ImGui::SameLine(0, 12);
    StatusMsg(status_, status_time_);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Fetch functions
// =============================================================================
void USTreasuryProvider::fetch_prices() {
    std::vector<std::string> args = {"--action", "prices",
        "--start-date", start_date_, "--end-date", end_date_};
    auto& sec = security_types();
    if (security_type_idx_ > 0)
        args.insert(args.end(), {"--security-type", sec[security_type_idx_].value});
    if (cusip_filter_[0] != '\0')
        args.insert(args.end(), {"--cusip", cusip_filter_});
    data_.run_script("government_us_data.py", args);
    status_ = "Fetching prices...";
    status_time_ = ImGui::GetTime();
}

void USTreasuryProvider::fetch_auctions() {
    std::vector<std::string> args = {"--action", "auctions",
        "--start-date", start_date_, "--end-date", end_date_,
        "--page-size", std::to_string(page_size_),
        "--page", std::to_string(page_num_)};
    auto& auc = auction_security_types();
    if (auction_type_idx_ > 0)
        args.insert(args.end(), {"--security-type", auc[auction_type_idx_].value});
    data_.run_script("government_us_data.py", args);
    status_ = "Fetching auctions...";
    status_time_ = ImGui::GetTime();
}

void USTreasuryProvider::fetch_summary() {
    std::vector<std::string> args = {"--action", "summary",
        "--start-date", start_date_, "--end-date", end_date_};
    data_.run_script("government_us_data.py", args);
    status_ = "Fetching summary...";
    status_time_ = ImGui::GetTime();
}

// =============================================================================
// Pickup async result
// =============================================================================
void USTreasuryProvider::pickup_result() {
    if (!data_.has_result() && data_.error().empty()) return;

    std::lock_guard<std::mutex> lock(data_.mutex());
    if (data_.has_result()) {
        const auto& r = data_.result();
        switch (active_endpoint_) {
            case 0: prices_result_   = r; break;
            case 1: auctions_result_ = r; break;
            case 2: summary_result_  = r; break;
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
// Render: Prices
// =============================================================================
void USTreasuryProvider::render_prices() {
    using namespace theme::colors;

    // Filters row
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "Security Type:");
    ImGui::SameLine(0, 8);
    auto& sec = security_types();
    ImGui::PushItemWidth(150);
    if (ImGui::BeginCombo("##ust_sectype", sec[security_type_idx_].label)) {
        for (size_t i = 0; i < sec.size(); i++) {
            if (ImGui::Selectable(sec[i].label, (int)i == security_type_idx_))
                security_type_idx_ = (int)i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "CUSIP:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(120);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputText("##ust_cusip", cusip_filter_, sizeof(cusip_filter_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Data table
    if (prices_result_.empty() || !prices_result_.is_array()) {
        EmptyState("No price data loaded", "Select date range and click FETCH");
        return;
    }

    if (data_.is_loading()) {
        LoadingState("Loading prices...", COLOR);
        return;
    }

    auto& arr = prices_result_;
    ImGui::TextColored(TEXT_DIM, "%zu records", arr.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##ust_prices_tbl", 8,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("CUSIP");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Rate");
        ImGui::TableSetupColumn("Maturity");
        ImGui::TableSetupColumn("Buy");
        ImGui::TableSetupColumn("Sell");
        ImGui::TableSetupColumn("EOD");
        ImGui::TableSetupColumn("Date");
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& row : arr) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(COLOR, "%s", row.value("cusip", "-").c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.value("security_type", "-").c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.value("rate", "-").c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", format_date(row.value("maturity_date", "")).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(SUCCESS, "%s", row.value("buy", "-").c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(ERROR_RED, "%s", row.value("sell", "-").c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.value("eod_price", "-").c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", format_date(row.value("date", "")).c_str());
        }
        ImGui::EndTable();
    }
}

// =============================================================================
// Render: Auctions
// =============================================================================
void USTreasuryProvider::render_auctions() {
    using namespace theme::colors;

    // Filters row
    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "Auction Type:");
    ImGui::SameLine(0, 8);
    auto& auc = auction_security_types();
    ImGui::PushItemWidth(120);
    if (ImGui::BeginCombo("##ust_auctype", auc[auction_type_idx_].label)) {
        for (size_t i = 0; i < auc.size(); i++) {
            if (ImGui::Selectable(auc[i].label, (int)i == auction_type_idx_))
                auction_type_idx_ = (int)i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "Page:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(60);
    ImGui::InputInt("##ust_page", &page_num_, 1, 5);
    ImGui::PopItemWidth();
    if (page_num_ < 1) page_num_ = 1;

    ImGui::Spacing();

    if (auctions_result_.empty() || !auctions_result_.is_array()) {
        EmptyState("No auction data loaded", "Select filters and click FETCH");
        return;
    }

    if (data_.is_loading()) {
        LoadingState("Loading auctions...", COLOR);
        return;
    }

    auto& arr = auctions_result_;
    ImGui::TextColored(TEXT_DIM, "%zu auctions", arr.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##ust_auctions_tbl", 9,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("CUSIP");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Term");
        ImGui::TableSetupColumn("Issue Date");
        ImGui::TableSetupColumn("Maturity");
        ImGui::TableSetupColumn("High Rate");
        ImGui::TableSetupColumn("Allotted");
        ImGui::TableSetupColumn("Bid/Cover");
        ImGui::TableSetupColumn("Total Accepted");
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& row : arr) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(COLOR, "%s", row.value("cusip", "-").c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.value("security_type", "-").c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.value("security_term", "-").c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", format_date(row.value("issue_date", "")).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", format_date(row.value("maturity_date", "")).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(SUCCESS, "%s", row.value("high_discount_rate", row.value("high_yield", "-")).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.value("total_accepted", "-").c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.value("bid_to_cover_ratio", "-").c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", row.value("total_tendered", "-").c_str());
        }
        ImGui::EndTable();
    }
}

// =============================================================================
// Render: Summary
// =============================================================================
void USTreasuryProvider::render_summary() {
    using namespace theme::colors;

    if (summary_result_.empty()) {
        EmptyState("No summary data loaded", "Click FETCH to load treasury summary");
        return;
    }

    if (data_.is_loading()) {
        LoadingState("Loading summary...", COLOR);
        return;
    }

    ImGui::Spacing();
    ImGui::TextColored(COLOR, "TREASURY MARKET SUMMARY");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Display summary as key-value pairs from JSON
    if (summary_result_.is_object()) {
        for (auto& [key, val] : summary_result_.items()) {
            if (val.is_object()) {
                ImGui::Spacing();
                ImGui::TextColored(COLOR, "%s", key.c_str());
                ImGui::Indent(12);
                for (auto& [k2, v2] : val.items()) {
                    std::string v_str = v2.is_string() ? v2.get<std::string>() : v2.dump();
                    InfoRow(k2.c_str(), v_str.c_str());
                }
                ImGui::Unindent(12);
            } else if (val.is_array()) {
                ImGui::Spacing();
                ImGui::TextColored(COLOR, "%s (%zu items)", key.c_str(), val.size());
                // Show as mini-table
                if (!val.empty() && val[0].is_object()) {
                    // Collect column keys
                    std::vector<std::string> cols;
                    for (auto& [ck, _] : val[0].items())
                        cols.push_back(ck);

                    int ncol = std::min((int)cols.size(), 8);
                    std::string tbl_id = "##sum_" + key;
                    if (ImGui::BeginTable(tbl_id.c_str(), ncol,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                            ImVec2(0, 200))) {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        for (int c = 0; c < ncol; c++)
                            ImGui::TableSetupColumn(cols[c].c_str());
                        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
                        ImGui::TableHeadersRow();
                        ImGui::PopStyleColor();

                        for (auto& row : val) {
                            ImGui::TableNextRow();
                            for (int c = 0; c < ncol; c++) {
                                ImGui::TableNextColumn();
                                auto& v = row[cols[c]];
                                std::string vs = v.is_string() ? v.get<std::string>() : v.dump();
                                ImGui::Text("%s", vs.c_str());
                            }
                        }
                        ImGui::EndTable();
                    }
                }
            } else {
                std::string v_str = val.is_string() ? val.get<std::string>() : val.dump();
                InfoRow(key.c_str(), v_str.c_str(), COLOR);
            }
        }
    }
}

} // namespace fincept::gov_data
