#include "pxweb.h"
#include "ui/theme.h"
#include <imgui.h>

namespace fincept::gov_data {

using json = nlohmann::json;
static constexpr auto& COLOR = provider_colors::FINLAND;

void PxWebProvider::render() {
    pickup_result();

    if (!initial_fetch_) {
        initial_fetch_ = true;
        fetch_nodes(""); // root
    }

    render_toolbar();

    ImGui::BeginChild("##pxw_body", ImVec2(0, 0));
    switch (active_view_) {
        case 0: render_browse();    break;
        case 1: render_metadata();  break;
        case 2: render_data_view(); break;
    }
    ImGui::EndChild();
}

void PxWebProvider::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##pxw_toolbar", ImVec2(0, 36), ImGuiChildFlags_Borders);

    const char* tabs[] = {"BROWSE", "METADATA", "DATA"};
    for (int i = 0; i < 3; i++) {
        if (ProviderTab(tabs[i], COLOR, active_view_ == i))
            active_view_ = i;
        ImGui::SameLine(0, 2);
    }

    // Navigation path
    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);

    if (!path_stack_.empty()) {
        if (ImGui::SmallButton("<< BACK")) {
            navigate_back();
        }
        ImGui::SameLine(0, 8);
    }

    // Breadcrumb
    ImGui::TextColored(TEXT_DIM, "Path:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(COLOR, "%s", current_path_.empty() ? "/" : current_path_.c_str());

    if (!selected_table_.empty()) {
        ImGui::SameLine(0, 8);
        ImGui::TextColored(TEXT_DIM, "Table:");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(COLOR, "%s", selected_table_.c_str());
    }

    ImGui::SameLine(0, 12);
    StatusMsg(status_, status_time_);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// Navigation
void PxWebProvider::fetch_nodes(const std::string& path) {
    current_path_ = path;
    std::vector<std::string> args = {"--action", "browse"};
    if (!path.empty()) args.insert(args.end(), {"--path", path});
    data_.run_script("pxweb_fetcher.py", args);
    status_ = "Loading..."; status_time_ = ImGui::GetTime();
}

void PxWebProvider::navigate_into(const NodeEntry& node) {
    if (node.type == 't') {
        // Table: fetch metadata
        selected_table_ = node.id;
        table_label_ = node.text;
        std::string table_path = current_path_.empty() ? node.id : current_path_ + "/" + node.id;
        data_.run_script("pxweb_fetcher.py", {"--action", "metadata", "--path", table_path});
        active_view_ = 1;
        status_ = "Loading metadata..."; status_time_ = ImGui::GetTime();
    } else {
        // Folder: browse deeper
        path_stack_.push_back(current_path_);
        std::string new_path = current_path_.empty() ? node.id : current_path_ + "/" + node.id;
        fetch_nodes(new_path);
    }
}

void PxWebProvider::navigate_back() {
    if (path_stack_.empty()) return;
    std::string prev = path_stack_.back();
    path_stack_.pop_back();
    fetch_nodes(prev);
    active_view_ = 0;
}

void PxWebProvider::fetch_data() {
    if (selected_table_.empty()) return;
    std::string table_path = current_path_.empty() ? selected_table_ : current_path_ + "/" + selected_table_;
    data_.run_script("pxweb_fetcher.py", {"--action", "data", "--path", table_path});
    active_view_ = 2;
    status_ = "Loading data..."; status_time_ = ImGui::GetTime();
}

void PxWebProvider::pickup_result() {
    if (!data_.has_result() && data_.error().empty()) return;
    std::lock_guard<std::mutex> lock(data_.mutex());
    if (data_.has_result()) {
        const auto& r = data_.result();
        if (active_view_ == 0) parse_nodes(r);
        else if (active_view_ == 1) parse_variables(r);
        else stat_data_ = r;
        status_ = "Data loaded";
    } else if (!data_.error().empty()) {
        status_ = "Error: " + data_.error();
    }
    status_time_ = ImGui::GetTime();
    data_.clear();
}

void PxWebProvider::parse_nodes(const json& j) {
    nodes_.clear();
    json arr = j.is_array() ? j : (j.contains("result") ? j["result"] : json::array());
    for (auto& n : arr) {
        NodeEntry e;
        e.id = n.value("id", n.value("dbid", ""));
        e.text = n.value("text", n.value("title", e.id));
        std::string type_str = n.value("type", "l");
        e.type = (type_str == "t" || type_str == "table") ? 't' : 'l';
        e.updated = n.value("updated", "");
        nodes_.push_back(std::move(e));
    }
}

void PxWebProvider::parse_variables(const json& j) {
    variables_ = j.contains("variables") ? j["variables"]
        : (j.contains("result") && j["result"].is_object() && j["result"].contains("variables")
            ? j["result"]["variables"] : j);
    if (j.contains("title")) table_label_ = j["title"].get<std::string>();
}

// Browse view
void PxWebProvider::render_browse() {
    using namespace theme::colors;
    if (data_.is_loading() && nodes_.empty()) { LoadingState("Loading...", COLOR); return; }
    if (nodes_.empty()) { EmptyState("No items", "Navigate or wait for data"); return; }

    ImGui::TextColored(TEXT_DIM, "%zu items at %s", nodes_.size(), current_path_.empty() ? "/" : current_path_.c_str());
    ImGui::Spacing();

    if (ImGui::BeginTable("##pxw_browse_tbl", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Updated", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (auto& n : nodes_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(n.type == 't' ? theme::colors::SUCCESS : COLOR,
                               "%s", n.type == 't' ? "TABLE" : "FOLDER");
            ImGui::TableNextColumn();
            if (ImGui::Selectable(n.id.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                navigate_into(n);
            }
            ImGui::TableNextColumn();
            ImGui::Text("%s", n.text.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s", format_date(n.updated).c_str());
        }
        ImGui::EndTable();
    }
}

// Metadata view
void PxWebProvider::render_metadata() {
    using namespace theme::colors;

    if (selected_table_.empty()) {
        EmptyState("No table selected", "Browse and click a TABLE item");
        return;
    }

    ImGui::Spacing();
    ImGui::TextColored(COLOR, "TABLE: %s", table_label_.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Variables list
    if (!variables_.is_array() || variables_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No variable metadata available");
    } else {
        ImGui::TextColored(TEXT_DIM, "%zu variables", variables_.size());
        ImGui::Spacing();

        for (auto& v : variables_) {
            std::string code = v.value("code", v.value("id", ""));
            std::string text = v.value("text", v.value("label", code));
            bool elim = v.value("elimination", false);

            ImGui::TextColored(COLOR, "%s", code.c_str());
            ImGui::SameLine(120);
            ImGui::Text("%s", text.c_str());
            if (elim) {
                ImGui::SameLine();
                ImGui::TextColored(TEXT_DIM, "(eliminable)");
            }

            // Show value count
            if (v.contains("values") && v["values"].is_array()) {
                ImGui::SameLine();
                ImGui::TextColored(TEXT_DIM, "[%zu values]", v["values"].size());
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (FetchButton("FETCH DATA", data_.is_loading(), COLOR))
        fetch_data();
}

// Data view
void PxWebProvider::render_data_view() {
    using namespace theme::colors;

    if (stat_data_.empty()) {
        EmptyState("No data loaded", "Select a table and click FETCH DATA in metadata view");
        return;
    }

    if (data_.is_loading()) {
        LoadingState("Loading data...", COLOR);
        return;
    }

    ImGui::Spacing();
    ImGui::TextColored(COLOR, "DATA: %s", table_label_.c_str());
    ImGui::Spacing();

    // json-stat2 or raw: display as generic JSON table
    if (stat_data_.is_object()) {
        // Try to extract columns/data for tabular display
        if (stat_data_.contains("columns") && stat_data_.contains("data")) {
            auto& cols = stat_data_["columns"];
            auto& data = stat_data_["data"];
            int ncol = std::min((int)cols.size(), 10);

            if (ImGui::BeginTable("##pxw_data_tbl", ncol,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                for (int c = 0; c < ncol; c++) {
                    std::string cn = cols[c].is_object() ? cols[c].value("text", cols[c].value("code", "")) : cols[c].dump();
                    ImGui::TableSetupColumn(cn.c_str());
                }
                ImGui::PushStyleColor(ImGuiCol_Text, COLOR);
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor();

                if (data.is_array()) {
                    for (auto& row : data) {
                        ImGui::TableNextRow();
                        if (row.is_object()) {
                            for (int c = 0; c < ncol; c++) {
                                ImGui::TableNextColumn();
                                std::string key = cols[c].is_object() ? cols[c].value("code", std::to_string(c)) : std::to_string(c);
                                auto& v = row.contains(key) ? row[key] : row[std::to_string(c)];
                                std::string vs = v.is_string() ? v.get<std::string>() : v.dump();
                                ImGui::Text("%s", vs.c_str());
                            }
                        } else if (row.is_array()) {
                            for (int c = 0; c < ncol && c < (int)row.size(); c++) {
                                ImGui::TableNextColumn();
                                std::string vs = row[c].is_string() ? row[c].get<std::string>() : row[c].dump();
                                ImGui::Text("%s", vs.c_str());
                            }
                        }
                    }
                }
                ImGui::EndTable();
            }
        } else {
            // Fallback: show key-value pairs
            for (auto& [key, val] : stat_data_.items()) {
                std::string vs = val.is_string() ? val.get<std::string>() : val.dump();
                if (vs.size() > 200) vs = vs.substr(0, 200) + "...";
                InfoRow(key.c_str(), vs.c_str());
            }
        }
    }
}

} // namespace fincept::gov_data
