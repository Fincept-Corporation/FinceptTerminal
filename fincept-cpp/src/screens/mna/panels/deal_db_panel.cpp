#include "deal_db_panel.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstring>

namespace fincept::mna {

using json = nlohmann::json;

void DealDbPanel::init() {
    if (initialized_) return;
    initialized_ = true;
    load_deals();
}

void DealDbPanel::load_deals() {
    deals_ = db::ops::get_all_ma_deals();
}

// =============================================================================
// Main render
// =============================================================================
void DealDbPanel::render(MNAData& data) {
    init();

    const char* tabs[] = {"Deal Database", "SEC Scanner"};
    for (int i = 0; i < 2; i++) {
        if (ColoredButton(tabs[i], ma_colors::DEAL_DB, left_tab_ == i)) {
            left_tab_ = i;
        }
        if (i == 0) ImGui::SameLine(0, 2);
    }

    ImGui::SameLine(0, 16);
    if (RunButton("+ NEW DEAL", false, ma_colors::DEAL_DB)) {
        show_create_modal_ = true;
    }

    ImGui::Separator();
    ImGui::Spacing();

    if (left_tab_ == 0) {
        // Search bar
        ImGui::TextColored(theme::colors::TEXT_DIM, "Search:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("##deal_search", search_buf_, sizeof(search_buf_),
                ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string q(search_buf_);
            if (q.empty())
                load_deals();
            else
                deals_ = db::ops::search_ma_deals(q);
        }
        ImGui::SameLine(0, 8);
        if (ImGui::SmallButton("Search")) {
            std::string q(search_buf_);
            if (q.empty())
                load_deals();
            else
                deals_ = db::ops::search_ma_deals(q);
        }
        ImGui::SameLine(0, 8);
        if (ImGui::SmallButton("Refresh")) {
            load_deals();
        }

        ImGui::Spacing();

        // Two-panel: list on left, detail on right
        float avail = ImGui::GetContentRegionAvail().x;
        float list_w = avail * 0.45f;
        float detail_w = avail - list_w - 4.0f;

        ImGui::BeginChild("##deal_list_pane", ImVec2(list_w, 0), ImGuiChildFlags_Borders);
        render_deal_list();
        ImGui::EndChild();

        ImGui::SameLine(0, 4);

        ImGui::BeginChild("##deal_detail_pane", ImVec2(detail_w, 0), ImGuiChildFlags_Borders);
        render_deal_detail();
        ImGui::EndChild();
    } else {
        render_scanner(data);
    }

    if (show_create_modal_) {
        render_create_modal();
    }

    StatusMessage(status_, status_time_);
}

// =============================================================================
// Deal List
// =============================================================================
void DealDbPanel::render_deal_list() {
    using namespace theme::colors;

    ImGui::TextColored(ma_colors::DEAL_DB, "DEALS (%d)", static_cast<int>(deals_.size()));
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(deals_.size()); i++) {
        auto& d = deals_[i];
        bool selected = (i == selected_deal_);

        // Color based on status
        ImVec4 status_col = TEXT_DIM;
        if (d.status == "Completed") status_col = SUCCESS;
        else if (d.status == "Pending") status_col = ImVec4(1.0f, 0.77f, 0.0f, 1.0f);
        else if (d.status == "Failed") status_col = ImVec4(0.96f, 0.30f, 0.30f, 1.0f);

        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Header,
                ImVec4(ma_colors::DEAL_DB.x, ma_colors::DEAL_DB.y, ma_colors::DEAL_DB.z, 0.15f));
        }

        char label[128];
        std::snprintf(label, sizeof(label), "%s -> %s##deal_%d",
                      d.target_name.c_str(), d.acquirer_name.c_str(), i);

        if (ImGui::Selectable(label, selected, 0, ImVec2(0, 36))) {
            selected_deal_ = i;
        }

        if (selected) ImGui::PopStyleColor();

        // Sub-info
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
        ImGui::TextColored(status_col, "%s", d.status.c_str());

        if (d.deal_value > 0) {
            ImGui::TextColored(TEXT_DIM, "  %s  |  %s",
                format_deal_value(d.deal_value).c_str(), d.industry.c_str());
        }
    }

    if (deals_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "No deals found. Click '+ NEW DEAL' to add.");
    }
}

// =============================================================================
// Deal Detail
// =============================================================================
void DealDbPanel::render_deal_detail() {
    using namespace theme::colors;

    if (selected_deal_ < 0 || selected_deal_ >= static_cast<int>(deals_.size())) {
        ImGui::TextColored(TEXT_DIM, "Select a deal to view details");
        return;
    }

    auto& d = deals_[selected_deal_];

    ImGui::TextColored(ma_colors::DEAL_DB, "%s", d.target_name.c_str());
    ImGui::TextColored(TEXT_DIM, "Acquired by %s", d.acquirer_name.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    // Key metrics
    ImGui::TextColored(ma_colors::DEAL_DB, "DEAL METRICS");
    ImGui::Spacing();
    DataRow("Deal Value", format_deal_value(d.deal_value).c_str(), TEXT_PRIMARY);
    if (d.offer_price_per_share > 0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "$%.2f", d.offer_price_per_share);
        DataRow("Offer Price/Share", buf, TEXT_PRIMARY);
    }
    if (d.premium_1day > 0)
        DataRow("1-Day Premium", format_percent(d.premium_1day).c_str(), TEXT_PRIMARY);
    if (d.ev_revenue > 0)
        DataRow("EV/Revenue", format_multiple(d.ev_revenue).c_str(), TEXT_SECONDARY);
    if (d.ev_ebitda > 0)
        DataRow("EV/EBITDA", format_multiple(d.ev_ebitda).c_str(), TEXT_SECONDARY);
    if (d.synergies > 0)
        DataRow("Synergies", format_deal_value(d.synergies).c_str(), TEXT_SECONDARY);

    ImGui::Spacing();
    ImGui::TextColored(ma_colors::DEAL_DB, "DEAL STRUCTURE");
    ImGui::Spacing();
    if (!d.payment_method.empty())
        DataRow("Payment Method", d.payment_method.c_str(), TEXT_SECONDARY);
    if (d.payment_cash_pct > 0)
        DataRow("Cash %", format_percent(d.payment_cash_pct).c_str(), TEXT_SECONDARY);
    if (d.payment_stock_pct > 0)
        DataRow("Stock %", format_percent(d.payment_stock_pct).c_str(), TEXT_SECONDARY);
    if (d.breakup_fee > 0)
        DataRow("Breakup Fee", format_deal_value(d.breakup_fee).c_str(), TEXT_SECONDARY);

    ImGui::Spacing();
    ImGui::TextColored(ma_colors::DEAL_DB, "DETAILS");
    ImGui::Spacing();
    DataRow("Status", d.status.c_str(), TEXT_SECONDARY);
    DataRow("Industry", d.industry.c_str(), TEXT_SECONDARY);
    if (!d.deal_type.empty())
        DataRow("Deal Type", d.deal_type.c_str(), TEXT_SECONDARY);
    if (!d.announced_date.empty())
        DataRow("Announced", d.announced_date.c_str(), TEXT_SECONDARY);
    if (d.hostile_flag)
        DataRow("Hostile", "Yes", ImVec4(0.96f, 0.30f, 0.30f, 1.0f));
    if (d.cross_border_flag)
        DataRow("Cross-Border", "Yes", TEXT_SECONDARY);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Delete button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    if (ImGui::Button("DELETE DEAL")) {
        db::ops::delete_ma_deal(d.deal_id);
        selected_deal_ = -1;
        load_deals();
        status_ = "Deal deleted";
        status_time_ = ImGui::GetTime();
    }
    ImGui::PopStyleColor(3);
}

// =============================================================================
// SEC Filing Scanner
// =============================================================================
void DealDbPanel::render_scanner(MNAData& data) {
    using namespace theme::colors;

    ImGui::TextColored(ma_colors::DEAL_DB, "SEC FILING SCANNER (EDGAR)");
    ImGui::Spacing();

    InputRowInt("Days to Scan", &scan_days_);
    ImGui::Spacing();

    if (RunButton("SCAN FILINGS", data.is_loading(), ma_colors::DEAL_DB)) {
        data.run_analysis("deal_database/deal_scanner.py", {
            "scan", std::to_string(scan_days_)
        });
    }

    ImGui::SameLine(0, 8);

    if (RunButton("HIGH CONFIDENCE", data.is_loading(), ma_colors::DEAL_DB)) {
        data.run_analysis("deal_database/deal_scanner.py", {
            "high_confidence", "0.75"
        });
    }

    if (!data.is_loading() && data.has_result()) {
        std::lock_guard<std::mutex> lock(data.mutex());
        filings_ = data.result();
        status_ = "Scan complete";
        status_time_ = ImGui::GetTime();
        data.clear();
    }
    if (!data.is_loading() && !data.error().empty()) {
        status_ = "Error: " + data.error();
        status_time_ = ImGui::GetTime();
        data.clear();
    }

    // Display filings
    if (filings_.is_array() && !filings_.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ma_colors::DEAL_DB, "FILINGS (%d)", static_cast<int>(filings_.size()));
        ImGui::Spacing();

        if (ImGui::BeginTable("##filings", 5,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                ImVec2(0, 300))) {
            ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Company", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Form", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Confidence", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableHeadersRow();

            for (auto& f : filings_) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", f.value("date", "").c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", f.value("company_name", f.value("company", "")).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", f.value("form_type", "").c_str());
                ImGui::TableNextColumn();
                double conf = f.value("confidence", 0.0);
                ImVec4 conf_col = (conf >= 0.75) ? SUCCESS :
                                  (conf >= 0.5) ? ImVec4(1.0f, 0.77f, 0.0f, 1.0f) :
                                                  ImVec4(0.96f, 0.30f, 0.30f, 1.0f);
                ImGui::TextColored(conf_col, "%.0f%%", conf * 100);
                ImGui::TableNextColumn();
                char btn[32];
                std::snprintf(btn, sizeof(btn), "Add##f_%s", f.value("accession_number", "").c_str());
                if (ImGui::SmallButton(btn)) {
                    // Create deal from filing
                    db::MADealRow row;
                    row.target_name = f.value("company_name", f.value("company", "Unknown"));
                    row.industry = f.value("industry", "General");
                    row.status = "Pending";
                    row.announced_date = f.value("date", "");
                    db::ops::create_ma_deal(row);
                    load_deals();
                    status_ = "Deal created from filing";
                    status_time_ = ImGui::GetTime();
                }
            }
            ImGui::EndTable();
        }
    } else if (filings_.is_object() && filings_.contains("count")) {
        // Result wrapper format
        ImGui::TextColored(TEXT_DIM, "Found %d filings", filings_.value("count", 0));
        if (filings_.contains("data") && filings_["data"].is_array()) {
            filings_ = filings_["data"];  // unwrap for next render
        }
    }
}

// =============================================================================
// Create Deal Modal
// =============================================================================
void DealDbPanel::render_create_modal() {
    using namespace theme::colors;

    ImGui::OpenPopup("Create Deal##modal");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 380));

    if (ImGui::BeginPopupModal("Create Deal##modal", &show_create_modal_,
            ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored(ma_colors::DEAL_DB, "NEW DEAL");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Target Name");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##cd_target", cd_target_, sizeof(cd_target_));

        ImGui::Text("Acquirer Name");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##cd_acquirer", cd_acquirer_, sizeof(cd_acquirer_));

        InputRow("Deal Value ($M)", &cd_value_, "%.0f");
        InputRow("Premium (%)", &cd_premium_, "%.1f");
        InputRow("Cash %", &cd_cash_pct_, "%.0f");

        ImGui::Text("Industry");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##cd_industry", cd_industry_, sizeof(cd_industry_));

        ImGui::Text("Status");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##cd_status", cd_status_, sizeof(cd_status_));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (RunButton("CREATE", false, ma_colors::DEAL_DB)) {
            db::MADealRow row;
            row.target_name = cd_target_;
            row.acquirer_name = cd_acquirer_;
            row.deal_value = cd_value_;
            row.premium_1day = cd_premium_;
            row.payment_cash_pct = cd_cash_pct_;
            row.payment_stock_pct = 100.0 - cd_cash_pct_;
            row.industry = cd_industry_;
            row.status = cd_status_;
            row.payment_method = (cd_cash_pct_ >= 100) ? "Cash" :
                                 (cd_cash_pct_ <= 0) ? "Stock" : "Mixed";

            db::ops::create_ma_deal(row);
            load_deals();
            show_create_modal_ = false;
            status_ = "Deal created";
            status_time_ = ImGui::GetTime();

            // Reset fields
            std::memset(cd_target_, 0, sizeof(cd_target_));
            std::memset(cd_acquirer_, 0, sizeof(cd_acquirer_));
            cd_value_ = 0;
            cd_premium_ = 0;
            cd_cash_pct_ = 100;
        }

        ImGui::SameLine(0, 8);
        if (ImGui::Button("Cancel")) {
            show_create_modal_ = false;
        }

        ImGui::EndPopup();
    }
}

} // namespace fincept::mna
