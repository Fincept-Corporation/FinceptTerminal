#include "canada_gov.h"
#include "ui/theme.h"
#include <imgui.h>

namespace fincept::gov_data {

using json = nlohmann::json;
static constexpr auto& COLOR = provider_colors::CANADA;

void CanadaGovProvider::render() {
    pickup_result();

    if (!initial_fetch_) {
        initial_fetch_ = true;
        fetch_publishers();
    }

    render_toolbar();

    ImGui::BeginChild("##can_body", ImVec2(0, 0));
    switch (active_view_) {
        case 0: render_publishers(); break;
        case 1: render_datasets(datasets_, datasets_total_); break;
        case 2: render_resources(); break;
        case 3: render_datasets(search_results_, search_total_); break;
        case 4: render_datasets(recent_, (int)recent_.size()); break;
    }
    ImGui::EndChild();
}

// =============================================================================
// Toolbar
// =============================================================================
void CanadaGovProvider::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##can_toolbar", ImVec2(0, 36), ImGuiChildFlags_Borders);

    // Back button for drill-down views
    if (active_view_ == 1 || active_view_ == 2) {
        if (ImGui::SmallButton("<< BACK")) {
            if (active_view_ == 2) { active_view_ = 1; }
            else { active_view_ = 0; selected_publisher_.clear(); }
        }
        ImGui::SameLine(0, 8);
    }

    // View tabs
    const char* tabs[] = {"PUBLISHERS", "DATASETS", "RESOURCES", "SEARCH", "RECENT"};
    for (int i = 0; i < 5; i++) {
        if (ProviderTab(tabs[i], COLOR, active_view_ == i)) {
            active_view_ = i;
            if (i == 4 && recent_.empty()) fetch_recent();
        }
        ImGui::SameLine(0, 2);
    }

    // Search input
    if (active_view_ == 3) {
        ImGui::SameLine(0, 12);
        if (SearchBar(search_buf_, sizeof(search_buf_), "Search Canadian datasets...", COLOR, data_.is_loading()))
            fetch_search();
    }

    // Breadcrumb
    if (active_view_ == 1 && !selected_publisher_.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(TEXT_DIM, ">");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(COLOR, "%s", truncate(selected_publisher_, 30).c_str());
        ImGui::SameLine(0, 4);
        ImGui::TextColored(TEXT_DIM, "(%d datasets)", datasets_total_);
    }

    ImGui::SameLine(0, 12);
    StatusMsg(status_, status_time_);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Fetch functions
// =============================================================================
void CanadaGovProvider::fetch_publishers() {
    data_.run_script("canada_gov_api.py", {"--action", "publishers"});
    status_ = "Fetching publishers...";
    status_time_ = ImGui::GetTime();
}

void CanadaGovProvider::fetch_datasets(const std::string& publisher_id) {
    selected_publisher_ = publisher_id;
    data_.run_script("canada_gov_api.py", {"--action", "datasets", "--publisher", publisher_id});
    status_ = "Fetching datasets...";
    status_time_ = ImGui::GetTime();
}

void CanadaGovProvider::fetch_resources(const std::string& dataset_id) {
    selected_dataset_ = dataset_id;
    data_.run_script("canada_gov_api.py", {"--action", "resources", "--dataset", dataset_id});
    status_ = "Fetching resources...";
    status_time_ = ImGui::GetTime();
}

void CanadaGovProvider::fetch_search() {
    if (search_buf_[0] == '\0') return;
    data_.run_script("canada_gov_api.py", {"--action", "search", "--query", search_buf_});
    status_ = "Searching...";
    status_time_ = ImGui::GetTime();
}

void CanadaGovProvider::fetch_recent() {
    data_.run_script("canada_gov_api.py", {"--action", "recent"});
    status_ = "Fetching recent...";
    status_time_ = ImGui::GetTime();
}

// =============================================================================
// Pickup result
// =============================================================================
void CanadaGovProvider::pickup_result() {
    if (!data_.has_result() && data_.error().empty()) return;
    std::lock_guard<std::mutex> lock(data_.mutex());

    if (data_.has_result()) {
        const auto& r = data_.result();
        switch (active_view_) {
            case 0: parse_publishers(r); break;
            case 1: parse_datasets(r, datasets_, datasets_total_); break;
            case 2: parse_resources(r); break;
            case 3: parse_datasets(r, search_results_, search_total_); break;
            case 4: { int dummy = 0; parse_datasets(r, recent_, dummy); } break;
        }
        status_ = "Data loaded";
    } else if (!data_.error().empty()) {
        status_ = "Error: " + data_.error();
    }
    status_time_ = ImGui::GetTime();
    data_.clear();
}

// =============================================================================
// Parse helpers
// =============================================================================
void CanadaGovProvider::parse_publishers(const json& j) {
    publishers_.clear();
    json arr = j.is_array() ? j : (j.contains("result") ? j["result"] : json::array());
    for (auto& o : arr) {
        OrganizationRecord rec;
        rec.id = o.value("id", "");
        rec.name = o.value("name", "");
        rec.display_name = o.value("display_name", o.value("title", rec.name));
        rec.description = o.value("description", "");
        rec.dataset_count = o.value("package_count", o.value("dataset_count", 0));
        rec.created = o.value("created", "");
        publishers_.push_back(std::move(rec));
    }
}

void CanadaGovProvider::parse_datasets(const json& j, std::vector<DatasetRecord>& out, int& total) {
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
        rec.modified = d.value("metadata_modified", d.value("modified", ""));
        rec.created = d.value("metadata_created", d.value("created", ""));
        rec.num_resources = d.value("num_resources", 0);
        if (d.contains("tags") && d["tags"].is_array()) {
            for (auto& t : d["tags"]) {
                if (t.is_string()) rec.tags.push_back(t.get<std::string>());
                else if (t.is_object()) rec.tags.push_back(t.value("name", t.value("display_name", "")));
            }
        }
        out.push_back(std::move(rec));
    }
}

void CanadaGovProvider::parse_resources(const json& j) {
    resources_.clear();
    json arr = j.is_array() ? j : (j.contains("result") ? j["result"] : (j.contains("resources") ? j["resources"] : json::array()));
    for (auto& r : arr) {
        ResourceRecord rec;
        rec.id = r.value("id", "");
        rec.name = r.value("name", r.value("title", ""));
        rec.description = r.value("description", "");
        rec.format = r.value("format", "");
        rec.url = r.value("url", "");
        rec.modified = r.value("last_modified", r.value("modified", ""));
        rec.size = r.value("size", (int64_t)0);
        resources_.push_back(std::move(rec));
    }
}

// =============================================================================
// Render: Publishers
// =============================================================================
void CanadaGovProvider::render_publishers() {
    using namespace theme::colors;

    if (data_.is_loading() && publishers_.empty()) {
        LoadingState("Loading publishers...", COLOR);
        return;
    }

    if (publishers_.empty()) {
        EmptyState("No publishers loaded", "Data will load automatically");
        return;
    }

    ImGui::TextColored(TEXT_DIM, "%zu publishers", publishers_.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##can_pub_tbl", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Datasets");
        ImGui::TableSetupColumn("");
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& pub : publishers_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(pub.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                active_view_ = 1;
                fetch_datasets(pub.name);
            }
            ImGui::TableNextColumn();
            ImGui::Text("%s", pub.display_name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(COLOR, "%d", pub.dataset_count);
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, ">");
        }
        ImGui::EndTable();
    }
}

// =============================================================================
// Render: Datasets
// =============================================================================
void CanadaGovProvider::render_datasets(const std::vector<DatasetRecord>& ds, int total) {
    using namespace theme::colors;

    if (data_.is_loading() && ds.empty()) {
        LoadingState("Loading datasets...", COLOR);
        return;
    }

    if (ds.empty()) {
        EmptyState("No datasets found", active_view_ == 3 ? "Enter a search query above" : "Select a publisher");
        return;
    }

    ImGui::TextColored(TEXT_DIM, "Showing %zu of %d datasets", ds.size(), total);
    ImGui::Spacing();

    if (ImGui::BeginTable("##can_ds_tbl", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Title");
        ImGui::TableSetupColumn("Organization", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Tags");
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
            // Tags
            std::string tag_str;
            for (size_t i = 0; i < d.tags.size() && i < 3; i++) {
                if (i > 0) tag_str += ", ";
                tag_str += truncate(d.tags[i], 20);
            }
            if (d.tags.size() > 3)
                tag_str += " +" + std::to_string(d.tags.size() - 3);
            ImGui::TextColored(TEXT_DIM, "%s", tag_str.c_str());
        }
        ImGui::EndTable();
    }
}

// =============================================================================
// Render: Resources
// =============================================================================
void CanadaGovProvider::render_resources() {
    using namespace theme::colors;

    if (data_.is_loading() && resources_.empty()) {
        LoadingState("Loading resources...", COLOR);
        return;
    }

    if (resources_.empty()) {
        EmptyState("No resources found", "Select a dataset to view its resources");
        return;
    }

    ImGui::TextColored(TEXT_DIM, "%zu resources", resources_.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##can_res_tbl", 5,
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
            } else {
                ImGui::TextColored(TEXT_DIM, "-");
            }
        }
        ImGui::EndTable();
    }
}

} // namespace fincept::gov_data
