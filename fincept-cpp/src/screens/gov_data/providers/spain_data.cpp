#include "spain_data.h"
#include "ui/theme.h"
#include <imgui.h>

namespace fincept::gov_data {

using json = nlohmann::json;
static constexpr auto& COLOR = provider_colors::SPAIN;

void SpainDataProvider::render() {
    pickup_result();

    if (!initial_fetch_) {
        initial_fetch_ = true;
        fetch_catalogues();
    }

    render_toolbar();

    ImGui::BeginChild("##esp_body", ImVec2(0, 0));
    switch (active_view_) {
        case 0: render_catalogues(); break;
        case 1: render_datasets(datasets_); break;
        case 2: render_resources(); break;
        case 3: render_datasets(search_results_); break;
    }
    ImGui::EndChild();
}

void SpainDataProvider::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##esp_toolbar", ImVec2(0, 36), ImGuiChildFlags_Borders);

    if (active_view_ == 1 || active_view_ == 2) {
        if (ImGui::SmallButton("<< BACK")) {
            if (active_view_ == 2) active_view_ = 1;
            else { active_view_ = 0; selected_catalogue_.clear(); }
        }
        ImGui::SameLine(0, 8);
    }

    const char* tabs[] = {"CATALOGUES", "DATASETS", "RESOURCES", "SEARCH"};
    for (int i = 0; i < 4; i++) {
        if (ProviderTab(tabs[i], COLOR, active_view_ == i))
            active_view_ = i;
        ImGui::SameLine(0, 2);
    }

    if (active_view_ == 3) {
        ImGui::SameLine(0, 12);
        if (SearchBar(search_buf_, sizeof(search_buf_), "Search Spanish datasets...", COLOR, data_.is_loading()))
            fetch_search();
    }

    // Page nav for datasets
    if (active_view_ == 1) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(TEXT_DIM, "Page %d", page_);
        ImGui::SameLine(0, 4);
        if (ImGui::SmallButton("< Prev") && page_ > 1) { page_--; fetch_datasets(selected_catalogue_); }
        ImGui::SameLine(0, 4);
        if (ImGui::SmallButton("Next >")) { page_++; fetch_datasets(selected_catalogue_); }
    }

    ImGui::SameLine(0, 12);
    StatusMsg(status_, status_time_);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// Fetch
void SpainDataProvider::fetch_catalogues() {
    data_.run_script("spain_data.py", {"--action", "catalogues"});
    status_ = "Fetching catalogues..."; status_time_ = ImGui::GetTime();
}
void SpainDataProvider::fetch_datasets(const std::string& catalogue_id) {
    if (!catalogue_id.empty()) selected_catalogue_ = catalogue_id;
    std::vector<std::string> args = {"--action", "datasets", "--page", std::to_string(page_)};
    if (!selected_catalogue_.empty())
        args.insert(args.end(), {"--catalogue", selected_catalogue_});
    data_.run_script("spain_data.py", args);
    status_ = "Fetching datasets..."; status_time_ = ImGui::GetTime();
}
void SpainDataProvider::fetch_resources(const std::string& dataset_id) {
    selected_dataset_ = dataset_id;
    data_.run_script("spain_data.py", {"--action", "resources", "--dataset", dataset_id});
    status_ = "Fetching resources..."; status_time_ = ImGui::GetTime();
}
void SpainDataProvider::fetch_search() {
    if (search_buf_[0] == '\0') return;
    data_.run_script("spain_data.py", {"--action", "search", "--query", search_buf_});
    status_ = "Searching..."; status_time_ = ImGui::GetTime();
}

void SpainDataProvider::pickup_result() {
    if (!data_.has_result() && data_.error().empty()) return;
    std::lock_guard<std::mutex> lock(data_.mutex());
    if (data_.has_result()) {
        const auto& r = data_.result();
        switch (active_view_) {
            case 0: parse_catalogues(r); break;
            case 1: parse_datasets(r, datasets_); break;
            case 2: parse_resources(r); break;
            case 3: parse_datasets(r, search_results_); break;
        }
        status_ = "Data loaded";
    } else if (!data_.error().empty()) {
        status_ = "Error: " + data_.error();
    }
    status_time_ = ImGui::GetTime();
    data_.clear();
}

void SpainDataProvider::parse_catalogues(const json& j) {
    catalogues_.clear();
    json arr = j.is_array() ? j : (j.contains("result") ? j["result"] : json::array());
    for (auto& c : arr) {
        std::string id = c.value("id", c.value("name", ""));
        std::string uri = c.value("uri", c.value("url", ""));
        catalogues_.emplace_back(id, uri);
    }
}

void SpainDataProvider::parse_datasets(const json& j, std::vector<DatasetRecord>& out) {
    out.clear();
    json arr = j.is_array() ? j
        : (j.contains("results") ? j["results"]
        : (j.contains("result") && j["result"].is_object() && j["result"].contains("results")
            ? j["result"]["results"] : json::array()));
    for (auto& d : arr) {
        DatasetRecord rec;
        rec.id = d.value("id", "");
        rec.name = d.value("name", "");
        rec.title = d.value("title", rec.name);
        rec.notes = d.value("notes", d.value("description", ""));
        rec.modified = d.value("metadata_modified", d.value("modified", ""));
        rec.num_resources = d.value("num_resources", 0);
        if (d.contains("tags") && d["tags"].is_array())
            for (auto& t : d["tags"]) {
                if (t.is_string()) rec.tags.push_back(t.get<std::string>());
                else if (t.is_object()) rec.tags.push_back(t.value("name", ""));
            }
        out.push_back(std::move(rec));
    }
}

void SpainDataProvider::parse_resources(const json& j) {
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

// Render
void SpainDataProvider::render_catalogues() {
    using namespace theme::colors;
    if (data_.is_loading() && catalogues_.empty()) { LoadingState("Loading...", COLOR); return; }
    if (catalogues_.empty()) { EmptyState("No catalogues loaded"); return; }

    ImGui::TextColored(TEXT_DIM, "%zu catalogues", catalogues_.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##esp_cat_tbl", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Catalogue ID");
        ImGui::TableSetupColumn("URI");
        ImGui::TableSetupColumn("");
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& [id, uri] : catalogues_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(id.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                active_view_ = 1;
                page_ = 1;
                fetch_datasets(id);
            }
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", truncate(uri, 60).c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, ">");
        }
        ImGui::EndTable();
    }
}

void SpainDataProvider::render_datasets(const std::vector<DatasetRecord>& ds) {
    using namespace theme::colors;
    if (data_.is_loading() && ds.empty()) { LoadingState("Loading...", COLOR); return; }
    if (ds.empty()) { EmptyState("No datasets found"); return; }

    ImGui::TextColored(TEXT_DIM, "%zu datasets", ds.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##esp_ds_tbl", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Title");
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
            ImGui::TextColored(TEXT_DIM, "%s", tag_str.c_str());
        }
        ImGui::EndTable();
    }
}

void SpainDataProvider::render_resources() {
    using namespace theme::colors;
    if (data_.is_loading() && resources_.empty()) { LoadingState("Loading...", COLOR); return; }
    if (resources_.empty()) { EmptyState("No resources", "Select a dataset"); return; }

    ImGui::TextColored(TEXT_DIM, "%zu resources", resources_.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##esp_res_tbl", 5,
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
