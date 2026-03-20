#include "report_builder_screen.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

namespace fincept::report_builder {

using namespace theme::colors;
static constexpr const char* TAG = "ReportBuilder";

// Layout constants (ES.45)
static constexpr float TOOLBAR_HEIGHT      = 36.0f;
static constexpr float LEFT_PANEL_WIDTH    = 200.0f;
static constexpr float RIGHT_PANEL_WIDTH   = 260.0f;
static constexpr float COMPONENT_BTN_H     = 28.0f;

// ============================================================================
// Initialization
// ============================================================================

void ReportBuilderScreen::init() {
    LOG_INFO(TAG, "Initializing report builder");

    // Set default date
    const std::time_t now = std::time(nullptr);
    const std::tm* tm = std::localtime(&now);
    std::strftime(document_.date, sizeof(document_.date), "%Y-%m-%d", tm);

    initialized_ = true;
    LOG_INFO(TAG, "Report builder initialized");
}

// ============================================================================
// Main render
// ============================================================================

void ReportBuilderScreen::render() {
    if (!initialized_) init();

    ui::ScreenFrame frame("##report_builder_screen", ImVec2(0, 0), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    const float w = frame.width();
    const float h = frame.height();

    render_toolbar(w);

    const float body_y = ImGui::GetCursorPosY();
    const float body_h = h - body_y;

    if (body_h > 50.0f) {
        const float left_w = LEFT_PANEL_WIDTH;
        const float right_w = RIGHT_PANEL_WIDTH;
        const float center_w = w - left_w - right_w;

        render_component_list(0.0f, body_y, left_w, body_h);
        render_canvas(left_w, body_y, center_w, body_h);
        render_properties_panel(left_w + center_w, body_y, right_w, body_h);
    }

    // Modal dialogs
    if (show_template_gallery_) render_template_gallery();
    if (show_export_dialog_)    render_export_dialog();

    frame.end();
}

// ============================================================================
// Toolbar — file ops, template gallery, export
// ============================================================================

void ReportBuilderScreen::render_toolbar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##rb_toolbar", ImVec2(w, TOOLBAR_HEIGHT), false);

    ImGui::SetCursorPos(ImVec2(8.0f, 4.0f));

    // Document name (editable)
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputText("##doc_name", document_.name, sizeof(document_.name))) {
        is_dirty_ = true;
    }

    ImGui::SameLine(0.0f, 16.0f);

    // Template gallery
    if (ImGui::SmallButton("Templates")) {
        show_template_gallery_ = true;
    }

    ImGui::SameLine();

    // Add component dropdown
    if (ImGui::BeginCombo("##add_comp", "+ Add Component", ImGuiComboFlags_NoPreview)) {
        static constexpr ComponentType add_types[] = {
            ComponentType::heading, ComponentType::subheading, ComponentType::text,
            ComponentType::table, ComponentType::chart, ComponentType::divider,
            ComponentType::kpi, ComponentType::code_block, ComponentType::quote,
            ComponentType::list, ComponentType::disclaimer, ComponentType::page_break,
        };
        for (const auto t : add_types) {
            char label[128];
            std::snprintf(label, sizeof(label), "%s  %s",
                          component_type_icon(t), component_type_label(t));
            if (ImGui::Selectable(label)) {
                add_component(t);
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0.0f, 16.0f);

    // Export
    if (ImGui::SmallButton("Export")) {
        show_export_dialog_ = true;
    }

    ImGui::SameLine();

    // Page size
    ImGui::SetNextItemWidth(80.0f);
    static constexpr const char* page_labels[] = {"A4", "Letter", "Legal"};
    int ps_idx = static_cast<int>(document_.page_size);
    if (ImGui::Combo("##page_sz", &ps_idx, page_labels, 3)) {
        document_.page_size = static_cast<PageSize>(ps_idx);
        is_dirty_ = true;
    }

    ImGui::SameLine();

    // Orientation
    const bool is_portrait = (document_.orientation == PageOrientation::portrait);
    if (ImGui::SmallButton(is_portrait ? "Portrait" : "Landscape")) {
        document_.orientation = is_portrait ? PageOrientation::landscape : PageOrientation::portrait;
        is_dirty_ = true;
    }

    // Status on the right
    ImGui::SameLine(w - 180.0f);
    ImGui::TextColored(TEXT_DIM, "%d components", static_cast<int>(document_.components.size()));
    ImGui::SameLine();
    if (is_dirty_) {
        ImGui::TextColored(WARNING, " [Modified]");
    } else {
        ImGui::TextColored(SUCCESS, " [Saved]");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Component List (left sidebar) — palette + document structure
// ============================================================================

void ReportBuilderScreen::render_component_list(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##rb_complist", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "COMPONENTS");
    ImGui::Separator();

    // Quick-add buttons
    static constexpr struct { ComponentType type; const char* label; } quick_add[] = {
        {ComponentType::heading,    "H1 Heading"},
        {ComponentType::subheading, "H2 Subhead"},
        {ComponentType::text,       "Text Block"},
        {ComponentType::table,      "Table"},
        {ComponentType::chart,      "Chart"},
        {ComponentType::kpi,        "KPI Cards"},
        {ComponentType::divider,    "Divider"},
        {ComponentType::code_block, "Code"},
        {ComponentType::list,       "List"},
        {ComponentType::quote,      "Quote"},
    };

    for (const auto& qa : quick_add) {
        char label[128];
        std::snprintf(label, sizeof(label), "%s %s", component_type_icon(qa.type), qa.label);
        if (ImGui::Button(label, ImVec2(w - 16.0f, COMPONENT_BTN_H))) {
            add_component(qa.type);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ACCENT, "DOCUMENT STRUCTURE");
    ImGui::Separator();

    // List existing components with selection
    if (document_.components.empty()) {
        ImGui::TextColored(TEXT_DIM, "No components yet.");
        ImGui::TextColored(TEXT_DIM, "Click buttons above");
        ImGui::TextColored(TEXT_DIM, "to add content.");
    } else {
        for (size_t i = 0; i < document_.components.size(); ++i) {
            const auto& comp = document_.components[i];
            const bool selected = (comp.id == selected_component_id_);

            char label[256];
            // Show a preview snippet
            char snippet[40] = {};
            if (comp.content[0] != '\0') {
                std::strncpy(snippet, comp.content, sizeof(snippet) - 1);
                if (std::strlen(comp.content) >= sizeof(snippet) - 1) {
                    snippet[sizeof(snippet) - 4] = '.';
                    snippet[sizeof(snippet) - 3] = '.';
                    snippet[sizeof(snippet) - 2] = '.';
                }
            }
            std::snprintf(label, sizeof(label), "%s %s##comp_%d",
                          component_type_icon(comp.type),
                          snippet[0] ? snippet : component_type_label(comp.type),
                          comp.id);

            if (ImGui::Selectable(label, selected)) {
                selected_component_id_ = comp.id;
            }

            // Context menu for each component
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Move Up") && i > 0) {
                    move_component(comp.id, -1);
                }
                if (ImGui::MenuItem("Move Down") && i + 1 < document_.components.size()) {
                    move_component(comp.id, 1);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Duplicate")) {
                    duplicate_component(comp.id);
                }
                if (ImGui::MenuItem("Delete")) {
                    delete_component(comp.id);
                }
                ImGui::EndPopup();
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Canvas — document preview area
// ============================================================================

void ReportBuilderScreen::render_canvas(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##rb_canvas", ImVec2(w, h), true);

    // Page-like area centered in canvas
    const float page_w = std::min(w - 40.0f, 600.0f);
    const float page_x = (w - page_w) * 0.5f;

    ImGui::SetCursorPosX(page_x);

    // Document header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
    ImGui::BeginChild("##rb_page", ImVec2(page_w, h - 20.0f), true);

    // Title area
    if (document_.name[0] != '\0') {
        ImGui::Dummy(ImVec2(0, 8.0f));
        ImGui::SetCursorPosX(16.0f);
        ImGui::TextColored(ACCENT, "%s", document_.name);

        if (document_.author[0] != '\0' || document_.company[0] != '\0') {
            ImGui::SetCursorPosX(16.0f);
            ImGui::TextColored(TEXT_DIM, "%s%s%s  |  %s",
                               document_.author,
                               (document_.author[0] && document_.company[0]) ? " - " : "",
                               document_.company,
                               document_.date);
        }
        ImGui::Separator();
    }

    // Render each component
    if (document_.components.empty()) {
        ImGui::Dummy(ImVec2(0, h * 0.3f));
        const float tw = ImGui::CalcTextSize("Empty Report").x;
        ImGui::SetCursorPosX((page_w - tw) * 0.5f);
        ImGui::TextColored(TEXT_DIM, "Empty Report");

        const float tw2 = ImGui::CalcTextSize("Add components from the left panel").x;
        ImGui::SetCursorPosX((page_w - tw2) * 0.5f);
        ImGui::TextColored(TEXT_DISABLED, "Add components from the left panel");
    } else {
        for (const auto& comp : document_.components) {
            ImGui::PushID(comp.id);

            // Highlight selected
            const bool selected = (comp.id == selected_component_id_);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ACCENT_BG);
            }

            // Click to select
            const ImVec2 cursor = ImGui::GetCursorPos();
            render_component_preview(comp, page_w - 32.0f);

            // Invisible button overlaid for selection
            const ImVec2 after = ImGui::GetCursorPos();
            const float comp_h = after.y - cursor.y;
            ImGui::SetCursorPos(cursor);
            char sel_id[32];
            std::snprintf(sel_id, sizeof(sel_id), "##sel_%d", comp.id);
            if (ImGui::InvisibleButton(sel_id, ImVec2(page_w - 32.0f, std::max(comp_h, 4.0f)))) {
                selected_component_id_ = comp.id;
            }
            ImGui::SetCursorPos(after);

            if (selected) {
                ImGui::PopStyleColor();
            }

            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Component preview dispatchers
// ============================================================================

void ReportBuilderScreen::render_component_preview(const ReportComponent& comp, float w) {
    ImGui::SetCursorPosX(16.0f);

    switch (comp.type) {
        case ComponentType::heading:
        case ComponentType::subheading:
            render_heading_preview(comp, w);
            break;
        case ComponentType::text:
        case ComponentType::quote:
        case ComponentType::disclaimer:
            render_text_preview(comp, w);
            break;
        case ComponentType::table:
            render_table_preview(comp, w);
            break;
        case ComponentType::chart:
            render_chart_preview(comp, w);
            break;
        case ComponentType::kpi:
            render_kpi_preview(comp, w);
            break;
        case ComponentType::code_block:
            render_code_preview(comp, w);
            break;
        case ComponentType::list:
            render_list_preview(comp, w);
            break;
        case ComponentType::divider:
            ImGui::Separator();
            ImGui::Spacing();
            break;
        case ComponentType::page_break:
            ImGui::TextColored(TEXT_DIM, "--- Page Break ---");
            ImGui::Spacing();
            break;
        case ComponentType::cover_page:
        case ComponentType::image:
            ImGui::TextColored(TEXT_DIM, "[%s]", component_type_label(comp.type));
            if (comp.content[0]) ImGui::TextColored(TEXT_SECONDARY, "%s", comp.content);
            ImGui::Spacing();
            break;
    }
}

void ReportBuilderScreen::render_heading_preview(const ReportComponent& comp, float w) {
    const bool is_h1 = (comp.type == ComponentType::heading);
    const ImVec4& col = is_h1 ? TEXT_PRIMARY : ACCENT;
    const char* text = comp.content[0] ? comp.content : (is_h1 ? "Heading" : "Subheading");

    ImGui::Dummy(ImVec2(0, is_h1 ? 8.0f : 4.0f));
    ImGui::TextColored(col, "%s", text);
    if (is_h1) ImGui::Separator();
    ImGui::Spacing();
}

void ReportBuilderScreen::render_text_preview(const ReportComponent& comp, float w) {
    if (comp.type == ComponentType::quote) {
        ImGui::TextColored(TEXT_DIM, "|");
        ImGui::SameLine();
    }
    if (comp.type == ComponentType::disclaimer) {
        ImGui::TextColored(WARNING, "!");
        ImGui::SameLine();
    }

    const char* text = comp.content[0] ? comp.content : "Click to edit text...";
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
    ImGui::TextColored(comp.content[0] ? TEXT_SECONDARY : TEXT_DIM, "%s", text);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
}

void ReportBuilderScreen::render_table_preview(const ReportComponent& comp, float w) {
    const int rows = std::max(1, comp.table_rows);
    const int cols = std::max(1, comp.table_cols);

    if (ImGui::BeginTable("##tbl_prev", cols,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingStretchSame, ImVec2(w, 0))) {
        for (int r = 0; r < std::min(rows, 8); ++r) {
            ImGui::TableNextRow();
            for (int c = 0; c < cols; ++c) {
                ImGui::TableNextColumn();
                const int idx = r * cols + c;
                if (idx < static_cast<int>(comp.table_cells.size()) &&
                    !comp.table_cells[idx].empty()) {
                    ImGui::TextColored(r == 0 ? ACCENT : TEXT_SECONDARY,
                                       "%s", comp.table_cells[idx].c_str());
                } else {
                    ImGui::TextColored(TEXT_DIM, r == 0 ? "Header" : "--");
                }
            }
        }
        if (rows > 8) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "... +%d more rows", rows - 8);
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
}

void ReportBuilderScreen::render_chart_preview(const ReportComponent& comp, float w) {
    const float chart_h = 100.0f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(p, ImVec2(p.x + w, p.y + chart_h), IM_COL32(20, 20, 22, 255));
    draw->AddRect(p, ImVec2(p.x + w, p.y + chart_h), IM_COL32(70, 70, 75, 200));

    if (comp.chart_data.empty()) {
        // Placeholder
        const char* text = "[Chart - No data]";
        const ImVec2 ts = ImGui::CalcTextSize(text);
        draw->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (chart_h - ts.y) * 0.5f),
                      IM_COL32(136, 136, 145, 200), text);
    } else {
        // Simple bar chart
        const int n = static_cast<int>(comp.chart_data.size());
        double max_val = 1.0;
        for (const auto v : comp.chart_data) max_val = std::max(max_val, std::abs(v));

        const float bar_w = std::max(4.0f, (w - 20.0f) / static_cast<float>(n) - 2.0f);
        for (int i = 0; i < n; ++i) {
            const float bar_h = static_cast<float>(comp.chart_data[i] / max_val) * (chart_h - 20.0f);
            const float bx = p.x + 10.0f + static_cast<float>(i) * (bar_w + 2.0f);
            const float by = p.y + chart_h - 10.0f;
            const ImU32 col = (comp.chart_data[i] >= 0)
                ? IM_COL32(13, 217, 107, 200)
                : IM_COL32(245, 64, 64, 200);
            draw->AddRectFilled(ImVec2(bx, by - std::abs(bar_h)),
                                ImVec2(bx + bar_w, by), col);
        }
    }

    ImGui::Dummy(ImVec2(w, chart_h));
    ImGui::Spacing();
}

void ReportBuilderScreen::render_kpi_preview(const ReportComponent& comp, float w) {
    if (comp.kpis.empty()) {
        ImGui::TextColored(TEXT_DIM, "[KPI Cards - Click to configure]");
        ImGui::Spacing();
        return;
    }

    const int n = static_cast<int>(comp.kpis.size());
    const float card_w = std::max(80.0f, (w - static_cast<float>(n - 1) * 8.0f) / static_cast<float>(n));

    for (int i = 0; i < n; ++i) {
        if (i > 0) ImGui::SameLine(0.0f, 8.0f);
        const auto& k = comp.kpis[i];

        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
        char kid[32];
        std::snprintf(kid, sizeof(kid), "##kpi_%d_%d", comp.id, i);
        ImGui::BeginChild(kid, ImVec2(card_w, 60.0f), true);

        ImGui::TextColored(TEXT_DIM, "%s", k.label);
        ImGui::TextColored(TEXT_PRIMARY, "%s", k.value);
        const auto& tc = k.trend_up ? MARKET_GREEN : MARKET_RED;
        ImGui::TextColored(tc, "%s%.1f%%", k.trend_up ? "+" : "", k.change);

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::EndGroup();
    }
    ImGui::Spacing();
}

void ReportBuilderScreen::render_code_preview(const ReportComponent& comp, float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    char cid[32];
    std::snprintf(cid, sizeof(cid), "##code_%d", comp.id);
    ImGui::BeginChild(cid, ImVec2(w, 80.0f), true);

    ImGui::TextColored(TEXT_DIM, "%s", comp.language);
    ImGui::Separator();
    const char* code = comp.content[0] ? comp.content : "# code here...";
    ImGui::TextColored(MARKET_GREEN, "%s", code);

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void ReportBuilderScreen::render_list_preview(const ReportComponent& comp, float w) {
    if (comp.list_items.empty()) {
        ImGui::TextColored(TEXT_DIM, "[List - Click to add items]");
    } else {
        for (size_t i = 0; i < comp.list_items.size(); ++i) {
            if (comp.ordered) {
                ImGui::TextColored(TEXT_SECONDARY, "  %d. %s",
                                   static_cast<int>(i + 1), comp.list_items[i].c_str());
            } else {
                ImGui::TextColored(TEXT_SECONDARY, "  * %s", comp.list_items[i].c_str());
            }
        }
    }
    ImGui::Spacing();
}

// ============================================================================
// Properties Panel (right sidebar) — edit selected component
// ============================================================================

void ReportBuilderScreen::render_properties_panel(float x, float y, float w, float h) {
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##rb_props", ImVec2(w, h), true);

    const float input_w = w - 24.0f;

    // Document metadata section (always visible)
    ImGui::TextColored(ACCENT, "DOCUMENT");
    ImGui::Separator();

    ImGui::TextColored(TEXT_DIM, "Author");
    ImGui::SetNextItemWidth(input_w);
    if (ImGui::InputText("##doc_author", document_.author, sizeof(document_.author))) {
        is_dirty_ = true;
    }

    ImGui::TextColored(TEXT_DIM, "Company");
    ImGui::SetNextItemWidth(input_w);
    if (ImGui::InputText("##doc_company", document_.company, sizeof(document_.company))) {
        is_dirty_ = true;
    }

    ImGui::TextColored(TEXT_DIM, "Date");
    ImGui::SetNextItemWidth(input_w);
    if (ImGui::InputText("##doc_date", document_.date, sizeof(document_.date))) {
        is_dirty_ = true;
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Selected component properties
    ReportComponent* sel = nullptr;
    for (auto& comp : document_.components) {
        if (comp.id == selected_component_id_) {
            sel = &comp;
            break;
        }
    }

    if (!sel) {
        ImGui::TextColored(TEXT_DIM, "Select a component");
        ImGui::TextColored(TEXT_DIM, "to edit properties");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    ImGui::TextColored(ACCENT, "PROPERTIES");
    ImGui::TextColored(TEXT_DIM, "Type: %s", component_type_label(sel->type));
    ImGui::Separator();

    // Content editing (for text-based components)
    if (sel->type == ComponentType::heading || sel->type == ComponentType::subheading ||
        sel->type == ComponentType::text || sel->type == ComponentType::quote ||
        sel->type == ComponentType::disclaimer || sel->type == ComponentType::code_block) {

        ImGui::TextColored(TEXT_DIM, "Content");
        ImGui::SetNextItemWidth(input_w);
        if (ImGui::InputTextMultiline("##comp_content", sel->content, sizeof(sel->content),
                ImVec2(input_w, 120.0f))) {
            is_dirty_ = true;
        }
    }

    // Alignment
    if (sel->type == ComponentType::heading || sel->type == ComponentType::subheading ||
        sel->type == ComponentType::text) {
        ImGui::TextColored(TEXT_DIM, "Alignment");
        ImGui::SetNextItemWidth(input_w);
        static constexpr const char* align_labels[] = {"Left", "Center", "Right"};
        int align_idx = static_cast<int>(sel->alignment);
        if (ImGui::Combo("##comp_align", &align_idx, align_labels, 3)) {
            sel->alignment = static_cast<Alignment>(align_idx);
            is_dirty_ = true;
        }
    }

    // Font size
    if (sel->type != ComponentType::divider && sel->type != ComponentType::page_break) {
        ImGui::TextColored(TEXT_DIM, "Font Size");
        ImGui::SetNextItemWidth(input_w);
        if (ImGui::SliderInt("##comp_fs", &sel->font_size, 8, 36)) {
            is_dirty_ = true;
        }
    }

    // Bold / Italic
    if (sel->type == ComponentType::text || sel->type == ComponentType::heading) {
        if (ImGui::Checkbox("Bold", &sel->bold)) is_dirty_ = true;
        ImGui::SameLine();
        if (ImGui::Checkbox("Italic", &sel->italic)) is_dirty_ = true;
    }

    // Table config
    if (sel->type == ComponentType::table) {
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "Rows");
        ImGui::SetNextItemWidth(input_w);
        if (ImGui::SliderInt("##tbl_rows", &sel->table_rows, 1, 50)) {
            is_dirty_ = true;
        }

        ImGui::TextColored(TEXT_DIM, "Columns");
        ImGui::SetNextItemWidth(input_w);
        if (ImGui::SliderInt("##tbl_cols", &sel->table_cols, 1, 10)) {
            is_dirty_ = true;
        }

        // Resize cells vector
        const int needed = sel->table_rows * sel->table_cols;
        if (static_cast<int>(sel->table_cells.size()) < needed) {
            sel->table_cells.resize(needed);
        }

        // Edit cells (show up to 6 rows inline)
        const int show_rows = std::min(sel->table_rows, 6);
        for (int r = 0; r < show_rows; ++r) {
            for (int c = 0; c < sel->table_cols; ++c) {
                const int idx = r * sel->table_cols + c;
                char cell_id[32];
                std::snprintf(cell_id, sizeof(cell_id), "##cell_%d_%d", r, c);
                ImGui::SetNextItemWidth(input_w / static_cast<float>(sel->table_cols) - 4.0f);
                if (c > 0) ImGui::SameLine();

                // Use a temp buffer for editing
                char buf[128] = {};
                std::strncpy(buf, sel->table_cells[idx].c_str(), sizeof(buf) - 1);
                if (ImGui::InputText(cell_id, buf, sizeof(buf))) {
                    sel->table_cells[idx] = buf;
                    is_dirty_ = true;
                }
            }
        }
    }

    // Chart config
    if (sel->type == ComponentType::chart) {
        ImGui::TextColored(TEXT_DIM, "Chart Type");
        ImGui::SetNextItemWidth(input_w);
        static constexpr const char* ct_labels[] = {"Bar", "Line", "Pie", "Area"};
        int ct_idx = static_cast<int>(sel->chart_type);
        if (ImGui::Combo("##chart_type", &ct_idx, ct_labels, 4)) {
            sel->chart_type = static_cast<ChartType>(ct_idx);
            is_dirty_ = true;
        }

        // Data points
        ImGui::TextColored(TEXT_DIM, "Data Points (comma-sep)");
        ImGui::SetNextItemWidth(input_w);
        // Use content field for raw data input
        if (ImGui::InputText("##chart_data", sel->content, sizeof(sel->content))) {
            // Parse comma-separated values
            sel->chart_data.clear();
            const char* p = sel->content;
            while (*p) {
                char* end = nullptr;
                const double val = std::strtod(p, &end);
                if (end != p) {
                    sel->chart_data.push_back(val);
                    p = end;
                    while (*p == ',' || *p == ' ') ++p;
                } else {
                    break;
                }
            }
            is_dirty_ = true;
        }
    }

    // KPI config
    if (sel->type == ComponentType::kpi) {
        if (ImGui::SmallButton("+ Add KPI")) {
            KPIEntry k;
            std::strncpy(k.label, "Metric", sizeof(k.label) - 1);
            std::strncpy(k.value, "0", sizeof(k.value) - 1);
            sel->kpis.push_back(k);
            is_dirty_ = true;
        }

        for (size_t i = 0; i < sel->kpis.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Separator();
            ImGui::SetNextItemWidth(input_w * 0.5f);
            if (ImGui::InputText("##kpi_label", sel->kpis[i].label, sizeof(sel->kpis[i].label)))
                is_dirty_ = true;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(input_w * 0.4f);
            if (ImGui::InputText("##kpi_val", sel->kpis[i].value, sizeof(sel->kpis[i].value)))
                is_dirty_ = true;
            ImGui::SetNextItemWidth(input_w * 0.5f);
            if (ImGui::InputDouble("##kpi_chg", &sel->kpis[i].change, 0.1, 1.0, "%.1f%%"))
                is_dirty_ = true;
            ImGui::SameLine();
            if (ImGui::Checkbox("Up", &sel->kpis[i].trend_up))
                is_dirty_ = true;
            ImGui::PopID();
        }
    }

    // List config
    if (sel->type == ComponentType::list) {
        if (ImGui::Checkbox("Ordered", &sel->ordered)) is_dirty_ = true;
        if (ImGui::SmallButton("+ Add Item")) {
            sel->list_items.emplace_back("New item");
            is_dirty_ = true;
        }
        for (size_t i = 0; i < sel->list_items.size(); ++i) {
            char li_id[32];
            std::snprintf(li_id, sizeof(li_id), "##li_%d", static_cast<int>(i));
            ImGui::SetNextItemWidth(input_w - 30.0f);
            char buf[256] = {};
            std::strncpy(buf, sel->list_items[i].c_str(), sizeof(buf) - 1);
            if (ImGui::InputText(li_id, buf, sizeof(buf))) {
                sel->list_items[i] = buf;
                is_dirty_ = true;
            }
        }
    }

    // Code block language
    if (sel->type == ComponentType::code_block) {
        ImGui::TextColored(TEXT_DIM, "Language");
        ImGui::SetNextItemWidth(input_w);
        if (ImGui::InputText("##code_lang", sel->language, sizeof(sel->language))) {
            is_dirty_ = true;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Actions for selected component
    if (ImGui::SmallButton("Duplicate")) {
        duplicate_component(sel->id);
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
    if (ImGui::SmallButton("Delete")) {
        delete_component(sel->id);
    }
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Template Gallery Modal
// ============================================================================

void ReportBuilderScreen::render_template_gallery() {
    ImGui::OpenPopup("Template Gallery");
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Template Gallery", &show_template_gallery_)) {
        ImGui::TextColored(ACCENT, "SELECT A TEMPLATE");
        ImGui::Separator();

        if (ImGui::BeginTable("##tpl_grid", 2,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders,
                ImVec2(0, 300))) {
            ImGui::TableSetupColumn("Template", ImGuiTableColumnFlags_None, 0.5f);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_None, 0.5f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < TEMPLATE_PRESET_COUNT; ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                char label[128];
                std::snprintf(label, sizeof(label), "%s##tpl_%d", TEMPLATE_PRESETS[i].name, i);
                if (ImGui::Selectable(label, false, ImGuiSelectableFlags_SpanAllColumns)) {
                    apply_template_preset(i);
                    show_template_gallery_ = false;
                }

                ImGui::TableNextColumn();
                ImGui::TextColored(TEXT_DIM, "[%s] %s",
                                   TEMPLATE_PRESETS[i].category,
                                   TEMPLATE_PRESETS[i].description);
            }
            ImGui::EndTable();
        }

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            show_template_gallery_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ============================================================================
// Export Dialog
// ============================================================================

void ReportBuilderScreen::render_export_dialog() {
    ImGui::OpenPopup("Export Report");
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(350, 200), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Export Report", &show_export_dialog_)) {
        ImGui::TextColored(ACCENT, "EXPORT FORMAT");
        ImGui::Separator();

        static constexpr const char* fmt_labels[] = {"PDF", "CSV", "HTML", "Markdown"};
        int fmt_idx = static_cast<int>(export_format_);
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::Combo("##exp_fmt", &fmt_idx, fmt_labels, 4)) {
            export_format_ = static_cast<ExportFormat>(fmt_idx);
        }

        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "Document: %s", document_.name);
        ImGui::TextColored(TEXT_DIM, "Components: %d",
                           static_cast<int>(document_.components.size()));

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Export", ImVec2(120, 0))) {
            export_report(export_format_);
            if (export_success_) {
                show_export_dialog_ = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            show_export_dialog_ = false;
            ImGui::CloseCurrentPopup();
        }

        if (export_status_[0] != '\0') {
            ImGui::Spacing();
            const ImVec4& col = export_success_ ? theme::colors::SUCCESS : theme::colors::MARKET_RED;
            ImGui::TextColored(col, "%s", export_status_);
        }

        ImGui::EndPopup();
    }
}

// ============================================================================
// Actions
// ============================================================================

void ReportBuilderScreen::add_component(ComponentType type) {
    ReportComponent comp;
    comp.id = document_.next_id++;
    comp.type = type;

    // Set defaults based on type
    switch (type) {
        case ComponentType::heading:
            std::strncpy(comp.content, "Section Title", sizeof(comp.content) - 1);
            comp.font_size = 18;
            comp.bold = true;
            break;
        case ComponentType::subheading:
            std::strncpy(comp.content, "Subsection", sizeof(comp.content) - 1);
            comp.font_size = 14;
            break;
        case ComponentType::text:
            comp.font_size = 11;
            break;
        case ComponentType::table:
            comp.table_rows = 4;
            comp.table_cols = 3;
            comp.table_cells.resize(12);
            comp.table_cells[0] = "Column A";
            comp.table_cells[1] = "Column B";
            comp.table_cells[2] = "Column C";
            break;
        case ComponentType::chart:
            comp.chart_data = {10, 25, 15, 30, 20, 35};
            break;
        case ComponentType::kpi: {
            KPIEntry k1, k2, k3;
            std::strncpy(k1.label, "Revenue", sizeof(k1.label) - 1);
            std::strncpy(k1.value, "$1.2M", sizeof(k1.value) - 1);
            k1.change = 12.5; k1.trend_up = true;
            std::strncpy(k2.label, "Users", sizeof(k2.label) - 1);
            std::strncpy(k2.value, "45,230", sizeof(k2.value) - 1);
            k2.change = 8.3; k2.trend_up = true;
            std::strncpy(k3.label, "Churn", sizeof(k3.label) - 1);
            std::strncpy(k3.value, "2.1%", sizeof(k3.value) - 1);
            k3.change = -0.3; k3.trend_up = false;
            comp.kpis = {k1, k2, k3};
            break;
        }
        case ComponentType::code_block:
            std::strncpy(comp.language, "python", sizeof(comp.language) - 1);
            break;
        case ComponentType::list:
            comp.list_items = {"Item 1", "Item 2", "Item 3"};
            break;
        case ComponentType::disclaimer:
            std::strncpy(comp.content,
                "This report is for informational purposes only and does not constitute financial advice.",
                sizeof(comp.content) - 1);
            comp.font_size = 9;
            break;
        default:
            break;
    }

    document_.components.push_back(std::move(comp));
    selected_component_id_ = document_.components.back().id;
    is_dirty_ = true;

    LOG_INFO(TAG, "Added component: %s (id=%d)",
             component_type_label(type), selected_component_id_);
}

void ReportBuilderScreen::delete_component(int id) {
    auto& comps = document_.components;
    comps.erase(
        std::remove_if(comps.begin(), comps.end(),
            [id](const ReportComponent& c) { return c.id == id; }),
        comps.end());
    if (selected_component_id_ == id) {
        selected_component_id_ = -1;
    }
    is_dirty_ = true;
}

void ReportBuilderScreen::duplicate_component(int id) {
    for (size_t i = 0; i < document_.components.size(); ++i) {
        if (document_.components[i].id == id) {
            ReportComponent copy = document_.components[i];
            copy.id = document_.next_id++;
            document_.components.insert(document_.components.begin() + static_cast<long>(i + 1), std::move(copy));
            selected_component_id_ = document_.components[i + 1].id;
            is_dirty_ = true;
            break;
        }
    }
}

void ReportBuilderScreen::move_component(int id, int direction) {
    auto& comps = document_.components;
    for (size_t i = 0; i < comps.size(); ++i) {
        if (comps[i].id == id) {
            const int new_i = static_cast<int>(i) + direction;
            if (new_i >= 0 && new_i < static_cast<int>(comps.size())) {
                std::swap(comps[i], comps[new_i]);
                is_dirty_ = true;
            }
            break;
        }
    }
}

void ReportBuilderScreen::apply_template_preset(int preset_idx) {
    if (preset_idx < 0 || preset_idx >= TEMPLATE_PRESET_COUNT) return;

    document_.components.clear();
    document_.next_id = 1;
    std::strncpy(document_.name, TEMPLATE_PRESETS[preset_idx].name, sizeof(document_.name) - 1);
    std::strncpy(document_.description, TEMPLATE_PRESETS[preset_idx].description,
                 sizeof(document_.description) - 1);

    // All templates get a standard structure
    add_component(ComponentType::heading);    // Title
    add_component(ComponentType::text);       // Executive summary placeholder
    add_component(ComponentType::divider);
    add_component(ComponentType::subheading); // Section 1
    add_component(ComponentType::text);       // Body
    add_component(ComponentType::chart);      // Data viz
    add_component(ComponentType::divider);
    add_component(ComponentType::subheading); // Section 2

    // Finance-specific templates get KPI cards and tables
    if (preset_idx >= 1 && preset_idx <= 5) {
        add_component(ComponentType::kpi);
        add_component(ComponentType::table);
    }

    add_component(ComponentType::disclaimer);
    selected_component_id_ = -1;
    is_dirty_ = true;

    LOG_INFO(TAG, "Applied template: %s", TEMPLATE_PRESETS[preset_idx].name);
}

// ============================================================================
// Native Save Dialog
// ============================================================================

std::string ReportBuilderScreen::native_save_dialog(const char* filter,
                                                     const char* title,
                                                     const char* default_ext) {
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    // Pre-fill with document name
    std::snprintf(filename, MAX_PATH - 1, "%s", document_.name);
    // Replace spaces with underscores for a safe filename
    for (char* p = filename; *p; ++p) {
        if (*p == ' ') *p = '_';
    }

    OPENFILENAMEA ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = nullptr;
    ofn.lpstrFilter  = filter;
    ofn.lpstrFile    = filename;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrTitle   = title;
    ofn.lpstrDefExt  = default_ext;
    ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn)) return std::string(filename);
    return "";
#else
    // Fallback: save to Documents or home directory
    const char* home = std::getenv("HOME");
    if (!home) home = ".";
    std::string path = std::string(home) + "/" + document_.name + "." + default_ext;
    // Replace spaces
    for (char& c : path) if (c == ' ') c = '_';
    return path;
#endif
}

// ============================================================================
// HTML Export
// ============================================================================

static std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;        break;
        }
    }
    return out;
}

bool ReportBuilderScreen::export_as_html(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
      << "<meta charset=\"UTF-8\">\n"
      << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
      << "<title>" << html_escape(document_.name) << "</title>\n"
      << "<style>\n"
      << "body{font-family:'Segoe UI',Arial,sans-serif;background:#1a1a1c;color:#e8e8ec;"
         "max-width:860px;margin:40px auto;padding:0 32px;line-height:1.6}\n"
      << "h1{font-size:2em;color:#ff730d;border-bottom:2px solid #ff730d;padding-bottom:8px;margin-top:32px}\n"
      << "h2{font-size:1.4em;color:#c8c8d0;margin-top:24px}\n"
      << "p{color:#b8b8bf;margin:8px 0}\n"
      << "blockquote{border-left:4px solid #ff730d;margin:12px 0;padding:8px 16px;"
         "background:#26262a;color:#b8b8bf}\n"
      << ".disclaimer{background:#2a2208;border:1px solid #ffc714;padding:10px 16px;"
         "border-radius:4px;color:#ffc714;font-size:0.85em;margin:12px 0}\n"
      << "table{border-collapse:collapse;width:100%;margin:12px 0}\n"
      << "th{background:#2a2a2e;color:#ff730d;padding:8px 12px;text-align:left;"
         "border:1px solid #3a3a3e}\n"
      << "td{padding:7px 12px;border:1px solid #2e2e32;color:#b8b8bf}\n"
      << "tr:nth-child(even) td{background:#1e1e22}\n"
      << ".kpi-row{display:flex;gap:12px;margin:12px 0;flex-wrap:wrap}\n"
      << ".kpi{background:#26262a;border:1px solid #3a3a3e;border-radius:6px;"
         "padding:14px 20px;min-width:120px;flex:1}\n"
      << ".kpi-label{font-size:0.8em;color:#888891;margin-bottom:4px}\n"
      << ".kpi-value{font-size:1.5em;font-weight:700;color:#f3f3f5}\n"
      << ".kpi-change.up{color:#0dd96b}.kpi-change.down{color:#f54040}\n"
      << ".chart{background:#141416;border:1px solid #3a3a3e;border-radius:4px;"
         "padding:12px;margin:12px 0;overflow:hidden}\n"
      << ".bar-chart{display:flex;align-items:flex-end;gap:4px;height:140px;padding:8px 0}\n"
      << ".bar{background:#ff730d;border-radius:3px 3px 0 0;min-width:20px;"
         "transition:opacity 0.2s;flex:1}\n"
      << ".bar.neg{background:#f54040}\n"
      << ".bar-label{font-size:0.7em;color:#888891;text-align:center;margin-top:4px}\n"
      << "pre{background:#0e0e10;border:1px solid #2a2a2e;border-radius:4px;"
         "padding:14px;overflow-x:auto;color:#0dd96b;font-size:0.9em}\n"
      << ".lang-tag{font-size:0.75em;color:#888891;margin-bottom:4px}\n"
      << "ul,ol{color:#b8b8bf;padding-left:24px;margin:8px 0}\n"
      << "li{margin:4px 0}\n"
      << "hr{border:none;border-top:1px solid #2e2e32;margin:20px 0}\n"
      << ".page-break{text-align:center;color:#444;font-size:0.8em;margin:20px 0;"
         "border-top:1px dashed #333;padding-top:8px}\n"
      << ".header{margin-bottom:32px;padding-bottom:16px;border-bottom:1px solid #2e2e32}\n"
      << ".meta{color:#888891;font-size:0.85em;margin-top:6px}\n"
      << "</style>\n</head>\n<body>\n";

    // Document header
    f << "<div class=\"header\">\n"
      << "<h1 style=\"border:none;margin:0\">" << html_escape(document_.name) << "</h1>\n";
    if (document_.author[0] || document_.company[0] || document_.date[0]) {
        f << "<div class=\"meta\">";
        if (document_.author[0])  f << html_escape(document_.author);
        if (document_.author[0] && document_.company[0]) f << " &mdash; ";
        if (document_.company[0]) f << html_escape(document_.company);
        if (document_.date[0])    f << " &nbsp;|&nbsp; " << html_escape(document_.date);
        f << "</div>\n";
    }
    f << "</div>\n";

    // Components
    for (const auto& comp : document_.components) {
        switch (comp.type) {
            case ComponentType::heading:
                f << "<h1>" << html_escape(comp.content) << "</h1>\n";
                break;
            case ComponentType::subheading:
                f << "<h2>" << html_escape(comp.content) << "</h2>\n";
                break;
            case ComponentType::text:
                f << "<p>" << html_escape(comp.content) << "</p>\n";
                break;
            case ComponentType::quote:
                f << "<blockquote>" << html_escape(comp.content) << "</blockquote>\n";
                break;
            case ComponentType::disclaimer:
                f << "<div class=\"disclaimer\">&#9888; " << html_escape(comp.content) << "</div>\n";
                break;
            case ComponentType::divider:
                f << "<hr>\n";
                break;
            case ComponentType::page_break:
                f << "<div class=\"page-break\" style=\"page-break-after:always\">"
                  << "&#8212; Page Break &#8212;</div>\n";
                break;
            case ComponentType::table: {
                const int rows = std::max(1, comp.table_rows);
                const int cols = std::max(1, comp.table_cols);
                f << "<table>\n";
                for (int r = 0; r < rows; ++r) {
                    f << "<tr>";
                    for (int c = 0; c < cols; ++c) {
                        const int idx = r * cols + c;
                        std::string cell;
                        if (idx < static_cast<int>(comp.table_cells.size()))
                            cell = comp.table_cells[idx];
                        if (r == 0)
                            f << "<th>" << html_escape(cell.empty() ? "Header" : cell) << "</th>";
                        else
                            f << "<td>" << html_escape(cell.empty() ? "" : cell) << "</td>";
                    }
                    f << "</tr>\n";
                }
                f << "</table>\n";
                break;
            }
            case ComponentType::chart: {
                f << "<div class=\"chart\">\n";
                const char* ct = chart_type_label(comp.chart_type);
                f << "<div style=\"color:#888891;font-size:0.8em;margin-bottom:8px\">"
                  << ct << " Chart</div>\n";
                if (!comp.chart_data.empty()) {
                    double max_val = 1.0;
                    for (double v : comp.chart_data) max_val = std::max(max_val, std::abs(v));
                    f << "<div class=\"bar-chart\">\n";
                    for (size_t i = 0; i < comp.chart_data.size(); ++i) {
                        const double v = comp.chart_data[i];
                        const int pct = static_cast<int>(std::abs(v) / max_val * 100.0);
                        const char* cls = (v >= 0) ? "bar" : "bar neg";
                        f << "<div style=\"display:flex;flex-direction:column;align-items:center;flex:1\">"
                          << "<div class=\"" << cls << "\" style=\"height:" << pct << "%\"></div>"
                          << "<div class=\"bar-label\">";
                        if (i < comp.chart_labels.size() && !comp.chart_labels[i].empty())
                            f << html_escape(comp.chart_labels[i]);
                        else
                            f << v;
                        f << "</div></div>\n";
                    }
                    f << "</div>\n";
                } else {
                    f << "<div style=\"color:#555;text-align:center;padding:40px 0\">No data</div>\n";
                }
                f << "</div>\n";
                break;
            }
            case ComponentType::kpi: {
                if (!comp.kpis.empty()) {
                    f << "<div class=\"kpi-row\">\n";
                    for (const auto& k : comp.kpis) {
                        const char* dir = k.trend_up ? "up" : "down";
                        const char* arrow = k.trend_up ? "&#8593;" : "&#8595;";
                        f << "<div class=\"kpi\">"
                          << "<div class=\"kpi-label\">" << html_escape(k.label) << "</div>"
                          << "<div class=\"kpi-value\">" << html_escape(k.value) << "</div>"
                          << "<div class=\"kpi-change " << dir << "\">"
                          << arrow << " ";
                        char chg[32];
                        std::snprintf(chg, sizeof(chg), "%.1f%%", std::abs(k.change));
                        f << chg << "</div></div>\n";
                    }
                    f << "</div>\n";
                }
                break;
            }
            case ComponentType::code_block:
                f << "<div class=\"lang-tag\">" << html_escape(comp.language) << "</div>\n"
                  << "<pre>" << html_escape(comp.content) << "</pre>\n";
                break;
            case ComponentType::list: {
                const char* tag = comp.ordered ? "ol" : "ul";
                f << "<" << tag << ">\n";
                for (const auto& item : comp.list_items)
                    f << "<li>" << html_escape(item) << "</li>\n";
                f << "</" << tag << ">\n";
                break;
            }
            case ComponentType::cover_page:
                f << "<div style=\"text-align:center;padding:60px 0\">"
                  << "<h1 style=\"font-size:2.5em\">" << html_escape(comp.content[0] ? comp.content : document_.name) << "</h1>"
                  << "</div>\n";
                break;
            case ComponentType::image:
                f << "<div style=\"text-align:center;padding:20px;border:1px dashed #444\">"
                  << "[Image: " << html_escape(comp.content) << "]</div>\n";
                break;
        }
    }

    f << "\n</body>\n</html>\n";
    return f.good();
}

// ============================================================================
// Markdown Export
// ============================================================================

bool ReportBuilderScreen::export_as_markdown(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    // YAML front matter
    f << "---\n"
      << "title: " << document_.name << "\n";
    if (document_.author[0])  f << "author: " << document_.author << "\n";
    if (document_.company[0]) f << "company: " << document_.company << "\n";
    if (document_.date[0])    f << "date: " << document_.date << "\n";
    f << "---\n\n";

    f << "# " << document_.name << "\n\n";

    for (const auto& comp : document_.components) {
        switch (comp.type) {
            case ComponentType::heading:
                f << "# " << comp.content << "\n\n";
                break;
            case ComponentType::subheading:
                f << "## " << comp.content << "\n\n";
                break;
            case ComponentType::text:
                f << comp.content << "\n\n";
                break;
            case ComponentType::quote:
                f << "> " << comp.content << "\n\n";
                break;
            case ComponentType::disclaimer:
                f << "> **Disclaimer:** " << comp.content << "\n\n";
                break;
            case ComponentType::divider:
                f << "---\n\n";
                break;
            case ComponentType::page_break:
                f << "\n<!-- page break -->\n\n";
                break;
            case ComponentType::table: {
                const int rows = std::max(1, comp.table_rows);
                const int cols = std::max(1, comp.table_cols);
                // Header row
                for (int c = 0; c < cols; ++c) {
                    const int idx = c;
                    std::string cell;
                    if (idx < static_cast<int>(comp.table_cells.size()))
                        cell = comp.table_cells[idx];
                    f << "| " << (cell.empty() ? "Header" : cell) << " ";
                }
                f << "|\n";
                // Separator
                for (int c = 0; c < cols; ++c) f << "| --- ";
                f << "|\n";
                // Data rows
                for (int r = 1; r < rows; ++r) {
                    for (int c = 0; c < cols; ++c) {
                        const int idx = r * cols + c;
                        std::string cell;
                        if (idx < static_cast<int>(comp.table_cells.size()))
                            cell = comp.table_cells[idx];
                        f << "| " << cell << " ";
                    }
                    f << "|\n";
                }
                f << "\n";
                break;
            }
            case ComponentType::chart: {
                f << "**" << chart_type_label(comp.chart_type) << " Chart**\n\n";
                if (!comp.chart_data.empty()) {
                    f << "| Index | Value |\n| --- | --- |\n";
                    for (size_t i = 0; i < comp.chart_data.size(); ++i) {
                        std::string label = (i < comp.chart_labels.size()) ? comp.chart_labels[i] : std::to_string(i + 1);
                        f << "| " << label << " | " << comp.chart_data[i] << " |\n";
                    }
                    f << "\n";
                }
                break;
            }
            case ComponentType::kpi: {
                if (!comp.kpis.empty()) {
                    f << "| Metric | Value | Change |\n| --- | --- | --- |\n";
                    for (const auto& k : comp.kpis) {
                        char chg[32];
                        std::snprintf(chg, sizeof(chg), "%+.1f%%", k.change);
                        f << "| " << k.label << " | " << k.value << " | " << chg << " |\n";
                    }
                    f << "\n";
                }
                break;
            }
            case ComponentType::code_block:
                f << "```" << comp.language << "\n"
                  << comp.content << "\n```\n\n";
                break;
            case ComponentType::list:
                for (size_t i = 0; i < comp.list_items.size(); ++i) {
                    if (comp.ordered)
                        f << (i + 1) << ". " << comp.list_items[i] << "\n";
                    else
                        f << "- " << comp.list_items[i] << "\n";
                }
                f << "\n";
                break;
            case ComponentType::cover_page:
                f << "# " << (comp.content[0] ? comp.content : document_.name) << "\n\n";
                break;
            case ComponentType::image:
                f << "![image](" << comp.content << ")\n\n";
                break;
        }
    }

    return f.good();
}

// ============================================================================
// CSV Export — flattens all table components; other blocks as comment rows
// ============================================================================

bool ReportBuilderScreen::export_as_csv(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "\"Report\",\"" << document_.name << "\"\n";
    if (document_.author[0])  f << "\"Author\",\"" << document_.author << "\"\n";
    if (document_.date[0])    f << "\"Date\",\"" << document_.date << "\"\n";
    f << "\n";

    // Lambda to quote a CSV field
    auto csv_cell = [](const std::string& s) -> std::string {
        std::string out = "\"";
        for (char c : s) {
            if (c == '"') out += "\"\"";
            else          out += c;
        }
        out += '"';
        return out;
    };

    for (const auto& comp : document_.components) {
        switch (comp.type) {
            case ComponentType::heading:
                f << csv_cell(std::string("## ") + comp.content) << "\n\n";
                break;
            case ComponentType::subheading:
                f << csv_cell(std::string("### ") + comp.content) << "\n\n";
                break;
            case ComponentType::text:
            case ComponentType::quote:
            case ComponentType::disclaimer:
                f << csv_cell(comp.content) << "\n\n";
                break;
            case ComponentType::table: {
                const int rows = std::max(1, comp.table_rows);
                const int cols = std::max(1, comp.table_cols);
                for (int r = 0; r < rows; ++r) {
                    for (int c = 0; c < cols; ++c) {
                        if (c > 0) f << ",";
                        const int idx = r * cols + c;
                        std::string cell;
                        if (idx < static_cast<int>(comp.table_cells.size()))
                            cell = comp.table_cells[idx];
                        f << csv_cell(cell);
                    }
                    f << "\n";
                }
                f << "\n";
                break;
            }
            case ComponentType::chart: {
                f << csv_cell(std::string(chart_type_label(comp.chart_type)) + " Chart") << "\n";
                f << "\"Label\",\"Value\"\n";
                for (size_t i = 0; i < comp.chart_data.size(); ++i) {
                    std::string label = (i < comp.chart_labels.size()) ? comp.chart_labels[i] : std::to_string(i + 1);
                    f << csv_cell(label) << "," << comp.chart_data[i] << "\n";
                }
                f << "\n";
                break;
            }
            case ComponentType::kpi: {
                f << "\"Metric\",\"Value\",\"Change\"\n";
                for (const auto& k : comp.kpis) {
                    char chg[32];
                    std::snprintf(chg, sizeof(chg), "%+.1f%%", k.change);
                    f << csv_cell(k.label) << "," << csv_cell(k.value) << "," << csv_cell(chg) << "\n";
                }
                f << "\n";
                break;
            }
            case ComponentType::list:
                for (size_t i = 0; i < comp.list_items.size(); ++i) {
                    if (comp.ordered)
                        f << (i + 1) << "," << csv_cell(comp.list_items[i]) << "\n";
                    else
                        f << csv_cell(comp.list_items[i]) << "\n";
                }
                f << "\n";
                break;
            default:
                break;
        }
    }

    return f.good();
}

// ============================================================================
// PDF Export — minimal valid PDF using stdlib only (no external libs)
// Generates text-based PDF with basic formatting for all component types
// ============================================================================

bool ReportBuilderScreen::export_as_pdf(const std::string& path) {
    // PDF uses points (72pt = 1 inch). A4 = 595 x 842 pt.
    static constexpr float PAGE_W = 595.0f;
    static constexpr float PAGE_H = 842.0f;
    static constexpr float MARGIN = 60.0f;
    static constexpr float TEXT_W = PAGE_W - 2.0f * MARGIN;

    // We build the PDF in memory so we can compute byte offsets for xref
    std::vector<std::string> objects;  // PDF objects (1-indexed, obj 0 unused)
    objects.emplace_back("");          // placeholder for obj 0

    // Helper: add object and return its 1-based id
    auto add_obj = [&](std::string s) -> int {
        objects.push_back(std::move(s));
        return static_cast<int>(objects.size()) - 1;
    };

    // Accumulate page content streams per page
    // We'll do simple text layout: cursor_y starts at top margin and decrements
    struct PageContent { std::string stream; };
    std::vector<PageContent> pages;
    pages.push_back({});

    float cursor_y = PAGE_H - MARGIN;
    const float line_h_normal = 14.0f;
    const float line_h_heading = 22.0f;
    const float line_h_sub = 18.0f;

    // Escape a string for PDF text (parentheses, backslash, non-ASCII)
    auto pdf_str = [](const std::string& in) -> std::string {
        std::string out;
        out.reserve(in.size() + 16);
        for (unsigned char c : in) {
            if (c == '(' || c == ')' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
            else if (c >= 32 && c < 127)            { out += static_cast<char>(c); }
            else                                    { /* skip non-printable */ }
        }
        return out;
    };

    // Emit text at position (x, y in PDF coordinates — origin bottom-left)
    auto emit_text = [&](const std::string& text, float x, float y,
                         float font_size, bool bold) -> std::string {
        std::ostringstream s;
        s << "BT\n"
          << "/" << (bold ? "F2" : "F1") << " " << font_size << " Tf\n"
          << x << " " << y << " Td\n"
          << "(" << pdf_str(text) << ") Tj\n"
          << "ET\n";
        return s.str();
    };

    // Wrap text into lines of approximately max_chars characters
    auto wrap_text = [](const std::string& text, int max_chars) -> std::vector<std::string> {
        std::vector<std::string> lines;
        if (text.empty()) { lines.emplace_back(""); return lines; }
        size_t pos = 0;
        while (pos < text.size()) {
            size_t end = std::min(pos + static_cast<size_t>(max_chars), text.size());
            // Try to break at last space
            if (end < text.size()) {
                size_t sp = text.rfind(' ', end);
                if (sp != std::string::npos && sp > pos) end = sp + 1;
            }
            lines.push_back(text.substr(pos, end - pos));
            pos = end;
        }
        return lines;
    };

    // Ensure space on page; start new page if needed
    auto ensure_space = [&](float needed) {
        if (cursor_y - needed < MARGIN) {
            pages.push_back({});
            cursor_y = PAGE_H - MARGIN;
        }
    };

    // Add a line of text to current page
    auto add_line = [&](const std::string& text, float font_size, bool bold,
                        float indent = 0.0f, float extra_above = 0.0f) {
        ensure_space(font_size + extra_above + 4.0f);
        cursor_y -= extra_above;
        pages.back().stream += emit_text(text, MARGIN + indent, cursor_y, font_size, bold);
        cursor_y -= (font_size + 2.0f);
    };

    // Draw a horizontal line
    auto add_rule = [&]() {
        ensure_space(10.0f);
        cursor_y -= 4.0f;
        std::ostringstream s;
        s << MARGIN << " " << cursor_y << " m "
          << (PAGE_W - MARGIN) << " " << cursor_y << " l S\n";
        pages.back().stream += s.str();
        cursor_y -= 6.0f;
    };

    // --- Render document title ---
    add_line(document_.name, 20.0f, true, 0.0f, 8.0f);
    if (document_.author[0] || document_.company[0] || document_.date[0]) {
        std::string meta;
        if (document_.author[0])  { meta += document_.author; }
        if (document_.author[0] && document_.company[0]) meta += " - ";
        if (document_.company[0]) meta += document_.company;
        if (document_.date[0])    { if (!meta.empty()) meta += "  |  "; meta += document_.date; }
        add_line(meta, 9.0f, false);
    }
    add_rule();

    // --- Render each component ---
    for (const auto& comp : document_.components) {
        switch (comp.type) {
            case ComponentType::heading:
                add_line(comp.content[0] ? comp.content : "Heading", line_h_heading, true, 0.0f, 8.0f);
                add_rule();
                break;
            case ComponentType::subheading:
                add_line(comp.content[0] ? comp.content : "Subheading", line_h_sub, true, 0.0f, 6.0f);
                break;
            case ComponentType::text:
            case ComponentType::disclaimer: {
                const auto lines = wrap_text(comp.content[0] ? comp.content : "", 85);
                for (const auto& ln : lines) add_line(ln, line_h_normal, false);
                cursor_y -= 4.0f;
                break;
            }
            case ComponentType::quote: {
                const auto lines = wrap_text(comp.content[0] ? comp.content : "", 78);
                for (const auto& ln : lines) add_line("    " + ln, line_h_normal, false, 8.0f);
                cursor_y -= 4.0f;
                break;
            }
            case ComponentType::divider:
                add_rule();
                break;
            case ComponentType::page_break:
                pages.push_back({});
                cursor_y = PAGE_H - MARGIN;
                break;
            case ComponentType::table: {
                const int rows = std::max(1, comp.table_rows);
                const int cols = std::max(1, comp.table_cols);
                const float col_w = TEXT_W / static_cast<float>(cols);
                const float row_h = 14.0f;
                ensure_space(static_cast<float>(rows) * row_h + 8.0f);
                cursor_y -= 4.0f;
                for (int r = 0; r < rows; ++r) {
                    for (int c = 0; c < cols; ++c) {
                        const int idx = r * cols + c;
                        std::string cell;
                        if (idx < static_cast<int>(comp.table_cells.size()))
                            cell = comp.table_cells[idx];
                        if (cell.empty()) cell = (r == 0) ? "Header" : "";
                        // Truncate to fit column
                        if (cell.size() > 20) cell = cell.substr(0, 17) + "...";
                        const float cx = MARGIN + static_cast<float>(c) * col_w;
                        pages.back().stream += emit_text(cell, cx, cursor_y,
                                                         line_h_normal - 2.0f, r == 0);
                    }
                    cursor_y -= row_h;
                    // Draw row separator
                    std::ostringstream s;
                    s << MARGIN << " " << cursor_y << " m "
                      << (PAGE_W - MARGIN) << " " << cursor_y << " l S\n";
                    pages.back().stream += s.str();
                }
                cursor_y -= 6.0f;
                break;
            }
            case ComponentType::kpi: {
                if (!comp.kpis.empty()) {
                    add_line("Key Performance Indicators", 11.0f, true, 0.0f, 4.0f);
                    for (const auto& k : comp.kpis) {
                        char buf[128];
                        std::snprintf(buf, sizeof(buf), "  %s: %s  (%+.1f%%)",
                                      k.label, k.value, k.change);
                        add_line(buf, line_h_normal, false);
                    }
                    cursor_y -= 4.0f;
                }
                break;
            }
            case ComponentType::chart: {
                add_line(std::string(chart_type_label(comp.chart_type)) + " Chart",
                         11.0f, true, 0.0f, 4.0f);
                if (!comp.chart_data.empty()) {
                    // ASCII bar representation
                    double max_val = 1.0;
                    for (double v : comp.chart_data) max_val = std::max(max_val, std::abs(v));
                    for (size_t i = 0; i < comp.chart_data.size(); ++i) {
                        const double v = comp.chart_data[i];
                        const int bars = static_cast<int>(std::abs(v) / max_val * 20.0);
                        std::string bar_str(static_cast<size_t>(bars), '#');
                        char buf[64];
                        std::snprintf(buf, sizeof(buf), "  [%2d] %-20s %.2f",
                                      static_cast<int>(i + 1), bar_str.c_str(), v);
                        add_line(buf, 9.0f, false);
                    }
                }
                cursor_y -= 4.0f;
                break;
            }
            case ComponentType::code_block: {
                add_line(std::string("[") + comp.language + "]", 9.0f, false, 0.0f, 4.0f);
                const auto lines = wrap_text(comp.content[0] ? comp.content : "", 80);
                for (const auto& ln : lines) add_line(ln, 9.0f, false, 8.0f);
                cursor_y -= 4.0f;
                break;
            }
            case ComponentType::list:
                for (size_t i = 0; i < comp.list_items.size(); ++i) {
                    std::string prefix = comp.ordered
                        ? std::to_string(i + 1) + ". "
                        : "* ";
                    add_line(prefix + comp.list_items[i], line_h_normal, false, 8.0f);
                }
                cursor_y -= 4.0f;
                break;
            case ComponentType::cover_page:
                add_line(comp.content[0] ? comp.content : document_.name,
                         24.0f, true, 0.0f, 40.0f);
                break;
            case ComponentType::image:
                add_line(std::string("[Image: ") + comp.content + "]",
                         line_h_normal, false, 0.0f, 4.0f);
                break;
        }
    }

    // --- Build PDF objects ---
    // Obj 1: Catalog
    add_obj("<< /Type /Catalog /Pages 2 0 R >>");
    const int catalog_id = static_cast<int>(objects.size()) - 1;

    // Build page content stream objects and collect page ids
    std::vector<int> page_ids;
    std::vector<int> content_ids;
    for (const auto& pg : pages) {
        const std::string& stream = pg.stream;
        std::ostringstream content_obj;
        content_obj << "<< /Length " << stream.size() << " >>\nstream\n"
                    << stream << "\nendstream";
        content_ids.push_back(add_obj(content_obj.str()));
    }

    // Obj 2: Pages dictionary (will be updated once we know page count)
    // We'll write it after building page objects
    // First build each page object
    for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
        std::ostringstream page_obj;
        page_obj << "<< /Type /Page /Parent 2 0 R "
                 << "/MediaBox [0 0 " << PAGE_W << " " << PAGE_H << "] "
                 << "/Contents " << content_ids[i] << " 0 R "
                 << "/Resources << /Font << "
                 << "/F1 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> "
                 << "/F2 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >> "
                 << ">> >> >>";
        page_ids.push_back(add_obj(page_obj.str()));
    }

    // Pages dictionary
    std::ostringstream pages_dict;
    pages_dict << "<< /Type /Pages /Count " << page_ids.size() << " /Kids [";
    for (int id : page_ids) pages_dict << id << " 0 R ";
    pages_dict << "] >>";
    // Insert at position 2 (0-indexed 2 = obj id 2)
    // We need obj 2 to be the pages dict, but objects are appended.
    // Simpler: re-number by just placing it correctly.
    // Actually let's rebuild using sequential numbering:
    // Reset and build properly with fixed numbering.
    objects.clear();
    objects.emplace_back("");  // obj 0 unused

    // Build all content streams first (they don't reference other objects)
    struct PdfObj { std::string body; };
    std::vector<PdfObj> pdf_objs;
    pdf_objs.push_back({""}); // 0 unused

    // Obj 1 = Catalog (references Pages = obj 2)
    pdf_objs.push_back({"<< /Type /Catalog /Pages 2 0 R >>"});

    // Obj 2 = Pages (placeholder; filled below)
    pdf_objs.push_back({"PAGES_PLACEHOLDER"});

    // Obj 3..N = content streams
    int first_content = static_cast<int>(pdf_objs.size());
    for (const auto& pg : pages) {
        const std::string& st = pg.stream;
        std::ostringstream o;
        o << "<< /Length " << st.size() << " >>\nstream\n" << st << "\nendstream";
        pdf_objs.push_back({o.str()});
    }

    // Obj (3+num_pages)..M = page objects
    int first_page = static_cast<int>(pdf_objs.size());
    for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
        const int content_obj_id = first_content + i;
        std::ostringstream o;
        o << "<< /Type /Page /Parent 2 0 R "
          << "/MediaBox [0 0 " << PAGE_W << " " << PAGE_H << "] "
          << "/Contents " << content_obj_id << " 0 R "
          << "/Resources << /Font << "
          << "/F1 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> "
          << "/F2 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >> "
          << ">> >> >>";
        pdf_objs.push_back({o.str()});
    }

    // Fill in Pages dict (obj 2)
    {
        std::ostringstream o;
        o << "<< /Type /Pages /Count " << pages.size() << " /Kids [";
        for (int i = 0; i < static_cast<int>(pages.size()); ++i)
            o << (first_page + i) << " 0 R ";
        o << "] >>";
        pdf_objs[2].body = o.str();
    }

    // Write PDF to file
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;

    const int num_objs = static_cast<int>(pdf_objs.size());
    std::vector<size_t> offsets(static_cast<size_t>(num_objs), 0);

    out << "%PDF-1.4\n";
    size_t pos = 9; // length of "%PDF-1.4\n"

    for (int i = 1; i < num_objs; ++i) {
        offsets[static_cast<size_t>(i)] = pos;
        std::ostringstream obj_str;
        obj_str << i << " 0 obj\n" << pdf_objs[static_cast<size_t>(i)].body << "\nendobj\n";
        const std::string s = obj_str.str();
        out << s;
        pos += s.size();
    }

    // xref table
    const size_t xref_offset = pos;
    out << "xref\n0 " << num_objs << "\n";
    out << "0000000000 65535 f \n";
    for (int i = 1; i < num_objs; ++i) {
        char entry[21];
        std::snprintf(entry, sizeof(entry), "%010zu 00000 n \n",
                      offsets[static_cast<size_t>(i)]);
        out << entry;
    }

    out << "trailer\n<< /Size " << num_objs
        << " /Root 1 0 R >>\nstartxref\n" << xref_offset << "\n%%EOF\n";

    return out.good();
}

// ============================================================================
// Export dispatcher
// ============================================================================

void ReportBuilderScreen::export_report(ExportFormat format) {
    export_status_[0] = '\0';
    export_success_   = false;

    const char* filter = nullptr;
    const char* title  = nullptr;
    const char* ext    = nullptr;

    switch (format) {
        case ExportFormat::pdf:      filter = "PDF Files\0*.pdf\0All Files\0*.*\0"; title = "Save PDF";      ext = "pdf";  break;
        case ExportFormat::html:     filter = "HTML Files\0*.html\0All Files\0*.*\0"; title = "Save HTML";   ext = "html"; break;
        case ExportFormat::csv:      filter = "CSV Files\0*.csv\0All Files\0*.*\0";  title = "Save CSV";     ext = "csv";  break;
        case ExportFormat::markdown: filter = "Markdown Files\0*.md\0All Files\0*.*\0"; title = "Save Markdown"; ext = "md"; break;
    }

    const std::string save_path = native_save_dialog(filter, title, ext);
    if (save_path.empty()) {
        // User cancelled — no error message needed
        return;
    }

    bool ok = false;
    switch (format) {
        case ExportFormat::pdf:      ok = export_as_pdf(save_path);      break;
        case ExportFormat::html:     ok = export_as_html(save_path);     break;
        case ExportFormat::csv:      ok = export_as_csv(save_path);      break;
        case ExportFormat::markdown: ok = export_as_markdown(save_path); break;
    }

    if (ok) {
        export_success_ = true;
        std::snprintf(export_status_, sizeof(export_status_),
                      "Saved: %s", save_path.c_str());
        is_dirty_ = false;
        LOG_INFO(TAG, "Exported report as %s -> %s", export_format_label(format), save_path.c_str());
    } else {
        export_success_ = false;
        std::snprintf(export_status_, sizeof(export_status_),
                      "Error: could not write to %s", save_path.c_str());
        LOG_ERROR(TAG, "Export failed: %s", save_path.c_str());
    }
}

} // namespace fincept::report_builder
