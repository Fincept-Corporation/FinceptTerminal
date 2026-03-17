// Data Mapping Screen — Fincept-themed API mapping configuration UI
// 2-panel layout with horizontal stepper, card-based sections, rich visual hierarchy
// Uses theme::colors exclusively — no custom BB_* palette

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
#include <cmath>

namespace fincept::data_mapping {

using json = nlohmann::json;
using namespace theme::colors;

// Step definitions
static constexpr CreateStep ALL_STEPS[] = {
    CreateStep::api_config, CreateStep::schema_select,
    CreateStep::field_mapping, CreateStep::cache_settings,
    CreateStep::test_save
};
static constexpr int NUM_STEPS = 5;

// Method colors for badges
static ImVec4 method_color(HttpMethod m) {
    switch (m) {
        case HttpMethod::get:   return SUCCESS;
        case HttpMethod::post:  return INFO;
        case HttpMethod::put:   return WARNING;
        case HttpMethod::del:   return ERROR_RED;
        case HttpMethod::patch: return {0.70f, 0.50f, 0.90f, 1.0f};
    }
    return TEXT_DIM;
}

// Schema colors for badges
static ImVec4 schema_color(SchemaType t) {
    switch (t) {
        case SchemaType::ohlcv:      return {0.30f, 0.70f, 1.00f, 1.0f};
        case SchemaType::quote:      return {0.40f, 0.85f, 0.55f, 1.0f};
        case SchemaType::tick:       return {0.90f, 0.60f, 0.20f, 1.0f};
        case SchemaType::order:      return {0.80f, 0.40f, 0.80f, 1.0f};
        case SchemaType::position:   return {0.50f, 0.80f, 0.90f, 1.0f};
        case SchemaType::portfolio:  return {0.90f, 0.75f, 0.30f, 1.0f};
        case SchemaType::instrument: return {0.65f, 0.65f, 0.80f, 1.0f};
    }
    return TEXT_DIM;
}

// ============================================================================
// UI Helpers — reusable card/badge/label components
// ============================================================================

bool DataMappingScreen::card_begin(const char* id, const char* title, float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10, 8});

    const float card_w = (w > 0) ? w : ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild(id, {card_w, 0}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    // Card title with accent underline
    if (title && title[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        ImGui::Text("%s", title);
        ImGui::PopStyleColor();

        // Orange accent line under title
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(
            {p.x, p.y}, {p.x + card_w - 20, p.y},
            ImGui::ColorConvertFloat4ToU32({ACCENT.x, ACCENT.y, ACCENT.z, 0.4f}), 1.0f);
        ImGui::Spacing();
    }
    return true;
}

void DataMappingScreen::card_end() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::Spacing();
}

void DataMappingScreen::method_badge(HttpMethod method) {
    const ImVec4 col = method_color(method);
    const char* label = http_method_label(method);

    ImGui::PushStyleColor(ImGuiCol_Button, {col.x, col.y, col.z, 0.20f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {col.x, col.y, col.z, 0.30f});
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {6, 2});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::SmallButton(label);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}

void DataMappingScreen::schema_badge(SchemaType type) {
    const ImVec4 col = schema_color(type);
    const char* label = schema_type_id(type);

    ImGui::PushStyleColor(ImGuiCol_Button, {col.x, col.y, col.z, 0.18f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {col.x, col.y, col.z, 0.28f});
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8, 2});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::SmallButton(label);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}

void DataMappingScreen::tag_pill(const char* text, const ImVec4& color) {
    ImGui::PushStyleColor(ImGuiCol_Button, {color.x, color.y, color.z, 0.12f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {color.x, color.y, color.z, 0.20f});
    ImGui::PushStyleColor(ImGuiCol_Text, {color.x, color.y, color.z, 0.85f});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {5, 1});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::SmallButton(text);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}

void DataMappingScreen::inline_label(const char* label, float label_w) {
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
    ImGui::Text("%s", label);
    ImGui::PopStyleColor();
    ImGui::SameLine(label_w);
}

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
// Main render — 2-panel layout
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
    // Account for app chrome (menu bar + tab bar at top, app footer at bottom)
    const float chrome_top = ImGui::GetFrameHeight() + 4.0f;
    const float chrome_bot = ImGui::GetFrameHeight();
    const float ox = vp->WorkPos.x;
    const float oy = vp->WorkPos.y + chrome_top;
    const float w = vp->WorkSize.x;
    const float h = vp->WorkSize.y - chrome_top - chrome_bot;

    // Background
    ImGui::GetWindowDrawList()->AddRectFilled(
        {ox, oy}, {ox + w, oy + h},
        ImGui::ColorConvertFloat4ToU32(BG_DARKEST));

    // Layout: command bar (top) | sidebar + content | footer
    const float cmd_h = 32.0f;
    const float footer_h = 24.0f;
    const float sidebar_w = sidebar_minimized_ ? 36.0f : 220.0f;
    const float content_x = ox + sidebar_w;
    const float content_y = oy + cmd_h;
    const float content_w = w - sidebar_w;
    const float content_h = h - cmd_h - footer_h;

    render_command_bar(w);
    render_sidebar(ox, content_y, sidebar_w, content_h);
    render_content(content_x, content_y, content_w, content_h);
    render_footer(w, footer_h);
}

// ============================================================================
// Command bar — top navigation with view tabs and search
// ============================================================================

void DataMappingScreen::render_command_bar(float w) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float chrome_top = ImGui::GetFrameHeight() + 4.0f;
    ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + chrome_top});
    ImGui::SetNextWindowSize({w, 32.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12, 6});

    ImGui::Begin("##dm_cmdbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking);

    // Title
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    ImGui::Text("DATA MAPPING");
    ImGui::PopStyleColor();

    // View tabs — styled like proper tab buttons
    ImGui::SameLine(170);

    auto tab_button = [&](const char* label, MappingView target_view) {
        const bool active = (view_ == target_view);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10, 3});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        if (ImGui::Button(label)) {
            view_ = target_view;
            if (target_view == MappingView::list) load_mappings();
            if (target_view == MappingView::create) reset_create_form();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
    };

    tab_button("Mappings", MappingView::list);
    tab_button("+ New", MappingView::create);
    tab_button("Templates", MappingView::templates);

    // Search bar (right-aligned)
    const float search_w = 220.0f;
    ImGui::SameLine(w - search_w - 16);
    ImGui::PushItemWidth(search_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::InputTextWithHint("##dm_search", "Search mappings...", search_buf_, sizeof(search_buf_));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Sidebar — context-sensitive: stats/filter in list, config summary in create
// ============================================================================

void DataMappingScreen::render_sidebar(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({w, h});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 8});

    ImGui::Begin("##dm_sidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking);

    // Collapse toggle
    if (sidebar_minimized_) {
        if (ImGui::Button(">", {20, 20})) sidebar_minimized_ = false;
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    if (ImGui::Button("<", {20, 20})) sidebar_minimized_ = true;
    ImGui::PopStyleColor();

    ImGui::Spacing();

    if (view_ == MappingView::create) {
        // ---- Create mode: config summary ----
        theme::SectionHeader("CONFIG SUMMARY");
        ImGui::Spacing();

        // Live URL preview
        if (api_base_url_buf_[0] != '\0') {
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
            ImGui::Text("URL");
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, INFO);
            ImGui::TextWrapped("%s%s", api_base_url_buf_, api_endpoint_buf_);
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // Method badge
        if (api_base_url_buf_[0] != '\0') {
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
            ImGui::Text("Method");
            ImGui::PopStyleColor();
            ImGui::SameLine(70);
            method_badge(static_cast<HttpMethod>(api_method_idx_));
            ImGui::Spacing();
        }

        // Schema badge
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("Schema");
        ImGui::PopStyleColor();
        ImGui::SameLine(70);
        if (current_mapping_.is_predefined_schema) {
            schema_badge(current_mapping_.schema_type);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
            ImGui::Text("Custom");
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        // Field count
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("Fields");
        ImGui::PopStyleColor();
        ImGui::SameLine(70);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        ImGui::Text("%d", static_cast<int>(current_mapping_.field_mappings.size()));
        ImGui::PopStyleColor();

        // Sample data status
        ImGui::Spacing();
        if (!sample_data_json_.empty()) {
            theme::StatusIndicator("Sample data loaded", true);
        } else {
            theme::StatusIndicator("No sample data", false);
        }

        // Test status
        if (test_result_) {
            theme::StatusIndicator(
                test_result_->success ? "Last test: PASSED" : "Last test: FAILED",
                test_result_->success);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Navigation buttons
        const int step_idx = static_cast<int>(current_step_);
        if (step_idx > 0) {
            if (theme::SecondaryButton("< Back", {w - 20, 0})) {
                current_step_ = ALL_STEPS[step_idx - 1];
            }
            ImGui::Spacing();
        }
        if (step_idx < NUM_STEPS - 1) {
            if (theme::AccentButton("Next >", {w - 20, 0})) {
                current_step_ = ALL_STEPS[step_idx + 1];
            }
        }

    } else {
        // ---- List/Templates mode: overview + schema filter ----
        theme::SectionHeader("OVERVIEW");
        ImGui::Spacing();

        // Stats
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        ImGui::Text("%d", static_cast<int>(saved_mappings_.size()));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("mappings saved");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        ImGui::Text("%d", static_cast<int>(templates_.size()));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("templates available");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Schema filter (clickable)
        theme::SectionHeader("FILTER BY SCHEMA");
        ImGui::Spacing();

        // "All" option
        {
            const bool selected = (filter_schema_idx_ == -1);
            if (selected) ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
            else ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);

            if (ImGui::Selectable("  All", selected)) filter_schema_idx_ = -1;
            ImGui::PopStyleColor();
        }

        const auto schema_names = get_schema_type_names();
        for (int i = 0; i < static_cast<int>(schema_names.size()); ++i) {
            int count = 0;
            for (const auto& m : saved_mappings_) {
                if (m.is_predefined_schema &&
                    schema_type_id(m.schema_type) == schema_names[static_cast<size_t>(i)]) {
                    ++count;
                }
            }

            const bool selected = (filter_schema_idx_ == i);
            if (selected) ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
            else if (count > 0) ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
            else ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);

            char label[64];
            std::snprintf(label, sizeof(label), "  %s (%d)", schema_names[static_cast<size_t>(i)].c_str(), count);
            if (ImGui::Selectable(label, selected)) {
                filter_schema_idx_ = selected ? -1 : i;
            }
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Quick actions
        if (theme::AccentButton("+ New Mapping", {w - 20, 0})) {
            view_ = MappingView::create;
            reset_create_form();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Main content area
// ============================================================================

void DataMappingScreen::render_content(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({w, h});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARKEST);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12, 10});

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
// List View — card-style rows with badges, method tags, inline actions
// ============================================================================

void DataMappingScreen::render_list_view(float w, float /*h*/) {
    if (saved_mappings_.empty()) {
        // Empty state
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::SetCursorPosX((w - 300) * 0.5f);
        ImGui::Text("No mappings configured yet.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::SetCursorPosX((w - 160) * 0.5f);
        if (theme::AccentButton("Create First Mapping")) {
            view_ = MappingView::create;
            reset_create_form();
        }
        return;
    }

    const std::string filter(search_buf_);
    const auto schema_names = get_schema_type_names();

    // Count visible items for header
    int visible_count = 0;

    for (size_t i = 0; i < saved_mappings_.size(); ++i) {
        const auto& m = saved_mappings_[i];

        // Search filter
        if (!filter.empty()) {
            std::string name_lower = m.name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                [](unsigned char c) { return std::tolower(c); });
            std::string filter_lower = filter;
            std::transform(filter_lower.begin(), filter_lower.end(),
                filter_lower.begin(), [](unsigned char c) { return std::tolower(c); });
            if (name_lower.find(filter_lower) == std::string::npos) continue;
        }

        // Schema filter
        if (filter_schema_idx_ >= 0 && m.is_predefined_schema) {
            if (schema_type_id(m.schema_type) !=
                schema_names[static_cast<size_t>(filter_schema_idx_)]) continue;
        }

        visible_count++;

        // Card for each mapping
        ImGui::PushID(static_cast<int>(i));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
        ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10, 8});
        ImGui::BeginChild("##map_card", {0, 0}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

        // Row 1: Name + Schema badge + Method badge
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        ImGui::Text("%s", m.name.c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine(w * 0.35f);
        if (m.is_predefined_schema) {
            schema_badge(m.schema_type);
        } else {
            tag_pill("Custom", TEXT_DIM);
        }

        ImGui::SameLine(w * 0.48f);
        method_badge(m.api_config.method);

        // Field count
        ImGui::SameLine(w * 0.58f);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("%d fields", static_cast<int>(m.field_mappings.size()));
        ImGui::PopStyleColor();

        // Actions (right-aligned)
        ImGui::SameLine(w * 0.72f);
        if (theme::SecondaryButton("Edit")) {
            edit_mapping(m);
        }
        ImGui::SameLine();
        if (theme::SecondaryButton("Dup")) {
            mapping_ops::duplicate_mapping(m.id);
            load_mappings();
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, {ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.15f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.30f});
        ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
        if (ImGui::Button("Del")) {
            pending_delete_id_ = m.id;
        }
        ImGui::PopStyleColor(3);

        // Row 2: Endpoint + Updated date
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        const std::string endpoint_display =
            m.api_config.base_url.substr(0, std::min<size_t>(40, m.api_config.base_url.size())) +
            m.api_config.endpoint;
        ImGui::Text("%s", endpoint_display.c_str());
        ImGui::PopStyleColor();

        if (!m.updated_at.empty()) {
            ImGui::SameLine(w * 0.72f);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
            ImGui::Text("Updated %s", m.updated_at.substr(0, 10).c_str());
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        ImGui::PopID();
    }

    if (visible_count == 0) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("No mappings match the current filter.");
        ImGui::PopStyleColor();
    }

    // Delete confirmation popup
    if (!pending_delete_id_.empty()) {
        ImGui::OpenPopup("Confirm Delete");
    }
    if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        ImGui::Text("Delete this mapping permanently?");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        if (theme::AccentButton("Delete")) {
            delete_mapping_by_id(pending_delete_id_);
            pending_delete_id_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (theme::SecondaryButton("Cancel")) {
            pending_delete_id_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ============================================================================
// Create View — horizontal stepper + step content
// ============================================================================

void DataMappingScreen::render_create_view(float w, float h) {
    // Horizontal step progress bar
    render_step_progress_bar(w);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Step content
    switch (current_step_) {
        case CreateStep::api_config:     render_step_api_config(w, h); break;
        case CreateStep::schema_select:  render_step_schema_select(w, h); break;
        case CreateStep::field_mapping:  render_step_field_mapping(w, h); break;
        case CreateStep::cache_settings: render_step_cache_settings(w, h); break;
        case CreateStep::test_save:      render_step_test_save(w, h); break;
    }
}

// ============================================================================
// Horizontal step progress bar — circles connected by lines
// ============================================================================

void DataMappingScreen::render_step_progress_bar(float w) {
    const float step_area_w = w - 40;
    const float step_spacing = step_area_w / (NUM_STEPS);
    const float circle_r = 12.0f;
    const float start_x = ImGui::GetCursorScreenPos().x + 20;
    const float center_y = ImGui::GetCursorScreenPos().y + circle_r + 2;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < NUM_STEPS; ++i) {
        const float cx = start_x + step_spacing * i + step_spacing * 0.5f;
        const bool is_current = (ALL_STEPS[i] == current_step_);
        const bool is_past = (i < static_cast<int>(current_step_));
        const bool is_future = (i > static_cast<int>(current_step_));

        // Connecting line to next step
        if (i < NUM_STEPS - 1) {
            const float nx = start_x + step_spacing * (i + 1) + step_spacing * 0.5f;
            const ImU32 line_col = is_past
                ? ImGui::ColorConvertFloat4ToU32(SUCCESS)
                : ImGui::ColorConvertFloat4ToU32(BORDER_DIM);
            dl->AddLine({cx + circle_r + 4, center_y}, {nx - circle_r - 4, center_y}, line_col, 2.0f);
        }

        // Circle
        if (is_current) {
            // Active: filled orange with glow
            dl->AddCircleFilled({cx, center_y}, circle_r + 2,
                ImGui::ColorConvertFloat4ToU32({ACCENT.x, ACCENT.y, ACCENT.z, 0.2f}));
            dl->AddCircleFilled({cx, center_y}, circle_r,
                ImGui::ColorConvertFloat4ToU32(ACCENT));
            dl->AddCircle({cx, center_y}, circle_r,
                ImGui::ColorConvertFloat4ToU32({1, 1, 1, 0.3f}), 0, 2.0f);
        } else if (is_past) {
            // Completed: green filled with checkmark
            dl->AddCircleFilled({cx, center_y}, circle_r,
                ImGui::ColorConvertFloat4ToU32(SUCCESS));
            // Checkmark
            dl->AddLine({cx - 4, center_y}, {cx - 1, center_y + 3},
                ImGui::ColorConvertFloat4ToU32({1, 1, 1, 1}), 2.0f);
            dl->AddLine({cx - 1, center_y + 3}, {cx + 5, center_y - 4},
                ImGui::ColorConvertFloat4ToU32({1, 1, 1, 1}), 2.0f);
        } else {
            // Future: dim border only
            dl->AddCircle({cx, center_y}, circle_r,
                ImGui::ColorConvertFloat4ToU32(BORDER), 0, 2.0f);
        }

        // Step number in circle
        if (is_current || is_future) {
            char num[4];
            std::snprintf(num, sizeof(num), "%d", i + 1);
            const ImVec2 text_size = ImGui::CalcTextSize(num);
            const ImU32 text_col = is_current
                ? ImGui::ColorConvertFloat4ToU32({1, 1, 1, 1})
                : ImGui::ColorConvertFloat4ToU32(TEXT_DISABLED);
            dl->AddText({cx - text_size.x * 0.5f, center_y - text_size.y * 0.5f}, text_col, num);
        }

        // Label below circle (clickable)
        const char* step_label = create_step_label(ALL_STEPS[i]);
        const ImVec2 label_size = ImGui::CalcTextSize(step_label);
        const float label_x = cx - label_size.x * 0.5f;
        const float label_y = center_y + circle_r + 6;

        ImU32 label_col;
        if (is_current) label_col = ImGui::ColorConvertFloat4ToU32(ACCENT);
        else if (is_past) label_col = ImGui::ColorConvertFloat4ToU32(TEXT_SECONDARY);
        else label_col = ImGui::ColorConvertFloat4ToU32(TEXT_DISABLED);

        dl->AddText({label_x, label_y}, label_col, step_label);

        // Invisible button for clicking
        ImGui::SetCursorScreenPos({cx - circle_r - 4, center_y - circle_r - 4});
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##step", {circle_r * 2 + 8, circle_r * 2 + label_size.y + 14})) {
            current_step_ = ALL_STEPS[i];
        }
        ImGui::PopID();
    }

    // Reserve vertical space for the stepper
    ImGui::SetCursorScreenPos({ImGui::GetCursorScreenPos().x,
                                center_y + circle_r + 26});
    ImGui::Dummy({0, 0});
}

// ============================================================================
// Step 1: API Configuration — grouped into cards
// ============================================================================

void DataMappingScreen::render_step_api_config(float w, float /*h*/) {
    const float input_w = std::min(w * 0.55f, 500.0f);
    const float label_w = 90.0f;

    // === Connection Card ===
    card_begin("##card_conn", "API CONNECTION");

    inline_label("Name", label_w);
    ImGui::PushItemWidth(input_w);
    ImGui::InputText("##api_name", api_name_buf_, sizeof(api_name_buf_));
    ImGui::PopItemWidth();

    inline_label("Description", label_w);
    ImGui::PushItemWidth(input_w);
    ImGui::InputText("##api_desc", api_desc_buf_, sizeof(api_desc_buf_));
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Method + URL on same visual row
    static const char* methods[] = {"GET", "POST", "PUT", "DELETE", "PATCH"};
    inline_label("Method", label_w);
    ImGui::PushItemWidth(90);
    ImGui::Combo("##method", &api_method_idx_, methods, 5);
    ImGui::PopItemWidth();

    inline_label("Base URL", label_w);
    ImGui::PushItemWidth(input_w);
    ImGui::InputTextWithHint("##base_url", "https://api.example.com/v1", api_base_url_buf_, sizeof(api_base_url_buf_));
    ImGui::PopItemWidth();

    inline_label("Endpoint", label_w);
    ImGui::PushItemWidth(input_w);
    ImGui::InputTextWithHint("##endpoint", "/ticker/{symbol}/candles", api_endpoint_buf_, sizeof(api_endpoint_buf_));
    ImGui::PopItemWidth();

    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
    ImGui::Text("          Use {placeholders} for runtime parameters");
    ImGui::PopStyleColor();

    // Body for POST/PUT/PATCH
    if (api_method_idx_ >= 1 && api_method_idx_ <= 4) {
        ImGui::Spacing();
        inline_label("Body", label_w);
        ImGui::InputTextMultiline("##body", api_body_buf_, sizeof(api_body_buf_),
            {input_w, 80});
    }

    // Fetch button
    ImGui::Spacing();
    if (fetching_) {
        theme::LoadingSpinner("Fetching sample data...");
    } else {
        if (theme::AccentButton("Fetch Sample Data")) {
            fetch_sample_data();
        }
        if (!sample_data_json_.empty()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
            ImGui::Text("Sample data loaded (%d bytes)",
                static_cast<int>(sample_data_json_.size()));
            ImGui::PopStyleColor();
        }
    }

    card_end();

    // === Authentication Card ===
    render_auth_config(w);

    // === Headers Card ===
    render_headers_editor(w);

    // === Query Params Card ===
    render_query_params_editor(w);
}

// ============================================================================
// Auth config card
// ============================================================================

void DataMappingScreen::render_auth_config(float w) {
    card_begin("##card_auth", "AUTHENTICATION");

    const float input_w = std::min(w * 0.45f, 400.0f);
    const float label_w = 90.0f;
    static const char* auth_types[] = {"None", "API Key", "Bearer Token", "Basic Auth", "OAuth2"};

    inline_label("Type", label_w);
    ImGui::PushItemWidth(160);
    ImGui::Combo("##auth_type", &api_auth_type_idx_, auth_types, 5);
    ImGui::PopItemWidth();

    switch (api_auth_type_idx_) {
        case 1: { // API Key
            static const char* locations[] = {"Header", "Query Param"};
            ImGui::Spacing();
            inline_label("Location", label_w);
            ImGui::PushItemWidth(130);
            ImGui::Combo("##key_loc", &auth_key_location_idx_, locations, 2);
            ImGui::PopItemWidth();

            inline_label("Key Name", label_w);
            ImGui::PushItemWidth(input_w * 0.6f);
            ImGui::InputTextWithHint("##key_name", "X-API-Key", auth_key_name_buf_, sizeof(auth_key_name_buf_));
            ImGui::PopItemWidth();

            inline_label("Value", label_w);
            ImGui::PushItemWidth(input_w);
            ImGui::InputText("##key_val", auth_key_value_buf_, sizeof(auth_key_value_buf_),
                ImGuiInputTextFlags_Password);
            ImGui::PopItemWidth();
            break;
        }
        case 2: { // Bearer
            ImGui::Spacing();
            inline_label("Token", label_w);
            ImGui::PushItemWidth(input_w);
            ImGui::InputText("##bearer", auth_bearer_buf_, sizeof(auth_bearer_buf_),
                ImGuiInputTextFlags_Password);
            ImGui::PopItemWidth();
            break;
        }
        case 3: { // Basic
            ImGui::Spacing();
            inline_label("Username", label_w);
            ImGui::PushItemWidth(input_w * 0.6f);
            ImGui::InputText("##basic_user", auth_basic_user_buf_, sizeof(auth_basic_user_buf_));
            ImGui::PopItemWidth();

            inline_label("Password", label_w);
            ImGui::PushItemWidth(input_w * 0.6f);
            ImGui::InputText("##basic_pass", auth_basic_pass_buf_, sizeof(auth_basic_pass_buf_),
                ImGuiInputTextFlags_Password);
            ImGui::PopItemWidth();
            break;
        }
        default: break;
    }

    card_end();
}

// ============================================================================
// Headers editor card
// ============================================================================

void DataMappingScreen::render_headers_editor(float /*w*/) {
    card_begin("##card_headers", "CUSTOM HEADERS");

    auto& hdrs = current_mapping_.api_config.headers;
    std::string to_remove;

    // Existing headers as mini-rows
    for (const auto& [k, v] : hdrs) {
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        ImGui::Text("%s", k.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(150);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("%s", v.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 20);
        ImGui::PushID(k.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
        if (ImGui::SmallButton("x")) to_remove = k;
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    if (!to_remove.empty()) hdrs.erase(to_remove);

    // Add new
    ImGui::Spacing();
    ImGui::PushItemWidth(130);
    ImGui::InputTextWithHint("##hdr_key", "Key", header_key_buf_, sizeof(header_key_buf_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    ImGui::InputTextWithHint("##hdr_val", "Value", header_val_buf_, sizeof(header_val_buf_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (theme::SecondaryButton("+ Add")) {
        if (std::strlen(header_key_buf_) > 0) {
            hdrs[header_key_buf_] = header_val_buf_;
            header_key_buf_[0] = '\0';
            header_val_buf_[0] = '\0';
        }
    }

    card_end();
}

// ============================================================================
// Query params editor card
// ============================================================================

void DataMappingScreen::render_query_params_editor(float /*w*/) {
    card_begin("##card_qparams", "QUERY PARAMETERS");

    auto& qparams = current_mapping_.api_config.query_params;
    std::string to_remove;

    for (const auto& [k, v] : qparams) {
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        ImGui::Text("%s", k.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(150);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("%s", v.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 20);
        ImGui::PushID(("qp_" + k).c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
        if (ImGui::SmallButton("x")) to_remove = k;
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    if (!to_remove.empty()) qparams.erase(to_remove);

    ImGui::Spacing();
    ImGui::PushItemWidth(130);
    ImGui::InputTextWithHint("##qp_key", "Key", qparam_key_buf_, sizeof(qparam_key_buf_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    ImGui::InputTextWithHint("##qp_val", "Value", qparam_val_buf_, sizeof(qparam_val_buf_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (theme::SecondaryButton("+ Add")) {
        if (std::strlen(qparam_key_buf_) > 0) {
            qparams[qparam_key_buf_] = qparam_val_buf_;
            qparam_key_buf_[0] = '\0';
            qparam_val_buf_[0] = '\0';
        }
    }

    card_end();
}

// ============================================================================
// Step 2: Schema Selection
// ============================================================================

void DataMappingScreen::render_step_schema_select(float w, float /*h*/) {
    const float input_w = std::min(w * 0.55f, 500.0f);
    const float label_w = 100.0f;

    // === Schema Card ===
    card_begin("##card_schema", "TARGET SCHEMA");

    ImGui::Checkbox("Use predefined schema", &current_mapping_.is_predefined_schema);
    is_custom_schema_ = !current_mapping_.is_predefined_schema;

    ImGui::Spacing();

    if (current_mapping_.is_predefined_schema) {
        static const char* schema_items[] = {"OHLCV", "QUOTE", "TICK", "ORDER", "POSITION", "PORTFOLIO", "INSTRUMENT"};
        inline_label("Schema", label_w);
        ImGui::PushItemWidth(200);
        if (ImGui::Combo("##schema_type", &schema_type_idx_, schema_items, 7)) {
            current_mapping_.schema_type = static_cast<SchemaType>(schema_type_idx_);
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        schema_badge(current_mapping_.schema_type);

        // Schema fields table
        const auto* schema = get_schema_for_type(current_mapping_.schema_type);
        if (schema) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
            ImGui::Text("%s — %s", schema->name.c_str(), schema->description.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();

            if (ImGui::BeginTable("##schema_fields", 4,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                    {0, std::min(250.0f, schema->fields.size() * 26.0f + 30.0f)})) {

                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_None, input_w * 0.28f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, input_w * 0.14f);
                ImGui::TableSetupColumn("Required", ImGuiTableColumnFlags_None, input_w * 0.12f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_None, input_w * 0.46f);
                ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_WIDGET);
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor();

                for (const auto& f : schema->fields) {
                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
                    ImGui::Text("%s", f.name.c_str());
                    ImGui::PopStyleColor();

                    ImGui::TableNextColumn();
                    ImGui::PushStyleColor(ImGuiCol_Text, INFO);
                    ImGui::Text("%s", field_type_label(f.type));
                    ImGui::PopStyleColor();

                    ImGui::TableNextColumn();
                    if (f.required) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
                        ImGui::Text("YES");
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
                        ImGui::Text("no");
                    }
                    ImGui::PopStyleColor();

                    ImGui::TableNextColumn();
                    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
                    ImGui::Text("%s", f.description.c_str());
                    ImGui::PopStyleColor();
                }
                ImGui::EndTable();
            }
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("Custom schema fields are configured in the Field Mapping step.");
        ImGui::PopStyleColor();
    }

    card_end();

    // === Extraction Card ===
    card_begin("##card_extract", "DATA EXTRACTION");

    inline_label("Root Path", label_w);
    ImGui::PushItemWidth(300);
    ImGui::InputTextWithHint("##root_path", "e.g. data.candles or results",
        root_path_buf_, sizeof(root_path_buf_));
    ImGui::PopItemWidth();

    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
    ImGui::Text("          JSON path to the data array in the API response");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Checkbox("Source data is an array", &current_mapping_.extraction.is_array);
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Array of arrays", &current_mapping_.extraction.is_array_of_arrays);

    card_end();
}

// ============================================================================
// Step 3: Field Mapping
// ============================================================================

void DataMappingScreen::render_step_field_mapping(float w, float /*h*/) {
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

    card_begin("##card_fieldmap", "FIELD MAPPINGS");

    // Header row with add button
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
    ImGui::Text("Map source data fields to target schema fields.");
    ImGui::PopStyleColor();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 140);
    if (theme::AccentButton("+ Add Field")) {
        current_mapping_.field_mappings.push_back({});
    }

    ImGui::Spacing();

    // Field mapping table
    if (!current_mapping_.field_mappings.empty() &&
        ImGui::BeginTable("##field_map_table", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            {0, std::min(400.0f, current_mapping_.field_mappings.size() * 30.0f + 35.0f)})) {

        ImGui::TableSetupColumn("Target Field", ImGuiTableColumnFlags_None, w * 0.18f);
        ImGui::TableSetupColumn("Source Path", ImGuiTableColumnFlags_None, w * 0.22f);
        ImGui::TableSetupColumn("Parser", ImGuiTableColumnFlags_None, w * 0.10f);
        ImGui::TableSetupColumn("Transform", ImGuiTableColumnFlags_None, w * 0.18f);
        ImGui::TableSetupColumn("Req", ImGuiTableColumnFlags_None, w * 0.06f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoResize, 30.0f);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_WIDGET);
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

            // Transform
            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            char xform_buf[64] = {};
            if (!fm.transforms.empty()) {
                std::strncpy(xform_buf, fm.transforms[0].c_str(), sizeof(xform_buf) - 1);
            }
            if (ImGui::InputTextWithHint("##xform", "e.g. toNumber", xform_buf, sizeof(xform_buf))) {
                if (std::strlen(xform_buf) > 0) {
                    if (fm.transforms.empty()) fm.transforms.push_back(xform_buf);
                    else fm.transforms[0] = xform_buf;
                } else {
                    fm.transforms.clear();
                }
            }
            ImGui::PopItemWidth();

            // Required checkbox
            ImGui::TableNextColumn();
            ImGui::Checkbox("##req", &fm.required);

            // Delete button
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
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

    // Available transforms reference
    ImGui::Spacing();
    if (ImGui::TreeNodeEx("Available Transforms", ImGuiTreeNodeFlags_None)) {
        const auto transforms = TransformRegistry::all_transforms();
        if (ImGui::BeginTable("##transforms_ref", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None, 140);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_None, 280);
            ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_None, 80);
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_WIDGET);
            ImGui::TableHeadersRow();
            ImGui::PopStyleColor();

            for (const auto& t : transforms) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
                ImGui::Text("%s", t.name);
                ImGui::PopStyleColor();
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
                ImGui::Text("%s", t.description);
                ImGui::PopStyleColor();
                ImGui::TableNextColumn();
                tag_pill(transform_category_label(t.category), TEXT_SECONDARY);
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }

    card_end();
}

// ============================================================================
// Step 4: Cache & Validation Settings
// ============================================================================

void DataMappingScreen::render_step_cache_settings(float /*w*/, float /*h*/) {
    // === Cache Card ===
    card_begin("##card_cache", "RESPONSE CACHING");

    ImGui::Checkbox("Enable caching", &current_mapping_.api_config.cache_enabled);

    if (current_mapping_.api_config.cache_enabled) {
        ImGui::Spacing();
        inline_label("TTL (seconds)", 110);
        ImGui::PushItemWidth(100);
        ImGui::InputInt("##cache_ttl", &current_mapping_.api_config.cache_ttl_seconds);
        ImGui::PopItemWidth();

        if (current_mapping_.api_config.cache_ttl_seconds < 0)
            current_mapping_.api_config.cache_ttl_seconds = 0;

        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
        if (current_mapping_.api_config.cache_ttl_seconds > 0) {
            const int mins = current_mapping_.api_config.cache_ttl_seconds / 60;
            if (mins > 0) ImGui::Text("              = %d min %d sec", mins,
                current_mapping_.api_config.cache_ttl_seconds % 60);
        } else {
            ImGui::Text("              0 = no caching");
        }
        ImGui::PopStyleColor();
    }

    card_end();

    // === Validation Card ===
    card_begin("##card_validation", "DATA VALIDATION");

    ImGui::Checkbox("Enable validation", &current_mapping_.validation.enabled);
    if (current_mapping_.validation.enabled) {
        ImGui::Spacing();
        ImGui::Checkbox("Strict mode (reject records with errors)", &current_mapping_.validation.strict_mode);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("Validates transformed data against the target schema.");
        ImGui::Text("Checks required fields, data types, and value ranges.");
        ImGui::PopStyleColor();
    }

    card_end();

    // === Post-Processing Card ===
    card_begin("##card_postproc", "POST-PROCESSING");

    // Limit
    int limit_val = current_mapping_.post_processing.limit.value_or(0);
    inline_label("Record limit", 110);
    ImGui::PushItemWidth(100);
    if (ImGui::InputInt("##pp_limit", &limit_val)) {
        current_mapping_.post_processing.limit =
            (limit_val > 0) ? std::optional<int>(limit_val) : std::nullopt;
    }
    ImGui::PopItemWidth();

    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
    ImGui::Text("              0 = no limit");
    ImGui::PopStyleColor();

    // Sort
    ImGui::Spacing();
    bool has_sort = current_mapping_.post_processing.sort.has_value();
    if (ImGui::Checkbox("Enable sorting", &has_sort)) {
        if (has_sort) current_mapping_.post_processing.sort = SortConfig{"timestamp", false};
        else current_mapping_.post_processing.sort = std::nullopt;
    }
    if (current_mapping_.post_processing.sort) {
        char sort_buf[64] = {};
        std::strncpy(sort_buf, current_mapping_.post_processing.sort->field.c_str(),
            sizeof(sort_buf) - 1);
        inline_label("Sort field", 110);
        ImGui::PushItemWidth(160);
        if (ImGui::InputText("##sort_field", sort_buf, sizeof(sort_buf))) {
            current_mapping_.post_processing.sort->field = sort_buf;
        }
        ImGui::PopItemWidth();
        ImGui::SameLine(0, 10);
        ImGui::Checkbox("Descending", &current_mapping_.post_processing.sort->descending);
    }

    // Deduplicate
    ImGui::Spacing();
    bool has_dedup = current_mapping_.post_processing.deduplicate_field.has_value();
    if (ImGui::Checkbox("Deduplicate", &has_dedup)) {
        if (has_dedup) current_mapping_.post_processing.deduplicate_field = "symbol";
        else current_mapping_.post_processing.deduplicate_field = std::nullopt;
    }
    if (current_mapping_.post_processing.deduplicate_field) {
        char dedup_buf[64] = {};
        std::strncpy(dedup_buf, current_mapping_.post_processing.deduplicate_field->c_str(),
            sizeof(dedup_buf) - 1);
        inline_label("Dedup field", 110);
        ImGui::PushItemWidth(160);
        if (ImGui::InputText("##dedup_field", dedup_buf, sizeof(dedup_buf))) {
            current_mapping_.post_processing.deduplicate_field = dedup_buf;
        }
        ImGui::PopItemWidth();
    }

    card_end();
}

// ============================================================================
// Step 5: Test & Save
// ============================================================================

void DataMappingScreen::render_step_test_save(float w, float h) {
    const float input_w = std::min(w * 0.55f, 500.0f);
    const float label_w = 90.0f;

    // === Details Card ===
    card_begin("##card_details", "MAPPING DETAILS");

    inline_label("Name", label_w);
    ImGui::PushItemWidth(input_w);
    ImGui::InputTextWithHint("##map_name", "My API Mapping", mapping_name_buf_, sizeof(mapping_name_buf_));
    ImGui::PopItemWidth();

    inline_label("Description", label_w);
    ImGui::PushItemWidth(input_w);
    ImGui::InputText("##map_desc", mapping_desc_buf_, sizeof(mapping_desc_buf_));
    ImGui::PopItemWidth();

    card_end();

    // === Test Card ===
    card_begin("##card_test", "TEST MAPPING");

    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("Paste sample JSON data to test the transformation pipeline:");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Sample data input
    static char sample_buf[8192] = {};
    if (!sample_data_json_.empty() && sample_buf[0] == '\0') {
        std::strncpy(sample_buf, sample_data_json_.c_str(),
            std::min<size_t>(sizeof(sample_buf) - 1, sample_data_json_.size()));
    }

    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputTextMultiline("##sample_json", sample_buf, sizeof(sample_buf),
        {std::min(w * 0.75f, 700.0f), 120});
    ImGui::PopStyleColor();

    ImGui::Spacing();

    if (testing_) {
        theme::LoadingSpinner("Running test...");
    } else {
        if (theme::AccentButton("Run Test")) {
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
            ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
            ImGui::Text("SUCCESS");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
            ImGui::Text("— %d records processed, %d returned in %lldms",
                test_result_->records_processed, test_result_->records_returned,
                static_cast<long long>(test_result_->duration_ms));
            ImGui::PopStyleColor();

            ImGui::Spacing();
            render_json_preview("Transformed Output", test_result_->data_json, 180);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
            ImGui::Text("FAILED");
            ImGui::PopStyleColor();
            for (const auto& err : test_result_->errors) {
                ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
                ImGui::Bullet();
                ImGui::Text("%s", err.c_str());
                ImGui::PopStyleColor();
            }
        }

        render_validation_results();
    }

    card_end();

    // Save button (prominent, bottom)
    ImGui::Spacing();
    ImGui::Spacing();
    if (theme::AccentButton(editing_existing_ ? "Update Mapping" : "Save Mapping",
            {180, 36})) {
        save_current_mapping();
    }
    ImGui::SameLine(0, 12);
    if (theme::SecondaryButton("Cancel", {100, 36})) {
        view_ = MappingView::list;
        load_mappings();
    }
}

// ============================================================================
// Templates view — card grid with category badges and tag pills
// ============================================================================

void DataMappingScreen::render_templates_view(float w, float /*h*/) {
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
    ImGui::Text("Pre-built mapping configurations for popular brokers and data providers.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (templates_.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("No templates available.");
        ImGui::PopStyleColor();
        return;
    }

    for (size_t i = 0; i < templates_.size(); ++i) {
        const auto& t = templates_[i];

        ImGui::PushID(static_cast<int>(i));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
        ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12, 8});
        ImGui::BeginChild("##tmpl_card", {0, 0}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

        // Row 1: Name + category badge + verified indicator
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        ImGui::Text("%s", t.name.c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine(w * 0.45f);
        tag_pill(t.category.c_str(), INFO);

        if (t.verified) {
            ImGui::SameLine();
            tag_pill("verified", SUCCESS);
        }
        if (t.official) {
            ImGui::SameLine();
            tag_pill("official", ACCENT);
        }

        // Use button (right-aligned)
        ImGui::SameLine(w * 0.80f);
        if (theme::AccentButton("Use Template")) {
            load_template(t);
        }

        // Row 2: Description
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::Text("%s", t.description.c_str());
        ImGui::PopStyleColor();

        // Row 3: Tags
        if (!t.tags.empty()) {
            for (const auto& tag : t.tags) {
                tag_pill(tag.c_str(), TEXT_SECONDARY);
                ImGui::SameLine(0, 4);
            }
            ImGui::NewLine();
        }

        // Instructions if available
        if (!t.instructions.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
            ImGui::Text("Tip: %s", t.instructions.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        ImGui::PopID();
    }
}

// ============================================================================
// Footer — stats + keyboard hints
// ============================================================================

void DataMappingScreen::render_footer(float w, float footer_h) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float chrome_top = ImGui::GetFrameHeight() + 4.0f;
    const float chrome_bot = ImGui::GetFrameHeight();
    const float screen_bottom = vp->WorkPos.y + chrome_top + (vp->WorkSize.y - chrome_top - chrome_bot);
    ImGui::SetNextWindowPos({vp->WorkPos.x, screen_bottom - footer_h});
    ImGui::SetNextWindowSize({w, footer_h});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12, 3});

    ImGui::Begin("##dm_footer", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking);

    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
    ImGui::Text("DATA MAPPING");
    ImGui::PopStyleColor();

    ImGui::SameLine(160);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("%d mappings  |  %d templates  |  7 schemas  |  19 transforms",
        static_cast<int>(saved_mappings_.size()),
        static_cast<int>(templates_.size()));
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// JSON Preview — syntax-colored read-only display
// ============================================================================

void DataMappingScreen::render_json_preview(const std::string& label,
                                             const std::string& json_str,
                                             float height) {
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    ImGui::Text("%s:", label.c_str());
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
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
// Validation results — inline colored indicators
// ============================================================================

void DataMappingScreen::render_validation_results() {
    if (!test_result_) return;

    ImGui::Spacing();
    if (test_result_->validation.errors.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
        ImGui::Text("Validation: PASSED");
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
        ImGui::Text("Validation: %d error(s)", static_cast<int>(test_result_->validation.errors.size()));
        ImGui::PopStyleColor();
        for (const auto& e : test_result_->validation.errors) {
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
            ImGui::Bullet();
            ImGui::Text("[%s] %s: %s", e.rule.c_str(), e.field.c_str(), e.message.c_str());
            ImGui::PopStyleColor();
        }
    }
}

// ============================================================================
// Actions — load, save, delete, test, fetch, edit, template, reset
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
    editing_existing_ = false;
    view_ = MappingView::list;
}

void DataMappingScreen::delete_mapping_by_id(const std::string& id) {
    mapping_ops::delete_mapping(id);
    load_mappings();
}

void DataMappingScreen::start_test() {
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
    editing_existing_ = false;

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
    editing_existing_ = true;

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
// Template loader — built-in broker templates
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
