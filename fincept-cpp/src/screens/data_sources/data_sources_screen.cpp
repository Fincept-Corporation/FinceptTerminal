// Data Sources Screen — 83 connector gallery with connection management

#include "data_sources_screen.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>

namespace fincept::data_sources {

using json = nlohmann::json;

// ============================================================================
// Init
// ============================================================================
void DataSourcesScreen::init() {
    if (initialized_) return;
    all_defs_ = get_all_data_source_defs();
    load_connections();
    initialized_ = true;
    LOG_INFO("DataSources", "Initialized with %zu sources", all_defs_.size());
}

void DataSourcesScreen::load_connections() {
    connections_ = db::ops::get_all_data_sources();
}

// ============================================================================
// Main render
// ============================================================================
void DataSourcesScreen::render() {
    init();
    using namespace theme::colors;

    // Poll async test futures (modal test)
    if (testing_ && test_future_.valid()) {
        auto status = test_future_.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) {
            test_result_ = test_future_.get();
            testing_ = false;
        }
    }
    // Poll per-connection test futures
    if (!testing_conn_id_.empty() && conn_test_future_.valid()) {
        auto status = conn_test_future_.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) {
            conn_test_results_[testing_conn_id_] = conn_test_future_.get();
            testing_conn_id_.clear();
        }
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_bar_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    float x = vp->WorkPos.x;
    float y = vp->WorkPos.y + tab_bar_h;
    float w = vp->WorkSize.x;
    float h = vp->WorkSize.y - tab_bar_h - footer_h;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##data_sources", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking);

    render_header();

    if (view_ == 0) {
        render_filter_bar();
        render_gallery();
    } else {
        render_connections();
    }

    // Status bar
    ImGui::SetCursorPos(ImVec2(0, h - 22));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##ds_status", ImVec2(w, 22), true);
    ImGui::TextColored(TEXT_DIM, "SOURCES: %zu  |  CONNECTIONS: %zu  |  VIEW: %s",
        all_defs_.size(), connections_.size(),
        view_ == 0 ? "GALLERY" : "CONNECTIONS");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (show_modal_) render_config_modal();
}

// ============================================================================
// Header
// ============================================================================
void DataSourcesScreen::render_header() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ds_header", ImVec2(-1, 38), true);

    ImGui::SetCursorPos(ImVec2(16, 8));
    ImGui::TextColored(ACCENT, "DATA SOURCES");
    ImGui::SameLine();

    // Separator
    ImVec2 sep_pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(sep_pos.x + 8, sep_pos.y - 2),
        ImVec2(sep_pos.x + 9, sep_pos.y + 14),
        IM_COL32(50, 50, 55, 255));
    ImGui::Dummy(ImVec2(16, 0));
    ImGui::SameLine();

    ImGui::TextColored(TEXT_DIM, "%zu AVAILABLE | %zu CONNECTED",
        all_defs_.size(), connections_.size());

    // View switcher on the right
    float btn_x = ImGui::GetContentRegionAvail().x - 200;
    ImGui::SameLine(btn_x);

    // Gallery button
    if (view_ == 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_DIM);
        ImGui::PushStyleColor(ImGuiCol_Text, BG_DARK);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    }
    if (ImGui::Button("GALLERY", ImVec2(80, 22))) view_ = 0;
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    // Connections button
    if (view_ == 1) {
        ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_DIM);
        ImGui::PushStyleColor(ImGuiCol_Text, BG_DARK);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    }
    char conn_label[64];
    std::snprintf(conn_label, sizeof(conn_label), "CONNECTIONS (%zu)", connections_.size());
    if (ImGui::Button(conn_label, ImVec2(120, 22))) view_ = 1;
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Orange accent line
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(p,
        ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + 2),
        IM_COL32(255, 136, 0, 255));
    ImGui::Dummy(ImVec2(0, 2));
}

// ============================================================================
// Filter bar (gallery view)
// ============================================================================
void DataSourcesScreen::render_filter_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ds_filter", ImVec2(-1, 38), true);

    ImGui::SetCursorPos(ImVec2(16, 8));

    // Search box
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::SetNextItemWidth(220);
    ImGui::InputTextWithHint("##ds_search", "Search data sources...", search_buf_, sizeof(search_buf_));
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::Dummy(ImVec2(8, 0));
    ImGui::SameLine();

    // Category filter buttons
    static const struct { int val; const char* label; } cats[] = {
        {-1, "ALL"}, {0, "DATABASE"}, {1, "API"}, {2, "FILE"},
        {3, "STREAMING"}, {4, "CLOUD"}, {5, "TIMESERIES"}, {6, "MARKET DATA"},
    };

    for (auto& cat : cats) {
        bool active = (selected_category_ == cat.val);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Border, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
            ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        if (ImGui::Button(cat.label, ImVec2(0, 20))) {
            selected_category_ = cat.val;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Gallery view — grid of source cards
// ============================================================================
void DataSourcesScreen::render_gallery() {
    using namespace theme::colors;

    ImGui::BeginChild("##ds_gallery", ImVec2(-1, -24), false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));

    std::string search_lower;
    for (char c : std::string(search_buf_)) search_lower += (char)std::tolower(c);

    float avail_w = ImGui::GetContentRegionAvail().x - 16;
    float card_w = 270.0f;
    int cols = std::max(1, (int)(avail_w / card_w));
    float actual_card_w = (avail_w - (cols - 1) * 8) / cols;

    int col = 0;
    for (auto& def : all_defs_) {
        // Category filter
        if (selected_category_ >= 0) {
            Category filter_cat = static_cast<Category>(selected_category_);
            if (def.category != filter_cat) continue;
        }

        // Search filter
        if (search_lower.length() > 0) {
            std::string name_lower, desc_lower;
            for (char c : std::string(def.name)) name_lower += (char)std::tolower(c);
            for (char c : std::string(def.description)) desc_lower += (char)std::tolower(c);
            if (name_lower.find(search_lower) == std::string::npos &&
                desc_lower.find(search_lower) == std::string::npos) continue;
        }

        if (col > 0) ImGui::SameLine(0, 8);

        // Card
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
        ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);

        char card_id[64];
        std::snprintf(card_id, sizeof(card_id), "##dsc_%s", def.id);
        ImGui::BeginChild(card_id, ImVec2(actual_card_w, 100), true);

        // Icon + Name
        ImGui::TextColored(ACCENT, "%s", def.icon);
        ImGui::SameLine();
        ImGui::TextColored(TEXT_PRIMARY, "%s", def.name);

        // Category badge
        ImGui::SameLine();
        ImGui::TextColored(INFO, "[%s]", category_label(def.category));

        // Description
        ImGui::TextColored(TEXT_DIM, "%s", def.description);

        // Badges
        if (def.requires_auth) {
            ImGui::TextColored(TEXT_DISABLED, "AUTH");
            ImGui::SameLine();
        }
        if (def.testable) {
            ImGui::TextColored(SUCCESS, "TESTABLE");
            ImGui::SameLine();
        }

        // Configure button
        ImGui::SameLine(actual_card_w - 90);
        char cfg_id[64];
        std::snprintf(cfg_id, sizeof(cfg_id), "Configure##cfg_%s", def.id);
        if (theme::AccentButton(cfg_id, ImVec2(75, 18))) {
            open_config(def);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);

        col++;
        if (col >= cols) col = 0;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

// ============================================================================
// Connections view — list of saved connections
// ============================================================================
void DataSourcesScreen::render_connections() {
    using namespace theme::colors;

    ImGui::BeginChild("##ds_conns", ImVec2(-1, -24), false);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));

    if (connections_.empty()) {
        float avail_h = ImGui::GetContentRegionAvail().y;
        ImGui::SetCursorPosY(avail_h / 2 - 40);
        float text_w = ImGui::CalcTextSize("NO CONNECTIONS CONFIGURED").x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - text_w) / 2);
        ImGui::TextColored(TEXT_DIM, "NO CONNECTIONS CONFIGURED");

        ImGui::Spacing();
        float btn_w = 160;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - btn_w) / 2);
        if (theme::AccentButton("BROWSE DATA SOURCES", ImVec2(btn_w, 24))) {
            view_ = 0;
        }
    } else {
        for (auto& conn : connections_) {
            render_connection_row(conn);
            ImGui::Spacing();
        }
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void DataSourcesScreen::render_connection_row(const db::DataSource& conn) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);

    char row_id[64];
    std::snprintf(row_id, sizeof(row_id), "##conn_%s", conn.id.c_str());
    ImGui::BeginChild(row_id, ImVec2(-1, 50), true);

    // Name
    ImGui::TextColored(TEXT_PRIMARY, "%s", conn.display_name.c_str());
    ImGui::SameLine();

    // Type badge
    ImGui::TextColored(INFO, "[%s]", conn.type.c_str());
    ImGui::SameLine();

    // Provider
    if (!conn.provider.empty()) {
        ImGui::TextColored(TEXT_DIM, "(%s)", conn.provider.c_str());
        ImGui::SameLine();
    }

    // Enabled status
    ImGui::TextColored(conn.enabled ? SUCCESS : TEXT_DISABLED,
        conn.enabled ? "ENABLED" : "DISABLED");

    // Second row: buttons
    // Test button
    bool is_testing_this = (testing_conn_id_ == conn.id);
    if (is_testing_this) {
        ImGui::TextColored(WARNING, "TESTING...");
    } else {
        char test_id[64];
        std::snprintf(test_id, sizeof(test_id), "Test##tst_%s", conn.id.c_str());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.3f, 0.15f, 1.0f));
        if (ImGui::SmallButton(test_id)) {
            // Parse config JSON to map
            std::map<std::string, std::string> cfg;
            try {
                auto j = json::parse(conn.config);
                for (auto& [k, v] : j.items())
                    cfg[k] = v.is_string() ? v.get<std::string>() : v.dump();
            } catch (...) {}
            testing_conn_id_ = conn.id;
            conn_test_future_ = adapters::AdapterRegistry::instance().test_connection_async(conn.type, cfg);
        }
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();

    // Show test result inline if available
    auto tr_it = conn_test_results_.find(conn.id);
    if (tr_it != conn_test_results_.end()) {
        auto& tr = tr_it->second;
        ImGui::TextColored(tr.success ? SUCCESS : ERROR_RED, "%s (%.0fms)",
            tr.success ? "OK" : "FAIL", tr.elapsed_ms);
        ImGui::SameLine();
    }

    // Edit button
    char edit_id[64];
    std::snprintf(edit_id, sizeof(edit_id), "Edit##ed_%s", conn.id.c_str());
    if (ImGui::SmallButton(edit_id)) {
        // Find the def for this type
        for (auto& def : all_defs_) {
            if (std::string(def.type) == conn.type) {
                editing_id_ = conn.id;
                modal_def_ = &def;
                show_modal_ = true;
                form_data_.clear();
                form_data_["name"] = conn.display_name;
                // Parse existing config JSON
                try {
                    auto cfg = json::parse(conn.config);
                    for (auto& [k, v] : cfg.items()) {
                        form_data_[k] = v.is_string() ? v.get<std::string>() : v.dump();
                    }
                } catch (...) {}
                break;
            }
        }
    }
    ImGui::SameLine();

    // Delete button
    ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
    char del_id[64];
    std::snprintf(del_id, sizeof(del_id), "Delete##del_%s", conn.id.c_str());
    if (ImGui::SmallButton(del_id)) {
        delete_connection(conn.id);
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextColored(TEXT_DISABLED, "Created: %s", conn.created_at.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Config modal — dynamic form for configuring a data source
// ============================================================================
void DataSourcesScreen::open_config(const DataSourceDef& def) {
    modal_def_ = &def;
    editing_id_.clear();
    save_error_.clear();
    form_data_.clear();
    form_data_["name"] = std::string("My ") + def.name;
    for (auto& field : def.fields) {
        if (field.default_value && field.default_value[0] != '\0') {
            form_data_[field.name] = field.default_value;
        }
    }
    show_modal_ = true;
}

void DataSourcesScreen::render_config_modal() {
    using namespace theme::colors;
    if (!modal_def_) return;

    // Center modal
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 center = ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                           vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460, 0));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, ACCENT);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    bool open = true;
    char modal_title[128];
    std::snprintf(modal_title, sizeof(modal_title), "%s %s###ds_config_modal",
        editing_id_.empty() ? "CONFIGURE" : "EDIT",
        modal_def_->name);

    if (ImGui::Begin(modal_title, &open,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {

        // Header
        ImGui::TextColored(ACCENT, "%s", modal_def_->icon);
        ImGui::SameLine();
        ImGui::TextColored(TEXT_PRIMARY, "%s", modal_def_->name);
        ImGui::TextColored(TEXT_DIM, "%s", modal_def_->description);
        ImGui::Separator();
        ImGui::Spacing();

        // Error
        if (!save_error_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
            ImGui::TextWrapped("%s", save_error_.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // Connection name field
        ImGui::TextColored(TEXT_DIM, "CONNECTION NAME *");
        auto& name_val = form_data_["name"];
        char name_buf[128];
        std::strncpy(name_buf, name_val.c_str(), sizeof(name_buf) - 1);
        name_buf[sizeof(name_buf) - 1] = '\0';
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##ds_conn_name", name_buf, sizeof(name_buf))) {
            name_val = name_buf;
        }
        ImGui::Spacing();

        // Dynamic fields
        for (auto& field : modal_def_->fields) {
            render_field(field, std::string(modal_def_->id));
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Test connection result display
        if (test_result_.has_value()) {
            ImGui::Spacing();
            auto& r = test_result_.value();
            ImVec4 col = r.success ? SUCCESS : ERROR_RED;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(col.x * 0.15f, col.y * 0.15f, col.z * 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, col);
            ImGui::BeginChild("##test_result", ImVec2(-1, 40), true);
            ImGui::TextColored(col, "%s  %s",
                r.success ? "OK" : "FAIL", r.message.c_str());
            if (r.elapsed_ms > 0)
                ImGui::TextColored(TEXT_DIM, "%.0f ms", r.elapsed_ms);
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::Spacing();
        }

        // Buttons
        // Test Connection button
        if (testing_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.1f, 1.0f));
            ImGui::Button("TESTING...", ImVec2(120, 24));
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.3f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.4f, 0.2f, 1.0f));
            if (ImGui::Button("TEST CONNECTION", ImVec2(120, 24))) {
                // Build config map from form data
                std::map<std::string, std::string> cfg;
                for (auto& [k, v] : form_data_) {
                    if (k != "name") cfg[k] = v;
                }
                test_result_.reset();
                testing_ = true;
                test_future_ = adapters::AdapterRegistry::instance().test_connection_async(
                    std::string(modal_def_->type), cfg);
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::SameLine();

        if (theme::AccentButton(editing_id_.empty() ? "SAVE CONNECTION" : "UPDATE CONNECTION",
            ImVec2(150, 24))) {
            save_connection();
        }
        ImGui::SameLine();
        if (ImGui::Button("CANCEL", ImVec2(80, 24))) {
            show_modal_ = false;
            modal_def_ = nullptr;
            test_result_.reset();
            testing_ = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    if (!open) {
        show_modal_ = false;
        modal_def_ = nullptr;
        test_result_.reset();
        testing_ = false;
    }
}

void DataSourcesScreen::render_field(const FieldDef& field, const std::string& /*ds_id*/) {
    using namespace theme::colors;

    char label[128];
    std::snprintf(label, sizeof(label), "%s%s", field.label, field.required ? " *" : "");
    ImGui::TextColored(TEXT_DIM, "%s", label);

    auto& val = form_data_[field.name];
    if (val.empty() && field.default_value && field.default_value[0] != '\0') {
        val = field.default_value;
    }

    char input_id[128];
    std::snprintf(input_id, sizeof(input_id), "##dsf_%s", field.name);

    ImGui::SetNextItemWidth(-1);

    switch (field.type) {
        case FieldType::Text:
        case FieldType::Url:
        case FieldType::Number: {
            char buf[256];
            std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputTextWithHint(input_id, field.placeholder, buf, sizeof(buf))) {
                val = buf;
            }
            break;
        }
        case FieldType::Password: {
            char buf[256];
            std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText(input_id, buf, sizeof(buf), ImGuiInputTextFlags_Password)) {
                val = buf;
            }
            break;
        }
        case FieldType::Textarea: {
            char buf[1024];
            std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputTextMultiline(input_id, buf, sizeof(buf), ImVec2(-1, 60))) {
                val = buf;
            }
            break;
        }
        case FieldType::Select: {
            if (ImGui::BeginCombo(input_id, val.c_str())) {
                for (auto& opt : field.options) {
                    bool selected = (val == opt.value);
                    if (ImGui::Selectable(opt.label, selected)) {
                        val = opt.value;
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }
        case FieldType::Checkbox: {
            bool checked = (val == "true" || val == "1");
            if (ImGui::Checkbox(input_id, &checked)) {
                val = checked ? "true" : "false";
            }
            break;
        }
    }

    ImGui::Spacing();
}

// ============================================================================
// Save / Delete
// ============================================================================
void DataSourcesScreen::save_connection() {
    if (!modal_def_) return;

    auto name_it = form_data_.find("name");
    if (name_it == form_data_.end() || name_it->second.empty()) {
        save_error_ = "Connection name is required";
        return;
    }

    // Build config JSON from form data (excluding "name")
    json config;
    for (auto& [k, v] : form_data_) {
        if (k == "name") continue;
        config[k] = v;
    }

    db::DataSource ds;
    if (editing_id_.empty()) {
        // Generate ID
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        ds.id = "conn_" + std::to_string(ms);
    } else {
        ds.id = editing_id_;
    }
    ds.alias = std::string(modal_def_->type);
    ds.display_name = name_it->second;
    ds.description = std::string(modal_def_->description);
    ds.type = std::string(modal_def_->type);
    ds.provider = std::string(modal_def_->name);
    ds.category = category_id(modal_def_->category);
    ds.config = config.dump();
    ds.enabled = true;

    db::ops::save_data_source(ds);
    load_connections();

    show_modal_ = false;
    modal_def_ = nullptr;
    editing_id_.clear();
    save_error_.clear();
    view_ = 1;  // Switch to connections view

    LOG_INFO("DataSources", "Saved connection: %s", ds.display_name.c_str());
}

void DataSourcesScreen::delete_connection(const std::string& id) {
    db::ops::delete_data_source(id);
    load_connections();
    LOG_INFO("DataSources", "Deleted connection: %s", id.c_str());
}

} // namespace fincept::data_sources
