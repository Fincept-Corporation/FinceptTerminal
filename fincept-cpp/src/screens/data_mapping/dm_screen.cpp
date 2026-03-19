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

} // namespace fincept::data_mapping
