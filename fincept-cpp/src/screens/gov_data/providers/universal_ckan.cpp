#include "universal_ckan.h"
#include "ui/theme.h"
#include <imgui.h>

namespace fincept::gov_data {

using json = nlohmann::json;
static constexpr auto& COLOR = provider_colors::CKAN;

const char* UniversalCkanProvider::portal_code() const {
    auto& portals = ckan_portals();
    if (active_portal_ >= 0 && active_portal_ < (int)portals.size())
        return portals[active_portal_].code;
    return "us";
}

void UniversalCkanProvider::render() {
    pickup_result();

    if (!initial_fetch_) {
        initial_fetch_ = true;
        fetch_organizations();
    }

    render_portal_bar();
    render_toolbar();

    ImGui::BeginChild("##ckan_body", ImVec2(0, 0));
    switch (active_view_) {
        case 0: render_organizations(); break;
        case 1: render_datasets(datasets_, datasets_total_); break;
        case 2: render_resources(); break;
        case 3: render_datasets(search_results_, search_total_); break;
    }
    ImGui::EndChild();
}

// =============================================================================
// Portal selector bar
// =============================================================================
void UniversalCkanProvider::render_portal_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(COLOR.x, COLOR.y, COLOR.z, 0.03f));
    ImGui::BeginChild("##ckan_portal_bar", ImVec2(0, 28), ImGuiChildFlags_Borders);

    ImGui::TextColored(COLOR, "PORTAL:");
    ImGui::SameLine(0, 8);

    auto& portals = ckan_portals();
    for (int i = 0; i < (int)portals.size(); i++) {
        bool active = (i == active_portal_);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, COLOR);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COLOR);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));

        char label[32];
        std::snprintf(label, sizeof(label), "%s %s", portals[i].flag, portals[i].code);
        // Make uppercase
        for (char* p = label; *p; p++) {
            if (*p >= 'a' && *p <= 'z') *p -= 32;
        }

        if (ImGui::Button(label)) {
            if (i != active_portal_) {
                active_portal_ = i;
                active_view_ = 0;
                organizations_.clear();
                datasets_.clear();
                resources_.clear();
                search_results_.clear();
                selected_org_.clear();
                selected_dataset_.clear();
                initial_fetch_ = false;
                fetch_organizations();
                initial_fetch_ = true;
            }
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 2);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Toolbar
// =============================================================================
void UniversalCkanProvider::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ckan_toolbar", ImVec2(0, 36), ImGuiChildFlags_Borders);

    if (active_view_ == 1 || active_view_ == 2) {
        if (ImGui::SmallButton("<< BACK")) {
            if (active_view_ == 2) active_view_ = 1;
            else { active_view_ = 0; selected_org_.clear(); }
        }
        ImGui::SameLine(0, 8);
    }

    const char* tabs[] = {"ORGANIZATIONS", "DATASETS", "RESOURCES", "SEARCH"};
    for (int i = 0; i < 4; i++) {
        if (i == 2 && active_view_ != 2) continue; // Resources only visible when drilled down
        if (ProviderTab(tabs[i], COLOR, active_view_ == i))
            active_view_ = i;
        ImGui::SameLine(0, 2);
    }

    if (active_view_ == 3) {
        ImGui::SameLine(0, 12);
        auto& portals = ckan_portals();
        char hint[64];
        std::snprintf(hint, sizeof(hint), "Search %s datasets...", portals[active_portal_].name);
        if (SearchBar(search_buf_, sizeof(search_buf_), hint, COLOR, data_.is_loading()))
            fetch_search();
    }

    // Breadcrumb
    if (active_view_ == 1 && !selected_org_.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(TEXT_DIM, ">");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(COLOR, "%s", truncate(selected_org_, 30).c_str());
        ImGui::SameLine(0, 4);
        ImGui::TextColored(TEXT_DIM, "(%d)", datasets_total_);
    }
    if (active_view_ == 2 && !selected_dataset_.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(TEXT_DIM, ">");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(COLOR, "%s", truncate(selected_dataset_, 30).c_str());
    }

    ImGui::SameLine(0, 12);
    StatusMsg(status_, status_time_);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Fetch — uses run_ckan() for country+action pattern
// =============================================================================
void UniversalCkanProvider::fetch_organizations() {
    data_.run_ckan(portal_code(), "organizations");
    status_ = "Fetching organizations..."; status_time_ = ImGui::GetTime();
}
void UniversalCkanProvider::fetch_datasets(const std::string& org_name) {
    selected_org_ = org_name;
    data_.run_ckan(portal_code(), "datasets", {"--organization", org_name});
    status_ = "Fetching datasets..."; status_time_ = ImGui::GetTime();
}
void UniversalCkanProvider::fetch_resources(const std::string& dataset_id) {
    selected_dataset_ = dataset_id;
    data_.run_ckan(portal_code(), "resources", {"--dataset", dataset_id});
    status_ = "Fetching resources..."; status_time_ = ImGui::GetTime();
}
void UniversalCkanProvider::fetch_search() {
    if (search_buf_[0] == '\0') return;
    data_.run_ckan(portal_code(), "search", {"--query", search_buf_});
    status_ = "Searching..."; status_time_ = ImGui::GetTime();
}

void UniversalCkanProvider::pickup_result() {
    if (!data_.has_result() && data_.error().empty()) return;
    std::lock_guard<std::mutex> lock(data_.mutex());
    if (data_.has_result()) {
        const auto& r = data_.result();
        switch (active_view_) {
            case 0: parse_organizations(r); break;
            case 1: parse_datasets(r, datasets_, datasets_total_); break;
            case 2: parse_resources(r); break;
            case 3: parse_datasets(r, search_results_, search_total_); break;
        }
        status_ = "Data loaded";
    } else if (!data_.error().empty()) {
        status_ = "Error: " + data_.error();
    }
    status_time_ = ImGui::GetTime();
    data_.clear();
}

// Parse — same CKAN pattern
void UniversalCkanProvider::parse_organizations(const json& j) {
    organizations_.clear();
    json arr = j.is_array() ? j : (j.contains("result") ? j["result"] : json::array());
    for (auto& o : arr) {
        OrganizationRecord rec;
        rec.id = o.value("id", "");
        rec.name = o.value("name", "");
        rec.display_name = o.value("display_name", o.value("title", rec.name));
        rec.description = o.value("description", "");
        rec.dataset_count = o.value("package_count", o.value("dataset_count", 0));
        organizations_.push_back(std::move(rec));
    }
}

void UniversalCkanProvider::parse_datasets(const json& j, std::vector<DatasetRecord>& out, int& total) {
    out.clear();
    json arr = j.is_array() ? j
        : (j.contains("results") ? j["results"]
        : (j.contains("result") && j["result"].is_object() && j["result"].contains("results")
            ? j["result"]["results"] : json::array()));
    total = j.contains("count") ? j["count"].get<int>()
          : (j.contains("result") && j["result"].is_object() ? j["result"].value("count", (int)arr.size()) : (int)arr.size());
    for (auto& d : arr) {
        DatasetRecord rec;
        rec.id = d.value("id", "");
        rec.name = d.value("name", "");
        rec.title = d.value("title", d.value("display_name", rec.name));
        rec.notes = d.value("notes", "");
        rec.organization = d.contains("organization") && d["organization"].is_object()
            ? d["organization"].value("title", d["organization"].value("name", ""))
            : d.value("organization_name", "");
        rec.modified = d.value("metadata_modified", "");
        rec.num_resources = d.value("num_resources", 0);
        if (d.contains("tags") && d["tags"].is_array())
            for (auto& t : d["tags"]) {
                if (t.is_string()) rec.tags.push_back(t.get<std::string>());
                else if (t.is_object()) rec.tags.push_back(t.value("name", t.value("display_name", "")));
            }
        out.push_back(std::move(rec));
    }
}

void UniversalCkanProvider::parse_resources(const json& j) {
    resources_.clear();
    json arr = j.is_array() ? j : (j.contains("result") ? j["result"] : (j.contains("resources") ? j["resources"] : json::array()));
    for (auto& r : arr) {
        ResourceRecord rec;
        rec.id = r.value("id", "");
        rec.name = r.value("name", r.value("title", ""));
        rec.description = r.value("description", "");
        rec.format = r.value("format", "");
        rec.url = r.value("url", "");
        rec.modified = r.value("last_modified", "");
        rec.size = r.value("size", (int64_t)0);
        resources_.push_back(std::move(rec));
    }
}

// Render — same table layout
void UniversalCkanProvider::render_organizations() {
    using namespace theme::colors;
    if (data_.is_loading() && organizations_.empty()) { LoadingState("Loading...", COLOR); return; }
    if (organizations_.empty()) { EmptyState("No organizations loaded"); return; }

    auto& portals = ckan_portals();
    ImGui::TextColored(TEXT_DIM, "%zu organizations on %s", organizations_.size(), portals[active_portal_].name);
    ImGui::Spacing();

    if (ImGui::BeginTable("##ckan_org_tbl", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ORGANIZATION ID");
        ImGui::TableSetupColumn("DISPLAY NAME");
        ImGui::TableSetupColumn("");
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& org : organizations_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(org.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                active_view_ = 1;
                fetch_datasets(org.name);
            }
            ImGui::TableNextColumn();
            ImGui::Text("%s", org.display_name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, ">");
        }
        ImGui::EndTable();
    }
}

void UniversalCkanProvider::render_datasets(const std::vector<DatasetRecord>& ds, int total) {
    using namespace theme::colors;
    if (data_.is_loading() && ds.empty()) { LoadingState("Loading...", COLOR); return; }
    if (ds.empty()) { EmptyState("No datasets", active_view_ == 3 ? "Enter a search query" : "Select an organization"); return; }

    ImGui::TextColored(TEXT_DIM, "Showing %zu of %d datasets", ds.size(), total);
    ImGui::Spacing();

    if (ImGui::BeginTable("##ckan_ds_tbl", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Title");
        ImGui::TableSetupColumn("Organization", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Tags");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& d : ds) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            std::string label = truncate(d.title.empty() ? d.name : d.title, 80);
            if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                active_view_ = 2;
                fetch_resources(d.name.empty() ? d.id : d.name);
            }
            if (!d.notes.empty() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", truncate(d.notes, 200).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", d.organization.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(COLOR, "%d", d.num_resources);
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", format_date(d.modified).c_str());
            ImGui::TableNextColumn();
            std::string tag_str;
            for (size_t i = 0; i < d.tags.size() && i < 3; i++) {
                if (i > 0) tag_str += ", ";
                tag_str += truncate(d.tags[i], 20);
            }
            if (d.tags.size() > 3) tag_str += " +" + std::to_string(d.tags.size() - 3);
            ImGui::TextColored(TEXT_DIM, "%s", tag_str.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, ">");
        }
        ImGui::EndTable();
    }
}

void UniversalCkanProvider::render_resources() {
    using namespace theme::colors;
    if (data_.is_loading() && resources_.empty()) { LoadingState("Loading...", COLOR); return; }
    if (resources_.empty()) { EmptyState("No resources", "Select a dataset"); return; }

    ImGui::TextColored(TEXT_DIM, "%zu resources", resources_.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##ckan_res_tbl", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("URL");
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& r : resources_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", (r.name.empty() ? r.id : r.name).c_str());
            if (!r.description.empty() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", r.description.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(COLOR, "%s", r.format.empty() ? "?" : r.format.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", format_size(r.size).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", format_date(r.modified).c_str());
            ImGui::TableNextColumn();
            if (!r.url.empty()) {
                ImGui::TextColored(COLOR, "%s", truncate(r.url, 50).c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", r.url.c_str());
            } else ImGui::TextColored(TEXT_DIM, "-");
        }
        ImGui::EndTable();
    }
}

} // namespace fincept::gov_data
