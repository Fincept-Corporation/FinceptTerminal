#include "french_gov.h"
#include "ui/theme.h"
#include <imgui.h>

namespace fincept::gov_data {

using json = nlohmann::json;
static constexpr auto& COLOR = provider_colors::FRANCE;

void FrenchGovProvider::render() {
    pickup_result();
    render_toolbar();

    ImGui::BeginChild("##fra_body", ImVec2(0, 0));
    switch (active_view_) {
        case 0: render_geo();           break;
        case 1: render_datasets_view(); break;
        case 2: render_tabular();       break;
        case 3: render_company();       break;
    }
    ImGui::EndChild();
}

void FrenchGovProvider::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##fra_toolbar", ImVec2(0, 36), ImGuiChildFlags_Borders);

    const char* tabs[] = {"GEOGRAPHY", "DATASETS", "TABULAR", "COMPANY"};
    for (int i = 0; i < 4; i++) {
        if (ProviderTab(tabs[i], COLOR, active_view_ == i))
            active_view_ = i;
        ImGui::SameLine(0, 2);
    }

    ImGui::SameLine(0, 12);
    StatusMsg(status_, status_time_);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Fetch functions
// =============================================================================
void FrenchGovProvider::fetch_geo() {
    if (geo_query_[0] == '\0') return;
    const char* types[] = {"municipalities", "departments", "regions"};
    data_.run_script("french_gov_api.py", {"--action", "geo",
        "--geo-type", types[geo_type_], "--query", geo_query_});
    status_ = "Searching geography..."; status_time_ = ImGui::GetTime();
}

void FrenchGovProvider::fetch_datasets() {
    if (dataset_query_[0] == '\0') return;
    data_.run_script("french_gov_api.py", {"--action", "datasets", "--query", dataset_query_});
    status_ = "Searching datasets..."; status_time_ = ImGui::GetTime();
}

void FrenchGovProvider::fetch_tabular_profile() {
    if (resource_id_[0] == '\0') return;
    data_.run_script("french_gov_api.py", {"--action", "tabular-profile", "--resource-id", resource_id_});
    status_ = "Fetching profile..."; status_time_ = ImGui::GetTime();
}

void FrenchGovProvider::fetch_tabular_data() {
    if (resource_id_[0] == '\0') return;
    data_.run_script("french_gov_api.py", {"--action", "tabular-data", "--resource-id", resource_id_});
    status_ = "Fetching tabular data..."; status_time_ = ImGui::GetTime();
}

void FrenchGovProvider::fetch_company() {
    if (company_query_[0] == '\0') return;
    data_.run_script("french_gov_api.py", {"--action", "company", "--query", company_query_});
    status_ = "Searching companies..."; status_time_ = ImGui::GetTime();
}

void FrenchGovProvider::pickup_result() {
    if (!data_.has_result() && data_.error().empty()) return;
    std::lock_guard<std::mutex> lock(data_.mutex());
    if (data_.has_result()) {
        const auto& r = data_.result();
        switch (active_view_) {
            case 0: geo_results_ = r; break;
            case 1: dataset_results_ = r; break;
            case 2:
                // Could be profile or data based on what was requested
                if (r.contains("profile") || r.contains("columns"))
                    tabular_profile_ = r;
                else
                    tabular_data_ = r;
                break;
            case 3: company_results_ = r; break;
        }
        status_ = "Data loaded";
    } else if (!data_.error().empty()) {
        status_ = "Error: " + data_.error();
    }
    status_time_ = ImGui::GetTime();
    data_.clear();
}

// =============================================================================
// Geography view
// =============================================================================
void FrenchGovProvider::render_geo() {
    using namespace theme::colors;

    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "Type:");
    ImGui::SameLine(0, 8);
    const char* geo_labels[] = {"Municipalities", "Departments", "Regions"};
    ImGui::PushItemWidth(140);
    if (ImGui::BeginCombo("##fra_geotype", geo_labels[geo_type_])) {
        for (int i = 0; i < 3; i++) {
            if (ImGui::Selectable(geo_labels[i], i == geo_type_)) geo_type_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 12);
    if (SearchBar(geo_query_, sizeof(geo_query_), "Search French geography...", COLOR, data_.is_loading()))
        fetch_geo();

    ImGui::Spacing();

    if (geo_results_.empty() || (!geo_results_.is_array() && !geo_results_.contains("result"))) {
        EmptyState("Search French geographic entities",
                   "Select type and enter a query above");
        return;
    }

    json arr = geo_results_.is_array() ? geo_results_
        : (geo_results_.contains("result") ? geo_results_["result"] : json::array());

    if (arr.empty()) {
        EmptyState("No results found");
        return;
    }

    ImGui::TextColored(TEXT_DIM, "%zu results", arr.size());
    ImGui::Spacing();

    // Collect keys from first item
    if (!arr.empty() && arr[0].is_object()) {
        std::vector<std::string> cols;
        for (auto& [k, _] : arr[0].items()) cols.push_back(k);
        int ncol = std::min((int)cols.size(), 8);

        if (ImGui::BeginTable("##fra_geo_tbl", ncol,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            for (int c = 0; c < ncol; c++)
                ImGui::TableSetupColumn(cols[c].c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
            ImGui::TableHeadersRow();
            ImGui::PopStyleColor();

            for (auto& row : arr) {
                ImGui::TableNextRow();
                for (int c = 0; c < ncol; c++) {
                    ImGui::TableNextColumn();
                    auto& v = row[cols[c]];
                    std::string vs = v.is_string() ? v.get<std::string>() : v.dump();
                    ImGui::Text("%s", truncate(vs, 40).c_str());
                }
            }
            ImGui::EndTable();
        }
    }
}

// =============================================================================
// Datasets view
// =============================================================================
void FrenchGovProvider::render_datasets_view() {
    using namespace theme::colors;

    ImGui::Spacing();
    if (SearchBar(dataset_query_, sizeof(dataset_query_), "Search data.gouv.fr datasets...", COLOR, data_.is_loading()))
        fetch_datasets();
    ImGui::Spacing();

    if (dataset_results_.empty()) {
        EmptyState("Search French government datasets",
                   "Enter a query and press SEARCH");
        return;
    }

    json arr = dataset_results_.is_array() ? dataset_results_
        : (dataset_results_.contains("data") ? dataset_results_["data"]
        : (dataset_results_.contains("results") ? dataset_results_["results"] : json::array()));

    if (arr.empty()) {
        EmptyState("No datasets found");
        return;
    }

    ImGui::TextColored(TEXT_DIM, "%zu datasets", arr.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##fra_ds_tbl", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Title");
        ImGui::TableSetupColumn("Organization", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableSetupColumn("Resources", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Slug");
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& d : arr) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", truncate(d.value("title", "-"), 60).c_str());
            ImGui::TableNextColumn();
            std::string org = d.contains("organization") && d["organization"].is_object()
                ? d["organization"].value("name", "") : "";
            ImGui::TextColored(TEXT_DIM, "%s", org.c_str());
            ImGui::TableNextColumn();
            if (d.contains("resources") && d["resources"].is_array())
                ImGui::TextColored(COLOR, "%zu", d["resources"].size());
            else
                ImGui::Text("-");
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", format_date(d.value("last_modified", "")).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", truncate(d.value("slug", ""), 30).c_str());
        }
        ImGui::EndTable();
    }
}

// =============================================================================
// Tabular view
// =============================================================================
void FrenchGovProvider::render_tabular() {
    using namespace theme::colors;

    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "Resource ID:");
    ImGui::SameLine(0, 8);
    ImGui::PushItemWidth(300);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputText("##fra_resid", resource_id_, sizeof(resource_id_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 8);
    if (FetchButton("PROFILE", data_.is_loading(), COLOR))
        fetch_tabular_profile();
    ImGui::SameLine(0, 4);
    if (FetchButton("DATA", data_.is_loading(), COLOR))
        fetch_tabular_data();
    ImGui::Spacing();

    // Profile
    if (!tabular_profile_.empty()) {
        ImGui::TextColored(COLOR, "RESOURCE PROFILE");
        ImGui::Spacing();
        if (tabular_profile_.is_object()) {
            for (auto& [key, val] : tabular_profile_.items()) {
                std::string vs = val.is_string() ? val.get<std::string>() : val.dump();
                if (vs.size() > 120) vs = vs.substr(0, 120) + "...";
                InfoRow(key.c_str(), vs.c_str());
            }
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // Tabular data
    if (!tabular_data_.empty()) {
        json arr = tabular_data_.is_array() ? tabular_data_
            : (tabular_data_.contains("data") ? tabular_data_["data"] : json::array());

        if (!arr.empty() && arr[0].is_object()) {
            std::vector<std::string> cols;
            for (auto& [k, _] : arr[0].items()) cols.push_back(k);
            int ncol = std::min((int)cols.size(), 10);

            ImGui::TextColored(TEXT_DIM, "%zu rows", arr.size());
            ImGui::Spacing();

            if (ImGui::BeginTable("##fra_tab_tbl", ncol,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                    ImVec2(0, 400))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                for (int c = 0; c < ncol; c++)
                    ImGui::TableSetupColumn(cols[c].c_str());
                ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor();

                for (auto& row : arr) {
                    ImGui::TableNextRow();
                    for (int c = 0; c < ncol; c++) {
                        ImGui::TableNextColumn();
                        auto& v = row[cols[c]];
                        std::string vs = v.is_string() ? v.get<std::string>() : v.dump();
                        ImGui::Text("%s", truncate(vs, 40).c_str());
                    }
                }
                ImGui::EndTable();
            }
        } else if (!arr.empty()) {
            ImGui::TextColored(TEXT_DIM, "%zu items", arr.size());
        }
    }

    if (tabular_profile_.empty() && tabular_data_.empty()) {
        EmptyState("Enter a resource ID and click PROFILE or DATA",
                   "Resource IDs can be found in dataset details");
    }
}

// =============================================================================
// Company view
// =============================================================================
void FrenchGovProvider::render_company() {
    using namespace theme::colors;

    ImGui::Spacing();
    if (SearchBar(company_query_, sizeof(company_query_), "Search French companies...", COLOR, data_.is_loading()))
        fetch_company();
    ImGui::Spacing();

    if (company_results_.empty()) {
        EmptyState("Search French company registry",
                   "Enter company name or SIREN/SIRET number");
        return;
    }

    json arr = company_results_.is_array() ? company_results_
        : (company_results_.contains("results") ? company_results_["results"]
        : (company_results_.contains("etablissements") ? company_results_["etablissements"] : json::array()));

    if (arr.empty()) {
        EmptyState("No companies found");
        return;
    }

    ImGui::TextColored(TEXT_DIM, "%zu results", arr.size());
    ImGui::Spacing();

    // Auto-detect columns from first entry
    if (arr[0].is_object()) {
        std::vector<std::string> cols;
        for (auto& [k, v] : arr[0].items()) {
            if (!v.is_object() && !v.is_array()) cols.push_back(k);
        }
        int ncol = std::min((int)cols.size(), 8);

        if (ImGui::BeginTable("##fra_comp_tbl", ncol,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            for (int c = 0; c < ncol; c++)
                ImGui::TableSetupColumn(cols[c].c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
            ImGui::TableHeadersRow();
            ImGui::PopStyleColor();

            for (auto& row : arr) {
                ImGui::TableNextRow();
                for (int c = 0; c < ncol; c++) {
                    ImGui::TableNextColumn();
                    auto& v = row[cols[c]];
                    std::string vs = v.is_string() ? v.get<std::string>() : v.dump();
                    ImGui::Text("%s", truncate(vs, 40).c_str());
                }
            }
            ImGui::EndTable();
        }
    }
}

} // namespace fincept::gov_data
