// Data Mapping Screen — Bloomberg Terminal-inspired API mapping UI
// Dense, data-focused layout matching the project's visual language

#include "data_mapping_screen.h"
#include "data_mapping_schemas.h"
#include "ui/theme.h"
#include "storage/database.h"
#include "core/logger.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <chrono>

namespace fincept::data_mapping {

using json = nlohmann::json;
using namespace theme::colors;

// Bloomberg-style colors (matching data_sources_screen)
static constexpr ImVec4 BB_BG        = {0.055f, 0.055f, 0.060f, 1.0f};
static constexpr ImVec4 BB_PANEL     = {0.075f, 0.075f, 0.082f, 1.0f};
static constexpr ImVec4 BB_SIDEBAR   = {0.062f, 0.062f, 0.068f, 1.0f};
static constexpr ImVec4 BB_ROW_ALT   = {0.082f, 0.082f, 0.090f, 1.0f};
static constexpr ImVec4 BB_HEADER_BG = {0.090f, 0.090f, 0.098f, 1.0f};
static constexpr ImVec4 BB_CMD_BG    = {0.067f, 0.067f, 0.073f, 1.0f};
static constexpr ImVec4 BB_AMBER     = {1.0f, 0.65f, 0.0f, 1.0f};
static constexpr ImVec4 BB_GREEN     = {0.0f, 0.85f, 0.35f, 1.0f};
static constexpr ImVec4 BB_RED       = {1.0f, 0.25f, 0.20f, 1.0f};
static constexpr ImVec4 BB_CYAN      = {0.0f, 0.75f, 0.85f, 1.0f};
static constexpr ImVec4 BB_WHITE     = {0.92f, 0.92f, 0.93f, 1.0f};
static constexpr ImVec4 BB_GRAY      = {0.50f, 0.50f, 0.54f, 1.0f};
static constexpr ImVec4 BB_DIM       = {0.35f, 0.35f, 0.38f, 1.0f};
static constexpr ImVec4 BB_BORDER    = {0.16f, 0.16f, 0.18f, 1.0f};

// Step definitions
static constexpr CreateStep ALL_STEPS[] = {
    CreateStep::api_config, CreateStep::schema_select,
    CreateStep::field_mapping, CreateStep::cache_settings,
    CreateStep::test_save
};
static constexpr int NUM_STEPS = 5;

// ============================================================================
// Initialization
// ============================================================================

void DataMappingScreen::init() {
    if (initialized_) return;
    mapping_ops::initialize_tables();
    load_mappings();
    load_templates();
    reset_create_form();
    initialized_ = true;
}

// ============================================================================
// Main render
// ============================================================================

void DataMappingScreen::render() {
    init();

    // Check async operations
    if (testing_ && test_future_.valid() &&
        test_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        test_result_ = test_future_.get();
        testing_ = false;
    }
    if (fetching_ && fetch_future_.valid() &&
        fetch_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const auto response = fetch_future_.get();
        fetching_ = false;
        if (response.success) {
            sample_data_json_ = response.data_json;
        }
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float w = vp->WorkSize.x;
    const float h = vp->WorkSize.y;

    // Background
    ImGui::GetWindowDrawList()->AddRectFilled(
        vp->WorkPos, {vp->WorkPos.x + w, vp->WorkPos.y + h},
        ImGui::ColorConvertFloat4ToU32(BB_BG));

    // Layout: command bar (top) | sidebar + content + status | footer
    const float cmd_h = 28.0f;
    const float footer_h = 22.0f;
    const float sidebar_w = sidebar_minimized_ ? 32.0f : 180.0f;
    const float status_w = 240.0f;
    const float content_x = vp->WorkPos.x + sidebar_w;
    const float content_y = vp->WorkPos.y + cmd_h;
    const float content_w = w - sidebar_w - status_w;
    const float content_h = h - cmd_h - footer_h;

    render_command_bar(w);
    render_sidebar(vp->WorkPos.x, content_y, sidebar_w, content_h);
    render_content(content_x, content_y, content_w, content_h);
    render_status_panel(content_x + content_w, content_y, status_w, content_h);
    render_footer(w, footer_h);
}

// ============================================================================
// Command bar
// ============================================================================

void DataMappingScreen::render_command_bar(float w) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize({w, 28.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BB_CMD_BG);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 4});

    ImGui::Begin("##dm_cmdbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking);

    // Title
    ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
    ImGui::Text("DATA MAPPING");
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // View buttons
    ImGui::SameLine(160);
    const bool on_list = (view_ == MappingView::list);
    const bool on_create = (view_ == MappingView::create);
    const bool on_templates = (view_ == MappingView::templates);

    if (on_list) ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
    if (ImGui::SmallButton("LIST")) { view_ = MappingView::list; load_mappings(); }
    if (on_list) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (on_create) ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
    if (ImGui::SmallButton("+ NEW")) { view_ = MappingView::create; reset_create_form(); }
    if (on_create) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (on_templates) ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
    if (ImGui::SmallButton("TEMPLATES")) { view_ = MappingView::templates; }
    if (on_templates) ImGui::PopStyleColor();

    // Search
    ImGui::SameLine(w - 260);
    ImGui::PushItemWidth(200);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BB_PANEL);
    ImGui::InputTextWithHint("##dm_search", "Search mappings...", search_buf_, sizeof(search_buf_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Sidebar — step navigation in create mode, quick actions in list mode
// ============================================================================

void DataMappingScreen::render_sidebar(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({w, h});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BB_SIDEBAR);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {6, 6});

    ImGui::Begin("##dm_sidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking);

    if (sidebar_minimized_) {
        if (ImGui::SmallButton(">")) sidebar_minimized_ = false;
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    if (ImGui::SmallButton("<")) { sidebar_minimized_ = true; }

    ImGui::Separator();

    if (view_ == MappingView::create) {
        // Step navigation
        ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
        ImGui::Text("WIZARD STEPS");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        for (int i = 0; i < NUM_STEPS; ++i) {
            const bool is_current = (ALL_STEPS[i] == current_step_);
            const bool is_past = (i < static_cast<int>(current_step_));

            if (is_current) ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
            else if (is_past) ImGui::PushStyleColor(ImGuiCol_Text, BB_GREEN);
            else ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);

            char label[64];
            std::snprintf(label, sizeof(label), "%d. %s", i + 1, create_step_label(ALL_STEPS[i]));
            if (ImGui::Selectable(label, is_current)) {
                current_step_ = ALL_STEPS[i];
            }
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Navigation buttons
        const int step_idx = static_cast<int>(current_step_);
        if (step_idx > 0) {
            if (ImGui::Button("< Back", {w - 16, 0})) {
                current_step_ = ALL_STEPS[step_idx - 1];
            }
        }
        if (step_idx < NUM_STEPS - 1) {
            if (ImGui::Button("Next >", {w - 16, 0})) {
                current_step_ = ALL_STEPS[step_idx + 1];
            }
        }
    } else {
        // List mode sidebar — stats
        ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
        ImGui::Text("OVERVIEW");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, BB_WHITE);
        ImGui::Text("Mappings: %d", static_cast<int>(saved_mappings_.size()));
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Schema type filter
        ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
        ImGui::Text("SCHEMAS");
        ImGui::PopStyleColor();

        const auto schema_names = get_schema_type_names();
        for (const auto& name : schema_names) {
            int count = 0;
            for (const auto& m : saved_mappings_) {
                if (m.is_predefined_schema && schema_type_id(m.schema_type) == name) {
                    ++count;
                }
            }
            ImGui::PushStyleColor(ImGuiCol_Text, count > 0 ? BB_WHITE : BB_DIM);
            ImGui::Text("  %s (%d)", name.c_str(), count);
            ImGui::PopStyleColor();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Main content area — dispatches to current view
// ============================================================================

void DataMappingScreen::render_content(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({w, h});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BB_BG);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 8});

    ImGui::Begin("##dm_content", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking);

    switch (view_) {
        case MappingView::list:      render_list_view(w, h); break;
        case MappingView::create:    render_create_view(w, h); break;
        case MappingView::templates: render_templates_view(w, h); break;
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// List View — table of saved mappings
// ============================================================================

void DataMappingScreen::render_list_view(float w, float /*h*/) {
    if (saved_mappings_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);
        ImGui::Text("No mappings configured yet. Click '+ NEW' to create one.");
        ImGui::PopStyleColor();
        return;
    }

    const std::string filter(search_buf_);

    // Table header
    if (ImGui::BeginTable("##mappings_table", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY)) {

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None, w * 0.22f);
        ImGui::TableSetupColumn("Schema", ImGuiTableColumnFlags_None, w * 0.12f);
        ImGui::TableSetupColumn("API Endpoint", ImGuiTableColumnFlags_None, w * 0.25f);
        ImGui::TableSetupColumn("Fields", ImGuiTableColumnFlags_None, w * 0.08f);
        ImGui::TableSetupColumn("Updated", ImGuiTableColumnFlags_None, w * 0.15f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_None, w * 0.16f);

        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BB_HEADER_BG);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (size_t i = 0; i < saved_mappings_.size(); ++i) {
            const auto& m = saved_mappings_[i];

            // Filter
            if (!filter.empty()) {
                const std::string name_lower = [&] {
                    std::string s = m.name;
                    std::transform(s.begin(), s.end(), s.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    return s;
                }();
                std::string filter_lower = filter;
                std::transform(filter_lower.begin(), filter_lower.end(),
                    filter_lower.begin(), [](unsigned char c) { return std::tolower(c); });
                if (name_lower.find(filter_lower) == std::string::npos) continue;
            }

            ImGui::TableNextRow();
            if (i % 2 == 1) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    ImGui::ColorConvertFloat4ToU32(BB_ROW_ALT));
            }

            // Name
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_WHITE);
            ImGui::Text("%s", m.name.c_str());
            ImGui::PopStyleColor();

            // Schema
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
            if (m.is_predefined_schema) {
                ImGui::Text("%s", schema_type_id(m.schema_type));
            } else {
                ImGui::Text("Custom");
            }
            ImGui::PopStyleColor();

            // Endpoint
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_GRAY);
            const std::string endpoint = m.api_config.base_url.substr(0,
                std::min<size_t>(30, m.api_config.base_url.size())) + m.api_config.endpoint;
            ImGui::Text("%s", endpoint.c_str());
            ImGui::PopStyleColor();

            // Fields count
            ImGui::TableNextColumn();
            ImGui::Text("%d", static_cast<int>(m.field_mappings.size()));

            // Updated
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);
            ImGui::Text("%s", m.updated_at.substr(0, 10).c_str());
            ImGui::PopStyleColor();

            // Actions
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton("Edit")) {
                edit_mapping(m);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Dup")) {
                mapping_ops::duplicate_mapping(m.id);
                load_mappings();
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_RED);
            if (ImGui::SmallButton("Del")) {
                delete_mapping_by_id(m.id);
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// Create View — 5-step wizard
// ============================================================================

void DataMappingScreen::render_create_view(float w, float h) {
    // Step indicator bar
    ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
    ImGui::Text("Step %d/%d: %s", static_cast<int>(current_step_) + 1, NUM_STEPS,
                create_step_label(current_step_));
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    switch (current_step_) {
        case CreateStep::api_config:     render_step_api_config(w, h); break;
        case CreateStep::schema_select:  render_step_schema_select(w, h); break;
        case CreateStep::field_mapping:  render_step_field_mapping(w, h); break;
        case CreateStep::cache_settings: render_step_cache_settings(w, h); break;
        case CreateStep::test_save:      render_step_test_save(w, h); break;
    }
}

// ============================================================================
// Step 1: API Configuration
// ============================================================================

void DataMappingScreen::render_step_api_config(float w, float /*h*/) {
    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("API CONNECTION");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Name & Description
    ImGui::Text("Name:");
    ImGui::SameLine(80);
    ImGui::PushItemWidth(w * 0.5f);
    ImGui::InputText("##api_name", api_name_buf_, sizeof(api_name_buf_));
    ImGui::PopItemWidth();

    ImGui::Text("Desc:");
    ImGui::SameLine(80);
    ImGui::PushItemWidth(w * 0.5f);
    ImGui::InputText("##api_desc", api_desc_buf_, sizeof(api_desc_buf_));
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // URL & Method
    static const char* methods[] = {"GET", "POST", "PUT", "DELETE", "PATCH"};
    ImGui::Text("Method:");
    ImGui::SameLine(80);
    ImGui::PushItemWidth(80);
    ImGui::Combo("##method", &api_method_idx_, methods, 5);
    ImGui::PopItemWidth();

    ImGui::Text("Base URL:");
    ImGui::SameLine(80);
    ImGui::PushItemWidth(w * 0.5f);
    ImGui::InputText("##base_url", api_base_url_buf_, sizeof(api_base_url_buf_));
    ImGui::PopItemWidth();

    ImGui::Text("Endpoint:");
    ImGui::SameLine(80);
    ImGui::PushItemWidth(w * 0.5f);
    ImGui::InputText("##endpoint", api_endpoint_buf_, sizeof(api_endpoint_buf_));
    ImGui::PopItemWidth();

    ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);
    ImGui::Text("  Use {placeholders} for runtime parameters, e.g. /ticker/{symbol}");
    ImGui::PopStyleColor();

    // Body (for POST/PUT/PATCH)
    if (api_method_idx_ >= 1 && api_method_idx_ <= 3) {
        ImGui::Spacing();
        ImGui::Text("Body (JSON):");
        ImGui::InputTextMultiline("##body", api_body_buf_, sizeof(api_body_buf_),
            {w * 0.6f, 80});
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Authentication
    render_auth_config();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Headers
    render_headers_editor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Query params
    render_query_params_editor();

    // Fetch sample button
    ImGui::Spacing();
    ImGui::Spacing();
    if (fetching_) {
        ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
        ImGui::Text("Fetching...");
        ImGui::PopStyleColor();
    } else {
        if (ImGui::Button("Fetch Sample Data")) {
            fetch_sample_data();
        }
    }
}

// ============================================================================
// Auth config section
// ============================================================================

void DataMappingScreen::render_auth_config() {
    static const char* auth_types[] = {"None", "API Key", "Bearer Token", "Basic Auth", "OAuth2"};

    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("AUTHENTICATION");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::Text("Type:");
    ImGui::SameLine(80);
    ImGui::PushItemWidth(150);
    ImGui::Combo("##auth_type", &api_auth_type_idx_, auth_types, 5);
    ImGui::PopItemWidth();

    switch (api_auth_type_idx_) {
        case 1: { // API Key
            static const char* locations[] = {"Header", "Query Param"};
            ImGui::Text("Location:");
            ImGui::SameLine(80);
            ImGui::PushItemWidth(120);
            ImGui::Combo("##key_loc", &auth_key_location_idx_, locations, 2);
            ImGui::PopItemWidth();

            ImGui::Text("Key Name:");
            ImGui::SameLine(80);
            ImGui::PushItemWidth(200);
            ImGui::InputText("##key_name", auth_key_name_buf_, sizeof(auth_key_name_buf_));
            ImGui::PopItemWidth();

            ImGui::Text("Value:");
            ImGui::SameLine(80);
            ImGui::PushItemWidth(300);
            ImGui::InputText("##key_val", auth_key_value_buf_, sizeof(auth_key_value_buf_),
                ImGuiInputTextFlags_Password);
            ImGui::PopItemWidth();
            break;
        }
        case 2: { // Bearer
            ImGui::Text("Token:");
            ImGui::SameLine(80);
            ImGui::PushItemWidth(400);
            ImGui::InputText("##bearer", auth_bearer_buf_, sizeof(auth_bearer_buf_),
                ImGuiInputTextFlags_Password);
            ImGui::PopItemWidth();
            break;
        }
        case 3: { // Basic
            ImGui::Text("Username:");
            ImGui::SameLine(80);
            ImGui::PushItemWidth(200);
            ImGui::InputText("##basic_user", auth_basic_user_buf_, sizeof(auth_basic_user_buf_));
            ImGui::PopItemWidth();

            ImGui::Text("Password:");
            ImGui::SameLine(80);
            ImGui::PushItemWidth(200);
            ImGui::InputText("##basic_pass", auth_basic_pass_buf_, sizeof(auth_basic_pass_buf_),
                ImGuiInputTextFlags_Password);
            ImGui::PopItemWidth();
            break;
        }
        default: break;
    }
}

// ============================================================================
// Headers editor
// ============================================================================

void DataMappingScreen::render_headers_editor() {
    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("HEADERS");
    ImGui::PopStyleColor();

    // Show existing headers
    auto& hdrs = current_mapping_.api_config.headers;
    std::string to_remove;
    for (const auto& [k, v] : hdrs) {
        ImGui::PushStyleColor(ImGuiCol_Text, BB_WHITE);
        ImGui::Text("  %s:", k.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, BB_GRAY);
        ImGui::Text("%s", v.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushID(k.c_str());
        if (ImGui::SmallButton("x")) to_remove = k;
        ImGui::PopID();
    }
    if (!to_remove.empty()) hdrs.erase(to_remove);

    // Add new header
    ImGui::PushItemWidth(120);
    ImGui::InputTextWithHint("##hdr_key", "Key", header_key_buf_, sizeof(header_key_buf_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    ImGui::InputTextWithHint("##hdr_val", "Value", header_val_buf_, sizeof(header_val_buf_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Add Header")) {
        if (std::strlen(header_key_buf_) > 0) {
            hdrs[header_key_buf_] = header_val_buf_;
            header_key_buf_[0] = '\0';
            header_val_buf_[0] = '\0';
        }
    }
}

// ============================================================================
// Query params editor
// ============================================================================

void DataMappingScreen::render_query_params_editor() {
    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("QUERY PARAMETERS");
    ImGui::PopStyleColor();

    auto& qparams = current_mapping_.api_config.query_params;
    std::string to_remove;
    for (const auto& [k, v] : qparams) {
        ImGui::PushStyleColor(ImGuiCol_Text, BB_WHITE);
        ImGui::Text("  %s:", k.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, BB_GRAY);
        ImGui::Text("%s", v.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushID(("qp_" + k).c_str());
        if (ImGui::SmallButton("x")) to_remove = k;
        ImGui::PopID();
    }
    if (!to_remove.empty()) qparams.erase(to_remove);

    ImGui::PushItemWidth(120);
    ImGui::InputTextWithHint("##qp_key", "Key", qparam_key_buf_, sizeof(qparam_key_buf_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    ImGui::InputTextWithHint("##qp_val", "Value", qparam_val_buf_, sizeof(qparam_val_buf_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Add Param")) {
        if (std::strlen(qparam_key_buf_) > 0) {
            qparams[qparam_key_buf_] = qparam_val_buf_;
            qparam_key_buf_[0] = '\0';
            qparam_val_buf_[0] = '\0';
        }
    }
}

// ============================================================================
// Step 2: Schema Selection
// ============================================================================

void DataMappingScreen::render_step_schema_select(float w, float /*h*/) {
    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("TARGET SCHEMA");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::Checkbox("Use predefined schema", &current_mapping_.is_predefined_schema);
    is_custom_schema_ = !current_mapping_.is_predefined_schema;

    ImGui::Spacing();

    if (current_mapping_.is_predefined_schema) {
        const auto names = get_schema_type_names();
        static const char* schema_items[] = {"OHLCV", "QUOTE", "TICK", "ORDER", "POSITION", "PORTFOLIO", "INSTRUMENT"};
        ImGui::Text("Schema:");
        ImGui::SameLine(80);
        ImGui::PushItemWidth(200);
        if (ImGui::Combo("##schema_type", &schema_type_idx_, schema_items, 7)) {
            current_mapping_.schema_type = static_cast<SchemaType>(schema_type_idx_);
        }
        ImGui::PopItemWidth();

        // Show schema fields
        const auto* schema = get_schema_for_type(current_mapping_.schema_type);
        if (schema) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
            ImGui::Text("Fields in %s:", schema->name.c_str());
            ImGui::PopStyleColor();

            if (ImGui::BeginTable("##schema_fields", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_None, w * 0.25f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, w * 0.12f);
                ImGui::TableSetupColumn("Required", ImGuiTableColumnFlags_None, w * 0.1f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_None, w * 0.4f);
                ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BB_HEADER_BG);
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor();

                for (const auto& f : schema->fields) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::PushStyleColor(ImGuiCol_Text, BB_WHITE);
                    ImGui::Text("%s", f.name.c_str());
                    ImGui::PopStyleColor();

                    ImGui::TableNextColumn();
                    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
                    ImGui::Text("%s", field_type_label(f.type));
                    ImGui::PopStyleColor();

                    ImGui::TableNextColumn();
                    ImGui::PushStyleColor(ImGuiCol_Text, f.required ? BB_AMBER : BB_DIM);
                    ImGui::Text("%s", f.required ? "YES" : "no");
                    ImGui::PopStyleColor();

                    ImGui::TableNextColumn();
                    ImGui::PushStyleColor(ImGuiCol_Text, BB_GRAY);
                    ImGui::Text("%s", f.description.c_str());
                    ImGui::PopStyleColor();
                }
                ImGui::EndTable();
            }
        }
    } else {
        ImGui::Text("Custom schema fields are configured in the Field Mapping step.");
    }

    // Extraction config
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("DATA EXTRACTION");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::Text("Root Path:");
    ImGui::SameLine(100);
    ImGui::PushItemWidth(300);
    ImGui::InputTextWithHint("##root_path", "e.g. data.candles", root_path_buf_, sizeof(root_path_buf_));
    ImGui::PopItemWidth();

    ImGui::Checkbox("Source is array", &current_mapping_.extraction.is_array);
    ImGui::SameLine();
    ImGui::Checkbox("Array of arrays", &current_mapping_.extraction.is_array_of_arrays);
}

// ============================================================================
// Step 3: Field Mapping
// ============================================================================

void DataMappingScreen::render_step_field_mapping(float w, float /*h*/) {
    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("FIELD MAPPINGS");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Auto-populate from schema if empty
    if (current_mapping_.field_mappings.empty() && current_mapping_.is_predefined_schema) {
        const auto* schema = get_schema_for_type(current_mapping_.schema_type);
        if (schema) {
            for (const auto& f : schema->fields) {
                FieldMapping fm;
                fm.target_field = f.name;
                fm.source_path = f.name;
                fm.required = f.required;
                current_mapping_.field_mappings.push_back(fm);
            }
        }
    }

    // Add mapping button
    if (ImGui::Button("+ Add Field Mapping")) {
        current_mapping_.field_mappings.push_back({});
    }

    ImGui::Spacing();

    // Field mapping table
    if (!current_mapping_.field_mappings.empty() &&
        ImGui::BeginTable("##field_map_table", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY)) {

        ImGui::TableSetupColumn("Target Field", ImGuiTableColumnFlags_None, w * 0.18f);
        ImGui::TableSetupColumn("Source Path", ImGuiTableColumnFlags_None, w * 0.25f);
        ImGui::TableSetupColumn("Parser", ImGuiTableColumnFlags_None, w * 0.10f);
        ImGui::TableSetupColumn("Transform", ImGuiTableColumnFlags_None, w * 0.15f);
        ImGui::TableSetupColumn("Req?", ImGuiTableColumnFlags_None, w * 0.06f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_None, w * 0.06f);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BB_HEADER_BG);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        int to_delete = -1;
        for (size_t i = 0; i < current_mapping_.field_mappings.size(); ++i) {
            auto& fm = current_mapping_.field_mappings[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();

            // Target field
            ImGui::TableNextColumn();
            char target_buf[64] = {};
            std::strncpy(target_buf, fm.target_field.c_str(), sizeof(target_buf) - 1);
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##target", target_buf, sizeof(target_buf))) {
                fm.target_field = target_buf;
            }
            ImGui::PopItemWidth();

            // Source path
            ImGui::TableNextColumn();
            char source_buf[128] = {};
            std::strncpy(source_buf, fm.source_path.c_str(), sizeof(source_buf) - 1);
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##source", source_buf, sizeof(source_buf))) {
                fm.source_path = source_buf;
            }
            ImGui::PopItemWidth();

            // Parser
            ImGui::TableNextColumn();
            static const char* parsers[] = {"Direct", "JSONPath", "Regex"};
            int parser_idx = static_cast<int>(fm.parser);
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##parser", &parser_idx, parsers, 3)) {
                fm.parser = static_cast<ParserEngine>(parser_idx);
            }
            ImGui::PopItemWidth();

            // Transform (first one)
            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            char xform_buf[64] = {};
            if (!fm.transforms.empty()) {
                std::strncpy(xform_buf, fm.transforms[0].c_str(), sizeof(xform_buf) - 1);
            }
            if (ImGui::InputText("##xform", xform_buf, sizeof(xform_buf))) {
                if (std::strlen(xform_buf) > 0) {
                    if (fm.transforms.empty()) fm.transforms.push_back(xform_buf);
                    else fm.transforms[0] = xform_buf;
                } else {
                    fm.transforms.clear();
                }
            }
            ImGui::PopItemWidth();

            // Required
            ImGui::TableNextColumn();
            ImGui::Checkbox("##req", &fm.required);

            // Delete
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_RED);
            if (ImGui::SmallButton("X")) to_delete = static_cast<int>(i);
            ImGui::PopStyleColor();

            ImGui::PopID();
        }

        if (to_delete >= 0) {
            current_mapping_.field_mappings.erase(
                current_mapping_.field_mappings.begin() + to_delete);
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// Step 4: Cache Settings
// ============================================================================

void DataMappingScreen::render_step_cache_settings(float /*w*/, float /*h*/) {
    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("CACHE SETTINGS");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::Checkbox("Enable caching", &current_mapping_.api_config.cache_enabled);

    if (current_mapping_.api_config.cache_enabled) {
        ImGui::Text("TTL (seconds):");
        ImGui::SameLine(120);
        ImGui::PushItemWidth(100);
        ImGui::InputInt("##cache_ttl", &current_mapping_.api_config.cache_ttl_seconds);
        ImGui::PopItemWidth();

        if (current_mapping_.api_config.cache_ttl_seconds < 0) {
            current_mapping_.api_config.cache_ttl_seconds = 0;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("VALIDATION");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::Checkbox("Enable validation", &current_mapping_.validation.enabled);
    if (current_mapping_.validation.enabled) {
        ImGui::Checkbox("Strict mode (fail on errors)", &current_mapping_.validation.strict_mode);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Post-processing
    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("POST-PROCESSING");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Limit
    int limit_val = current_mapping_.post_processing.limit.value_or(0);
    ImGui::Text("Limit:");
    ImGui::SameLine(80);
    ImGui::PushItemWidth(100);
    if (ImGui::InputInt("##pp_limit", &limit_val)) {
        current_mapping_.post_processing.limit = (limit_val > 0) ? std::optional<int>(limit_val) : std::nullopt;
    }
    ImGui::PopItemWidth();

    ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);
    ImGui::Text("  0 = no limit");
    ImGui::PopStyleColor();
}

// ============================================================================
// Step 5: Test & Save
// ============================================================================

void DataMappingScreen::render_step_test_save(float w, float h) {
    // Mapping name/description
    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("MAPPING DETAILS");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::Text("Name:");
    ImGui::SameLine(80);
    ImGui::PushItemWidth(w * 0.4f);
    ImGui::InputText("##map_name", mapping_name_buf_, sizeof(mapping_name_buf_));
    ImGui::PopItemWidth();

    ImGui::Text("Desc:");
    ImGui::SameLine(80);
    ImGui::PushItemWidth(w * 0.4f);
    ImGui::InputText("##map_desc", mapping_desc_buf_, sizeof(mapping_desc_buf_));
    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Test section
    ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
    ImGui::Text("TEST MAPPING");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Sample data input
    ImGui::Text("Sample JSON Data:");
    static char sample_buf[8192] = {};
    if (!sample_data_json_.empty() && sample_buf[0] == '\0') {
        std::strncpy(sample_buf, sample_data_json_.c_str(),
            std::min<size_t>(sizeof(sample_buf) - 1, sample_data_json_.size()));
    }
    ImGui::InputTextMultiline("##sample_json", sample_buf, sizeof(sample_buf),
        {w * 0.7f, 120});

    ImGui::Spacing();

    if (testing_) {
        ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
        ImGui::Text("Testing...");
        ImGui::PopStyleColor();
    } else {
        if (ImGui::Button("Run Test")) {
            sample_data_json_ = sample_buf;
            start_test();
        }
    }

    // Test results
    if (test_result_) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (test_result_->success) {
            ImGui::PushStyleColor(ImGuiCol_Text, BB_GREEN);
            ImGui::Text("SUCCESS - %d records in %lldms",
                test_result_->records_returned, static_cast<long long>(test_result_->duration_ms));
            ImGui::PopStyleColor();

            // Show preview
            render_json_preview("Transformed Data", test_result_->data_json, 200);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, BB_RED);
            ImGui::Text("FAILED");
            ImGui::PopStyleColor();
            for (const auto& err : test_result_->errors) {
                ImGui::PushStyleColor(ImGuiCol_Text, BB_RED);
                ImGui::Text("  - %s", err.c_str());
                ImGui::PopStyleColor();
            }
        }

        render_validation_results();
    }

    // Save button
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.0f, 0.5f, 0.2f, 1.0f});
    if (ImGui::Button("Save Mapping", {140, 30})) {
        save_current_mapping();
    }
    ImGui::PopStyleColor();
}

// ============================================================================
// Templates view
// ============================================================================

void DataMappingScreen::render_templates_view(float w, float /*h*/) {
    ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
    ImGui::Text("MAPPING TEMPLATES");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (templates_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);
        ImGui::Text("No templates available.");
        ImGui::PopStyleColor();
        return;
    }

    if (ImGui::BeginTable("##templates_table", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY)) {

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None, w * 0.25f);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_None, w * 0.12f);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_None, w * 0.35f);
        ImGui::TableSetupColumn("Tags", ImGuiTableColumnFlags_None, w * 0.15f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_None, w * 0.1f);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BB_HEADER_BG);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        for (size_t i = 0; i < templates_.size(); ++i) {
            const auto& t = templates_[i];
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_WHITE);
            ImGui::Text("%s", t.name.c_str());
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
            ImGui::Text("%s", t.category.c_str());
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_GRAY);
            ImGui::Text("%s", t.description.c_str());
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);
            std::string tags_str;
            for (const auto& tag : t.tags) {
                if (!tags_str.empty()) tags_str += ", ";
                tags_str += tag;
            }
            ImGui::Text("%s", tags_str.c_str());
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton("Use")) {
                load_template(t);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Status panel (right side)
// ============================================================================

void DataMappingScreen::render_status_panel(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({w, h});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BB_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 8});

    ImGui::Begin("##dm_status", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking);

    ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
    ImGui::Text("STATUS");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    // Current view
    ImGui::PushStyleColor(ImGuiCol_Text, BB_GRAY);
    ImGui::Text("View:");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, BB_WHITE);
    switch (view_) {
        case MappingView::list:      ImGui::Text("List"); break;
        case MappingView::create:    ImGui::Text("Create/Edit"); break;
        case MappingView::templates: ImGui::Text("Templates"); break;
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Mapping count
    ImGui::PushStyleColor(ImGuiCol_Text, BB_GRAY);
    ImGui::Text("Saved:");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, BB_GREEN);
    ImGui::Text("%d", static_cast<int>(saved_mappings_.size()));
    ImGui::PopStyleColor();

    // If in create mode, show current config summary
    if (view_ == MappingView::create) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, BB_CYAN);
        ImGui::Text("CURRENT CONFIG");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        if (api_base_url_buf_[0] != '\0') {
            ImGui::PushStyleColor(ImGuiCol_Text, BB_GRAY);
            ImGui::Text("URL:");
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);
            ImGui::TextWrapped("%s%s", api_base_url_buf_, api_endpoint_buf_);
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, BB_GRAY);
        ImGui::Text("Fields: %d", static_cast<int>(current_mapping_.field_mappings.size()));
        ImGui::PopStyleColor();

        // Sample data status
        ImGui::Spacing();
        if (!sample_data_json_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, BB_GREEN);
            ImGui::Text("Sample data loaded");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);
            ImGui::Text("No sample data");
            ImGui::PopStyleColor();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Footer
// ============================================================================

void DataMappingScreen::render_footer(float w, float footer_h) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - footer_h});
    ImGui::SetNextWindowSize({w, footer_h});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BB_CMD_BG);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 2});

    ImGui::Begin("##dm_footer", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking);

    ImGui::PushStyleColor(ImGuiCol_Text, BB_DIM);
    ImGui::Text("DATA MAPPING  |  %d mappings  |  %d templates",
        static_cast<int>(saved_mappings_.size()),
        static_cast<int>(templates_.size()));
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// JSON Preview helper
// ============================================================================

void DataMappingScreen::render_json_preview(const std::string& label,
                                             const std::string& json_str,
                                             float height) {
    ImGui::PushStyleColor(ImGuiCol_Text, BB_AMBER);
    ImGui::Text("%s:", label.c_str());
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_FrameBg, BB_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Text, BB_WHITE);
    // Truncate for display
    const std::string display = (json_str.size() > 4000)
        ? json_str.substr(0, 4000) + "\n... (truncated)"
        : json_str;
    char preview_buf[4096] = {};
    std::strncpy(preview_buf, display.c_str(),
        std::min<size_t>(sizeof(preview_buf) - 1, display.size()));
    ImGui::InputTextMultiline("##json_preview", preview_buf, sizeof(preview_buf),
        {-1, height}, ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Validation results display
// ============================================================================

void DataMappingScreen::render_validation_results() {
    if (!test_result_ || !test_result_->validation.valid) return;

    if (test_result_->validation.errors.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, BB_GREEN);
        ImGui::Text("Validation: PASSED");
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, BB_RED);
        ImGui::Text("Validation: %d errors", static_cast<int>(test_result_->validation.errors.size()));
        ImGui::PopStyleColor();
        for (const auto& e : test_result_->validation.errors) {
            ImGui::PushStyleColor(ImGuiCol_Text, BB_RED);
            ImGui::Text("  [%s] %s: %s", e.rule.c_str(), e.field.c_str(), e.message.c_str());
            ImGui::PopStyleColor();
        }
    }
}

// ============================================================================
// Actions
// ============================================================================

void DataMappingScreen::load_mappings() {
    saved_mappings_ = mapping_ops::get_all_mappings();
}

void DataMappingScreen::save_current_mapping() {
    // Sync form buffers to current_mapping_
    current_mapping_.name = mapping_name_buf_;
    current_mapping_.description = mapping_desc_buf_;
    current_mapping_.api_config.name = api_name_buf_;
    current_mapping_.api_config.description = api_desc_buf_;
    current_mapping_.api_config.base_url = api_base_url_buf_;
    current_mapping_.api_config.endpoint = api_endpoint_buf_;
    current_mapping_.api_config.method = static_cast<HttpMethod>(api_method_idx_);
    current_mapping_.extraction.root_path = root_path_buf_;

    if (std::strlen(api_body_buf_) > 0) {
        current_mapping_.api_config.body = api_body_buf_;
    }

    // Auth
    switch (api_auth_type_idx_) {
        case 0: current_mapping_.api_config.authentication.type = AuthType::none; break;
        case 1: {
            current_mapping_.api_config.authentication.type = AuthType::apikey;
            current_mapping_.api_config.authentication.apikey = ApiKeyConfig{
                auth_key_location_idx_ == 1 ? ApiKeyLocation::query : ApiKeyLocation::header,
                auth_key_name_buf_, auth_key_value_buf_
            };
            break;
        }
        case 2: {
            current_mapping_.api_config.authentication.type = AuthType::bearer;
            current_mapping_.api_config.authentication.bearer = BearerConfig{auth_bearer_buf_};
            break;
        }
        case 3: {
            current_mapping_.api_config.authentication.type = AuthType::basic;
            current_mapping_.api_config.authentication.basic = BasicAuthConfig{
                auth_basic_user_buf_, auth_basic_pass_buf_};
            break;
        }
        default: break;
    }

    // Generate timestamps
    if (current_mapping_.id.empty()) {
        current_mapping_.id = db::generate_uuid();
    }
    if (current_mapping_.api_config.id.empty()) {
        current_mapping_.api_config.id = db::generate_uuid();
    }

    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &tt);
#else
    gmtime_r(&tt, &tm_buf);
#endif
    char time_str[64];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);

    if (current_mapping_.created_at.empty()) {
        current_mapping_.created_at = time_str;
    }
    current_mapping_.updated_at = time_str;

    mapping_ops::save_mapping(current_mapping_);
    load_mappings();
    view_ = MappingView::list;
}

void DataMappingScreen::delete_mapping_by_id(const std::string& id) {
    mapping_ops::delete_mapping(id);
    load_mappings();
}

void DataMappingScreen::start_test() {
    // Sync form to mapping
    current_mapping_.extraction.root_path = root_path_buf_;

    json sample_json;
    if (!sample_data_json_.empty()) {
        try {
            sample_json = json::parse(sample_data_json_);
        } catch (...) {
            test_result_ = ExecutionResult{};
            test_result_->errors.push_back("Invalid JSON in sample data");
            return;
        }
    }

    testing_ = true;
    test_future_ = std::async(std::launch::async, [this, sample_json]() {
        return engine_.test(current_mapping_, sample_json);
    });
}

void DataMappingScreen::fetch_sample_data() {
    // Sync buffers
    current_mapping_.api_config.base_url = api_base_url_buf_;
    current_mapping_.api_config.endpoint = api_endpoint_buf_;
    current_mapping_.api_config.method = static_cast<HttpMethod>(api_method_idx_);

    fetching_ = true;
    fetch_future_ = std::async(std::launch::async, [this]() {
        return engine_.fetch_api(current_mapping_.api_config);
    });
}

void DataMappingScreen::reset_create_form() {
    current_mapping_ = MappingConfig{};
    current_mapping_.id = db::generate_uuid();
    current_mapping_.api_config.id = db::generate_uuid();
    current_step_ = CreateStep::api_config;
    test_result_ = std::nullopt;
    sample_data_json_.clear();

    std::memset(api_name_buf_, 0, sizeof(api_name_buf_));
    std::memset(api_desc_buf_, 0, sizeof(api_desc_buf_));
    std::memset(api_base_url_buf_, 0, sizeof(api_base_url_buf_));
    std::memset(api_endpoint_buf_, 0, sizeof(api_endpoint_buf_));
    std::memset(api_body_buf_, 0, sizeof(api_body_buf_));
    std::memset(mapping_name_buf_, 0, sizeof(mapping_name_buf_));
    std::memset(mapping_desc_buf_, 0, sizeof(mapping_desc_buf_));
    std::memset(root_path_buf_, 0, sizeof(root_path_buf_));
    std::memset(auth_key_name_buf_, 0, sizeof(auth_key_name_buf_));
    std::memset(auth_key_value_buf_, 0, sizeof(auth_key_value_buf_));
    std::memset(auth_bearer_buf_, 0, sizeof(auth_bearer_buf_));
    std::memset(auth_basic_user_buf_, 0, sizeof(auth_basic_user_buf_));
    std::memset(auth_basic_pass_buf_, 0, sizeof(auth_basic_pass_buf_));

    api_method_idx_ = 0;
    api_auth_type_idx_ = 0;
    schema_type_idx_ = 0;
    parser_engine_idx_ = 0;
}

void DataMappingScreen::edit_mapping(const MappingConfig& config) {
    current_mapping_ = config;
    view_ = MappingView::create;
    current_step_ = CreateStep::api_config;

    std::strncpy(api_name_buf_, config.api_config.name.c_str(), sizeof(api_name_buf_) - 1);
    std::strncpy(api_desc_buf_, config.api_config.description.c_str(), sizeof(api_desc_buf_) - 1);
    std::strncpy(api_base_url_buf_, config.api_config.base_url.c_str(), sizeof(api_base_url_buf_) - 1);
    std::strncpy(api_endpoint_buf_, config.api_config.endpoint.c_str(), sizeof(api_endpoint_buf_) - 1);
    std::strncpy(mapping_name_buf_, config.name.c_str(), sizeof(mapping_name_buf_) - 1);
    std::strncpy(mapping_desc_buf_, config.description.c_str(), sizeof(mapping_desc_buf_) - 1);
    std::strncpy(root_path_buf_, config.extraction.root_path.c_str(), sizeof(root_path_buf_) - 1);

    api_method_idx_ = static_cast<int>(config.api_config.method);
    api_auth_type_idx_ = static_cast<int>(config.api_config.authentication.type);
    schema_type_idx_ = static_cast<int>(config.schema_type);

    if (config.api_config.body) {
        std::strncpy(api_body_buf_, config.api_config.body->c_str(), sizeof(api_body_buf_) - 1);
    }

    // Auth details
    if (config.api_config.authentication.apikey) {
        const auto& ak = *config.api_config.authentication.apikey;
        std::strncpy(auth_key_name_buf_, ak.key_name.c_str(), sizeof(auth_key_name_buf_) - 1);
        std::strncpy(auth_key_value_buf_, ak.value.c_str(), sizeof(auth_key_value_buf_) - 1);
        auth_key_location_idx_ = (ak.location == ApiKeyLocation::query) ? 1 : 0;
    }
    if (config.api_config.authentication.bearer) {
        std::strncpy(auth_bearer_buf_, config.api_config.authentication.bearer->token.c_str(),
            sizeof(auth_bearer_buf_) - 1);
    }
    if (config.api_config.authentication.basic) {
        std::strncpy(auth_basic_user_buf_, config.api_config.authentication.basic->username.c_str(),
            sizeof(auth_basic_user_buf_) - 1);
        std::strncpy(auth_basic_pass_buf_, config.api_config.authentication.basic->password.c_str(),
            sizeof(auth_basic_pass_buf_) - 1);
    }
}

void DataMappingScreen::load_template(const MappingTemplate& tmpl) {
    reset_create_form();
    current_mapping_ = tmpl.config;
    current_mapping_.id = db::generate_uuid();
    current_mapping_.api_config.id = db::generate_uuid();

    // Populate form buffers
    std::strncpy(mapping_name_buf_, tmpl.config.name.c_str(), sizeof(mapping_name_buf_) - 1);
    std::strncpy(mapping_desc_buf_, tmpl.config.description.c_str(), sizeof(mapping_desc_buf_) - 1);
    std::strncpy(api_name_buf_, tmpl.config.api_config.name.c_str(), sizeof(api_name_buf_) - 1);
    std::strncpy(api_base_url_buf_, tmpl.config.api_config.base_url.c_str(), sizeof(api_base_url_buf_) - 1);
    std::strncpy(api_endpoint_buf_, tmpl.config.api_config.endpoint.c_str(), sizeof(api_endpoint_buf_) - 1);
    std::strncpy(root_path_buf_, tmpl.config.extraction.root_path.c_str(), sizeof(root_path_buf_) - 1);

    api_method_idx_ = static_cast<int>(tmpl.config.api_config.method);
    schema_type_idx_ = static_cast<int>(tmpl.config.schema_type);

    view_ = MappingView::create;
    current_step_ = CreateStep::api_config;
}

// ============================================================================
// Template loader — built-in Indian broker templates
// ============================================================================

void DataMappingScreen::load_templates() {
    templates_.clear();

    // Upstox Historical OHLCV
    {
        MappingTemplate t;
        t.id = "upstox_historical_ohlcv";
        t.name = "Upstox Historical Candles -> OHLCV";
        t.description = "Map Upstox historical candle data to standard OHLCV format";
        t.category = "broker";
        t.tags = {"upstox", "historical", "ohlcv", "india"};
        t.verified = true;
        t.official = true;

        t.config.name = "Upstox Historical -> OHLCV";
        t.config.description = "Transform Upstox historical candle data";
        t.config.is_predefined_schema = true;
        t.config.schema_type = SchemaType::ohlcv;
        t.config.extraction.engine = ParserEngine::direct;
        t.config.extraction.root_path = "data.candles";
        t.config.extraction.is_array = true;
        t.config.extraction.is_array_of_arrays = true;

        t.config.field_mappings = {
            {"timestamp", FieldSourceType::path, "$[0]", "", ParserEngine::direct, {"toISODate"}, {}, true, 0.0f},
            {"open",      FieldSourceType::path, "$[1]", "", ParserEngine::direct, {}, {}, true, 0.0f},
            {"high",      FieldSourceType::path, "$[2]", "", ParserEngine::direct, {}, {}, true, 0.0f},
            {"low",       FieldSourceType::path, "$[3]", "", ParserEngine::direct, {}, {}, true, 0.0f},
            {"close",     FieldSourceType::path, "$[4]", "", ParserEngine::direct, {}, {}, true, 0.0f},
            {"volume",    FieldSourceType::path, "$[5]", "", ParserEngine::direct, {}, {}, true, 0.0f},
        };
        t.config.validation.enabled = true;
        t.instructions = "Use endpoint: /historical-candle/{instrument_key}/{interval}/{to_date}/{from_date}";
        templates_.push_back(t);
    }

    // Fyers Quotes
    {
        MappingTemplate t;
        t.id = "fyers_quotes";
        t.name = "Fyers Quotes -> QUOTE";
        t.description = "Map Fyers quote data to standard QUOTE format";
        t.category = "broker";
        t.tags = {"fyers", "quote", "india"};
        t.verified = true;
        t.official = true;

        t.config.name = "Fyers Quote -> QUOTE";
        t.config.is_predefined_schema = true;
        t.config.schema_type = SchemaType::quote;
        t.config.extraction.root_path = "d";
        t.config.extraction.is_array = true;

        t.config.field_mappings = {
            {"symbol",        FieldSourceType::path, "n",                  "", ParserEngine::direct, {},            {}, true, 0.0f},
            {"timestamp",     FieldSourceType::path, "v.tt",              "", ParserEngine::direct, {"toISODate"}, {}, true, 0.0f},
            {"price",         FieldSourceType::path, "v.lp",              "", ParserEngine::direct, {},            {}, true, 0.0f},
            {"bid",           FieldSourceType::path, "v.bid",             "", ParserEngine::direct, {},            {}, false, 0.0f},
            {"ask",           FieldSourceType::path, "v.ask",             "", ParserEngine::direct, {},            {}, false, 0.0f},
            {"volume",        FieldSourceType::path, "v.volume",          "", ParserEngine::direct, {},            {}, false, 0.0f},
            {"open",          FieldSourceType::path, "v.open_price",      "", ParserEngine::direct, {},            {}, false, 0.0f},
            {"high",          FieldSourceType::path, "v.high_price",      "", ParserEngine::direct, {},            {}, false, 0.0f},
            {"low",           FieldSourceType::path, "v.low_price",       "", ParserEngine::direct, {},            {}, false, 0.0f},
            {"previousClose", FieldSourceType::path, "v.prev_close_price", "", ParserEngine::direct, {},           {}, false, 0.0f},
            {"change",        FieldSourceType::path, "v.ch",              "", ParserEngine::direct, {},            {}, false, 0.0f},
            {"changePercent", FieldSourceType::path, "v.chp",             "", ParserEngine::direct, {},            {}, false, 0.0f},
        };
        t.config.validation.enabled = true;
        t.instructions = "Use endpoint: /data/quotes with POST method and symbols in body";
        templates_.push_back(t);
    }

    // Fyers Positions
    {
        MappingTemplate t;
        t.id = "fyers_positions";
        t.name = "Fyers Positions -> POSITION";
        t.description = "Map Fyers position data to standard POSITION format";
        t.category = "broker";
        t.tags = {"fyers", "position", "india"};
        t.verified = true;
        t.official = true;

        t.config.name = "Fyers Position -> POSITION";
        t.config.is_predefined_schema = true;
        t.config.schema_type = SchemaType::position;
        t.config.extraction.root_path = "netPositions";
        t.config.extraction.is_array = true;

        t.config.field_mappings = {
            {"symbol",       FieldSourceType::path, "symbol",      "", ParserEngine::direct, {},            {}, true, 0.0f},
            {"quantity",     FieldSourceType::path, "netQty",      "", ParserEngine::direct, {},            {}, true, 0.0f},
            {"averagePrice", FieldSourceType::path, "avgPrice",    "", ParserEngine::direct, {},            {}, true, 0.0f},
            {"currentPrice", FieldSourceType::path, "ltp",         "", ParserEngine::direct, {},            {}, true, 0.0f},
            {"pnl",          FieldSourceType::path, "pl",          "", ParserEngine::direct, {},            {}, true, 0.0f},
            {"pnlPercent",   FieldSourceType::path, "plPercent",   "", ParserEngine::direct, {},            {}, true, 0.0f},
            {"productType",  FieldSourceType::path, "productType", "", ParserEngine::direct, {"uppercase"}, {}, false, 0.0f},
        };
        t.config.validation.enabled = true;
        templates_.push_back(t);
    }

    // Dhan Positions
    {
        MappingTemplate t;
        t.id = "dhan_positions";
        t.name = "Dhan Positions -> POSITION";
        t.description = "Map Dhan position data to standard POSITION format";
        t.category = "broker";
        t.tags = {"dhan", "position", "india"};
        t.verified = true;
        t.official = true;

        t.config.name = "Dhan Position -> POSITION";
        t.config.is_predefined_schema = true;
        t.config.schema_type = SchemaType::position;
        t.config.extraction.root_path = "data";
        t.config.extraction.is_array = true;

        t.config.field_mappings = {
            {"symbol",       FieldSourceType::path, "tradingSymbol",   "", ParserEngine::direct, {},  {}, true, 0.0f},
            {"quantity",     FieldSourceType::path, "netQty",          "", ParserEngine::direct, {},  {}, true, 0.0f},
            {"averagePrice", FieldSourceType::path, "costPrice",       "", ParserEngine::direct, {},  {}, true, 0.0f},
            {"currentPrice", FieldSourceType::path, "ltp",             "", ParserEngine::direct, {},  {}, true, 0.0f},
            {"pnl",          FieldSourceType::path, "realizedProfit",  "", ParserEngine::direct, {},  {}, true, 0.0f},
            {"exchange",     FieldSourceType::path, "exchangeSegment", "", ParserEngine::direct, {},  {}, false, 0.0f},
        };
        t.config.validation.enabled = true;
        templates_.push_back(t);
    }

    // Dhan Orders
    {
        MappingTemplate t;
        t.id = "dhan_orders";
        t.name = "Dhan Orders -> ORDER";
        t.description = "Map Dhan order data to standard ORDER format";
        t.category = "broker";
        t.tags = {"dhan", "order", "india"};
        t.verified = true;
        t.official = true;

        t.config.name = "Dhan Order -> ORDER";
        t.config.is_predefined_schema = true;
        t.config.schema_type = SchemaType::order;
        t.config.extraction.root_path = "data";
        t.config.extraction.is_array = true;

        t.config.field_mappings = {
            {"orderId",        FieldSourceType::path, "orderId",          "", ParserEngine::direct, {},            {}, true, 0.0f},
            {"symbol",         FieldSourceType::path, "tradingSymbol",    "", ParserEngine::direct, {},            {}, true, 0.0f},
            {"side",           FieldSourceType::path, "transactionType",  "", ParserEngine::direct, {"uppercase"}, {}, true, 0.0f},
            {"type",           FieldSourceType::path, "orderType",        "", ParserEngine::direct, {"uppercase"}, {}, true, 0.0f},
            {"quantity",       FieldSourceType::path, "quantity",         "", ParserEngine::direct, {},            {}, true, 0.0f},
            {"filledQuantity", FieldSourceType::path, "tradedQuantity",   "", ParserEngine::direct, {},            {}, false, 0.0f},
            {"price",          FieldSourceType::path, "price",            "", ParserEngine::direct, {},            {}, false, 0.0f},
            {"averagePrice",   FieldSourceType::path, "tradedPrice",      "", ParserEngine::direct, {},            {}, false, 0.0f},
            {"status",         FieldSourceType::path, "orderStatus",      "", ParserEngine::direct, {"uppercase"}, {}, true, 0.0f},
            {"timestamp",      FieldSourceType::path, "createTime",       "", ParserEngine::direct, {"toISODate"}, {}, true, 0.0f},
            {"exchange",       FieldSourceType::path, "exchangeSegment",  "", ParserEngine::direct, {},            {}, false, 0.0f},
        };
        t.config.validation.enabled = true;
        templates_.push_back(t);
    }

    // Zerodha OHLC
    {
        MappingTemplate t;
        t.id = "zerodha_ohlc";
        t.name = "Zerodha OHLC -> OHLCV";
        t.description = "Map Zerodha/Kite OHLC data to standard OHLCV format";
        t.category = "broker";
        t.tags = {"zerodha", "kite", "ohlc", "india"};
        t.verified = true;

        t.config.name = "Zerodha OHLC -> OHLCV";
        t.config.is_predefined_schema = true;
        t.config.schema_type = SchemaType::ohlcv;
        t.config.extraction.root_path = "data";
        t.config.extraction.is_array = false;

        t.config.field_mappings = {
            {"open",   FieldSourceType::path, "ohlc.open",  "", ParserEngine::direct, {}, {}, true, 0.0f},
            {"high",   FieldSourceType::path, "ohlc.high",  "", ParserEngine::direct, {}, {}, true, 0.0f},
            {"low",    FieldSourceType::path, "ohlc.low",   "", ParserEngine::direct, {}, {}, true, 0.0f},
            {"close",  FieldSourceType::path, "last_price",  "", ParserEngine::direct, {}, {}, true, 0.0f},
            {"volume", FieldSourceType::path, "volume",      "", ParserEngine::direct, {}, {}, true, 0.0f},
        };
        t.config.validation.enabled = true;
        t.instructions = "Use endpoint: /quote/ohlc with query params i=NSE:SBIN&i=NSE:RELIANCE";
        templates_.push_back(t);
    }
}

} // namespace fincept::data_mapping
