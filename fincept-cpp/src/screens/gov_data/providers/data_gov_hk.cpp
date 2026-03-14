#include "data_gov_hk.h"
#include "ui/theme.h"
#include <imgui.h>

namespace fincept::gov_data {

using json = nlohmann::json;
static constexpr auto& COLOR = provider_colors::HONG_KONG;

void DataGovHkProvider::render() {
    pickup_result();

    if (!initial_fetch_) {
        initial_fetch_ = true;
        fetch_categories();
    }

    render_toolbar();

    ImGui::BeginChild("##hk_body", ImVec2(0, 0));
    switch (active_view_) {
        case 0: render_categories(); break;
        case 1: render_datasets(datasets_); break;
        case 2: render_resources(); break;
        case 3: render_historical(); break;
        case 4: render_datasets(search_results_); break;
    }
    ImGui::EndChild();
}

void DataGovHkProvider::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##hk_toolbar", ImVec2(0, 36), ImGuiChildFlags_Borders);

    if (active_view_ == 1 || active_view_ == 2) {
        if (ImGui::SmallButton("<< BACK")) {
            if (active_view_ == 2) { active_view_ = 1; selected_dataset_.clear(); }
            else { active_view_ = 0; selected_category_.clear(); }
        }
        ImGui::SameLine(0, 8);
    }

    const char* tabs[] = {"CATEGORIES", "DATASETS", "RESOURCES", "HISTORICAL", "SEARCH"};
    for (int i = 0; i < 5; i++) {
        if (i == 2 && active_view_ != 2) continue; // Resources only when drilled down
        if (ProviderTab(tabs[i], COLOR, active_view_ == i))
            active_view_ = i;
        ImGui::SameLine(0, 2);
    }

    // Search input
    if (active_view_ == 4) {
        ImGui::SameLine(0, 12);
        if (SearchBar(search_buf_, sizeof(search_buf_), "Search HK datasets...", COLOR, data_.is_loading()))
            fetch_search();
    }

    // Historical filters
    if (active_view_ == 3) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(TEXT_DIM, "From:");
        ImGui::SameLine(0, 4);
        ImGui::PushItemWidth(80);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##hk_from", hist_start_, sizeof(hist_start_));
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 4);
        ImGui::TextColored(TEXT_DIM, "To:");
        ImGui::SameLine(0, 4);
        ImGui::PushItemWidth(80);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##hk_to", hist_end_, sizeof(hist_end_));
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 4);
        ImGui::PushItemWidth(90);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputTextWithHint("##hk_hcat", "Category", hist_category_, sizeof(hist_category_));
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 4);
        ImGui::PushItemWidth(70);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputTextWithHint("##hk_hfmt", "Format", hist_format_, sizeof(hist_format_));
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 4);
        if (FetchButton("FETCH", data_.is_loading(), COLOR))
            fetch_historical();
    }

    // Breadcrumb
    if (active_view_ == 1 && !selected_category_.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(TEXT_DIM, "Categories >");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(COLOR, "%s", truncate(selected_category_, 40).c_str());
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

// Fetch
void DataGovHkProvider::fetch_categories() {
    data_.run_script("data_gov_hk_api.py", {"--action", "categories"});
    status_ = "Fetching categories..."; status_time_ = ImGui::GetTime();
}
void DataGovHkProvider::fetch_datasets(const std::string& category_id) {
    selected_category_ = category_id;
    data_.run_script("data_gov_hk_api.py", {"--action", "datasets", "--category", category_id});
    status_ = "Fetching datasets..."; status_time_ = ImGui::GetTime();
}
void DataGovHkProvider::fetch_resources(const std::string& dataset_id) {
    selected_dataset_ = dataset_id;
    data_.run_script("data_gov_hk_api.py", {"--action", "resources", "--dataset", dataset_id});
    status_ = "Fetching resources..."; status_time_ = ImGui::GetTime();
}
void DataGovHkProvider::fetch_historical() {
    std::vector<std::string> args = {"--action", "historical"};
    if (hist_start_[0] != '\0') args.insert(args.end(), {"--start-date", hist_start_});
    if (hist_end_[0] != '\0') args.insert(args.end(), {"--end-date", hist_end_});
    if (hist_category_[0] != '\0') args.insert(args.end(), {"--category", hist_category_});
    if (hist_format_[0] != '\0') args.insert(args.end(), {"--format", hist_format_});
    data_.run_script("data_gov_hk_api.py", args);
    status_ = "Fetching historical..."; status_time_ = ImGui::GetTime();
}
void DataGovHkProvider::fetch_search() {
    if (search_buf_[0] == '\0') return;
    data_.run_script("data_gov_hk_api.py", {"--action", "search", "--query", search_buf_});
    status_ = "Searching..."; status_time_ = ImGui::GetTime();
}

void DataGovHkProvider::pickup_result() {
    if (!data_.has_result() && data_.error().empty()) return;
    std::lock_guard<std::mutex> lock(data_.mutex());
    if (data_.has_result()) {
        const auto& r = data_.result();
        switch (active_view_) {
            case 0: parse_categories(r); break;
            case 1: parse_datasets(r, datasets_); break;
            case 2: parse_resources(r); break;
            case 3: parse_historical(r); break;
            case 4: parse_datasets(r, search_results_); break;
        }
        status_ = "Data loaded";
    } else if (!data_.error().empty()) {
        status_ = "Error: " + data_.error();
    }
    status_time_ = ImGui::GetTime();
    data_.clear();
}

// Parse
void DataGovHkProvider::parse_categories(const json& j) {
    categories_.clear();
    json arr = j.is_array() ? j : (j.contains("result") ? j["result"] : json::array());
    for (auto& c : arr) {
        OrganizationRecord rec;
        rec.id = c.value("id", c.value("name", ""));
        rec.name = c.value("name", rec.id);
        rec.display_name = c.value("display_name", c.value("title", rec.name));
        rec.description = c.value("description", "");
        rec.dataset_count = c.value("package_count", c.value("dataset_count", 0));
        categories_.push_back(std::move(rec));
    }
}

void DataGovHkProvider::parse_datasets(const json& j, std::vector<DatasetRecord>& out) {
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
        rec.notes = d.value("notes", "");
        rec.modified = d.value("metadata_modified", "");
        rec.num_resources = d.value("num_resources", 0);
        if (d.contains("tags") && d["tags"].is_array())
            for (auto& t : d["tags"]) {
                if (t.is_string()) rec.tags.push_back(t.get<std::string>());
                else if (t.is_object()) rec.tags.push_back(t.value("name", ""));
            }
        out.push_back(std::move(rec));
    }
}

void DataGovHkProvider::parse_resources(const json& j) {
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

void DataGovHkProvider::parse_historical(const json& j) {
    historical_.clear();
    json arr = j.is_array() ? j : (j.contains("result") ? j["result"] : json::array());
    for (auto& f : arr) {
        HkHistoricalFile rec;
        rec.name = f.value("name", f.value("filename", ""));
        rec.format = f.value("format", f.value("file_format", ""));
        rec.category = f.value("category", "");
        rec.date = f.value("date", f.value("reference_date", ""));
        rec.url = f.value("url", f.value("download_url", ""));
        rec.size = f.value("size", (int64_t)0);
        historical_.push_back(std::move(rec));
    }
}

// Render
void DataGovHkProvider::render_categories() {
    using namespace theme::colors;
    if (data_.is_loading() && categories_.empty()) { LoadingState("Loading...", COLOR); return; }
    if (categories_.empty()) { EmptyState("No categories loaded"); return; }

    ImGui::TextColored(TEXT_DIM, "%zu categories", categories_.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##hk_cat_tbl", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Display Name", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Description");
        ImGui::TableSetupColumn("Datasets", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& c : categories_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(c.id.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                active_view_ = 1;
                fetch_datasets(c.id);
            }
            ImGui::TableNextColumn();
            ImGui::Text("%s", c.display_name != c.id ? c.display_name.c_str() : "");
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", truncate(c.description, 80).c_str());
            ImGui::TableNextColumn();
            if (c.dataset_count > 0)
                ImGui::TextColored(COLOR, "%d", c.dataset_count);
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, ">");
        }
        ImGui::EndTable();
    }
}

void DataGovHkProvider::render_datasets(const std::vector<DatasetRecord>& ds) {
    using namespace theme::colors;
    if (data_.is_loading() && ds.empty()) { LoadingState("Loading...", COLOR); return; }
    if (ds.empty()) {
        EmptyState("No datasets",
            active_view_ == 4 ? "Enter a search query" : "Select a category");
        return;
    }

    ImGui::TextColored(TEXT_DIM, "%zu datasets", ds.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##hk_ds_tbl", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Title");
        ImGui::TableSetupColumn("Resources", ImGuiTableColumnFlags_WidthFixed, 70);
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
                fetch_resources(d.id.empty() ? d.name : d.id);
            }
            if (!d.notes.empty() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", truncate(d.notes, 200).c_str());
            ImGui::TableNextColumn();
            if (d.num_resources > 0)
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

void DataGovHkProvider::render_resources() {
    using namespace theme::colors;
    if (data_.is_loading() && resources_.empty()) { LoadingState("Loading...", COLOR); return; }
    if (resources_.empty()) { EmptyState("No resources", "Select a dataset"); return; }

    ImGui::TextColored(TEXT_DIM, "%zu resources", resources_.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##hk_res_tbl", 5,
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

// Historical table
void DataGovHkProvider::render_historical() {
    using namespace theme::colors;

    if (data_.is_loading() && historical_.empty()) {
        LoadingState("Loading...", COLOR);
        return;
    }

    if (historical_.empty()) {
        EmptyState("Query historical archive",
                   "Set date range (YYYYMMDD) and click FETCH");
        return;
    }

    ImGui::TextColored(TEXT_DIM, "%zu historical files", historical_.size());
    ImGui::Spacing();

    if (ImGui::BeginTable("##hk_hist_tbl", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("File Name");
        ImGui::TableSetupColumn("Format", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("URL");
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& f : historical_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", truncate(f.name, 60).c_str());
            ImGui::TableNextColumn();
            if (!f.format.empty())
                ImGui::TextColored(COLOR, "%s", f.format.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", f.category.empty() ? "-" : f.category.c_str());
            ImGui::TableNextColumn();
            // Format YYYYMMDD -> YYYY-MM-DD
            if (f.date.size() == 8) {
                char formatted[16];
                std::snprintf(formatted, sizeof(formatted), "%s-%s-%s",
                    f.date.substr(0, 4).c_str(), f.date.substr(4, 2).c_str(), f.date.substr(6, 2).c_str());
                ImGui::TextColored(TEXT_DIM, "%s", formatted);
            } else {
                ImGui::TextColored(TEXT_DIM, "%s", f.date.empty() ? "-" : f.date.c_str());
            }
            ImGui::TableNextColumn();
            ImGui::Text("%s", format_size(f.size).c_str());
            ImGui::TableNextColumn();
            if (!f.url.empty()) {
                ImGui::TextColored(COLOR, "%s", truncate(f.url, 40).c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.url.c_str());
            } else ImGui::TextColored(TEXT_DIM, "-");
        }
        ImGui::EndTable();
    }
}

} // namespace fincept::gov_data
