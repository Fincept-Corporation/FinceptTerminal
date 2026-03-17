// Notes Screen — Full CRUD implementation
// Bloomberg-Modern UI experiment: premium styling with local overrides
// Mirrors Tauri NotesTab: category sidebar, notes list, editor, viewer

#include "notes_screen.h"
#include "notes_types.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "imgui.h"
#include "imgui_internal.h"  // for GetWindowDrawList, advanced draw
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace fincept::notes {

using namespace theme::colors;

// ============================================================================
// Bloomberg-Modern local palette (overrides only within this screen)
// ============================================================================
namespace modern {
    // Layered backgrounds — subtle depth
    constexpr ImVec4 BG_BASE      = {0.055f, 0.055f, 0.067f, 1.0f};  // #0E0E11
    constexpr ImVec4 BG_SURFACE   = {0.078f, 0.078f, 0.094f, 1.0f};  // #141418
    constexpr ImVec4 BG_CARD      = {0.098f, 0.098f, 0.118f, 1.0f};  // #19191E
    constexpr ImVec4 BG_ELEVATED  = {0.118f, 0.118f, 0.141f, 1.0f};  // #1E1E24
    constexpr ImVec4 BG_HOVER     = {0.145f, 0.145f, 0.173f, 1.0f};  // #25252C

    // Accent — warm Bloomberg orange, refined
    constexpr ImVec4 ACCENT_BRIGHT = {1.0f, 0.50f, 0.08f, 1.0f};     // #FF8014
    constexpr ImVec4 ACCENT_GLOW   = {1.0f, 0.50f, 0.08f, 0.12f};    // glow layer
    constexpr ImVec4 ACCENT_SOFT   = {1.0f, 0.50f, 0.08f, 0.25f};    // selection bg

    // Text hierarchy
    constexpr ImVec4 TEXT_BRIGHT   = {0.96f, 0.96f, 0.98f, 1.0f};    // #F5F5FA
    constexpr ImVec4 TEXT_NORMAL   = {0.78f, 0.78f, 0.82f, 1.0f};    // #C7C7D1
    constexpr ImVec4 TEXT_MUTED    = {0.50f, 0.50f, 0.56f, 1.0f};    // #80808F
    constexpr ImVec4 TEXT_FAINT    = {0.35f, 0.35f, 0.40f, 1.0f};    // #595966

    // Borders — barely visible
    constexpr ImVec4 BORDER_SUBTLE = {0.18f, 0.18f, 0.22f, 0.40f};
    constexpr ImVec4 BORDER_HOVER  = {0.28f, 0.28f, 0.33f, 0.60f};

    // Semantic (refined, less saturated)
    constexpr ImVec4 GREEN   = {0.18f, 0.82f, 0.50f, 1.0f};
    constexpr ImVec4 RED     = {0.94f, 0.33f, 0.28f, 1.0f};
    constexpr ImVec4 AMBER   = {1.0f, 0.78f, 0.10f, 1.0f};
    constexpr ImVec4 BLUE    = {0.30f, 0.58f, 1.0f, 1.0f};

    // Style constants
    constexpr float ROUNDING     = 6.0f;
    constexpr float ROUNDING_SM  = 4.0f;
    constexpr float ROUNDING_XS  = 3.0f;
    constexpr float PANEL_PAD    = 12.0f;
}

// RAII style guard — pushes Bloomberg-modern overrides, pops on destruction
struct ModernStyleGuard {
    int color_count = 0;
    int var_count = 0;

    ModernStyleGuard() {
        auto push_col = [&](ImGuiCol idx, ImVec4 col) {
            ImGui::PushStyleColor(idx, col); color_count++;
        };
        auto push_var_f = [&](ImGuiStyleVar idx, float v) {
            ImGui::PushStyleVar(idx, v); var_count++;
        };
        auto push_var_v = [&](ImGuiStyleVar idx, ImVec2 v) {
            ImGui::PushStyleVar(idx, v); var_count++;
        };

        // Backgrounds
        push_col(ImGuiCol_WindowBg, modern::BG_BASE);
        push_col(ImGuiCol_ChildBg, modern::BG_SURFACE);
        push_col(ImGuiCol_PopupBg, modern::BG_ELEVATED);

        // Borders
        push_col(ImGuiCol_Border, modern::BORDER_SUBTLE);
        push_col(ImGuiCol_BorderShadow, {0, 0, 0, 0});

        // Text
        push_col(ImGuiCol_Text, modern::TEXT_BRIGHT);
        push_col(ImGuiCol_TextDisabled, modern::TEXT_FAINT);

        // Frames
        push_col(ImGuiCol_FrameBg, modern::BG_CARD);
        push_col(ImGuiCol_FrameBgHovered, modern::BG_HOVER);
        push_col(ImGuiCol_FrameBgActive, modern::BG_HOVER);

        // Buttons
        push_col(ImGuiCol_Button, modern::BG_ELEVATED);
        push_col(ImGuiCol_ButtonHovered, modern::BG_HOVER);
        push_col(ImGuiCol_ButtonActive, modern::ACCENT_SOFT);

        // Headers / selectables
        push_col(ImGuiCol_Header, modern::BG_CARD);
        push_col(ImGuiCol_HeaderHovered, modern::BG_HOVER);
        push_col(ImGuiCol_HeaderActive, modern::ACCENT_SOFT);

        // Separator
        push_col(ImGuiCol_Separator, modern::BORDER_SUBTLE);

        // Scrollbar
        push_col(ImGuiCol_ScrollbarBg, {0, 0, 0, 0.15f});
        push_col(ImGuiCol_ScrollbarGrab, {0.25f, 0.25f, 0.30f, 0.5f});
        push_col(ImGuiCol_ScrollbarGrabHovered, {0.35f, 0.35f, 0.40f, 0.7f});

        // Geometry
        push_var_f(ImGuiStyleVar_WindowRounding, modern::ROUNDING);
        push_var_f(ImGuiStyleVar_ChildRounding, modern::ROUNDING_SM);
        push_var_f(ImGuiStyleVar_FrameRounding, modern::ROUNDING_XS);
        push_var_f(ImGuiStyleVar_PopupRounding, modern::ROUNDING_SM);
        push_var_f(ImGuiStyleVar_ScrollbarRounding, modern::ROUNDING);
        push_var_f(ImGuiStyleVar_GrabRounding, modern::ROUNDING_XS);
        push_var_f(ImGuiStyleVar_TabRounding, modern::ROUNDING_XS);

        // Spacing — slightly more breathing room
        push_var_v(ImGuiStyleVar_WindowPadding, ImVec2(modern::PANEL_PAD, modern::PANEL_PAD));
        push_var_v(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
        push_var_v(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));

        // Softer borders
        push_var_f(ImGuiStyleVar_WindowBorderSize, 0.0f);
        push_var_f(ImGuiStyleVar_ChildBorderSize, 1.0f);
        push_var_f(ImGuiStyleVar_FrameBorderSize, 0.0f);
    }

    ~ModernStyleGuard() {
        ImGui::PopStyleVar(var_count);
        ImGui::PopStyleColor(color_count);
    }
};

// Draw helpers
namespace draw {

// Rounded rect with optional gradient (top-to-bottom)
inline void GradientRect(ImDrawList* dl, ImVec2 min, ImVec2 max,
                          ImU32 top_col, ImU32 bot_col, float rounding = 0) {
    if (rounding > 0) {
        // Draw two halves for gradient with rounding
        float mid_y = (min.y + max.y) * 0.5f;
        dl->AddRectFilled(min, ImVec2(max.x, mid_y), top_col, rounding, ImDrawFlags_RoundCornersTop);
        dl->AddRectFilled(ImVec2(min.x, mid_y), max, bot_col, rounding, ImDrawFlags_RoundCornersBottom);
    } else {
        dl->AddRectFilledMultiColor(min, max, top_col, top_col, bot_col, bot_col);
    }
}

// Left accent bar (vertical colored strip)
inline void AccentBar(ImDrawList* dl, ImVec2 pos, float height, ImU32 color,
                       float width = 3.0f, float rounding = 1.5f) {
    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), color, rounding);
}

// Subtle glow behind a rect (soft shadow/bloom)
inline void SoftGlow(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color,
                      float expand = 6.0f, float rounding = 6.0f) {
    ImVec2 glow_min = {min.x - expand, min.y - expand};
    ImVec2 glow_max = {max.x + expand, max.y + expand};
    dl->AddRectFilled(glow_min, glow_max, color, rounding + expand * 0.5f);
}

// Modern button with accent styling
inline bool AccentButton(const char* label, ImVec2 size = {0, 0}) {
    ImGui::PushStyleColor(ImGuiCol_Button, modern::ACCENT_BRIGHT);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.58f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.42f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));  // dark text on bright
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, modern::ROUNDING_SM);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 8));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return clicked;
}

// Ghost/outline button
inline bool GhostButton(const char* label, ImVec2 size = {0, 0}, bool active = false) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, modern::ACCENT_SOFT);
        ImGui::PushStyleColor(ImGuiCol_Text, modern::ACCENT_BRIGHT);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, modern::TEXT_NORMAL);
    }
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, modern::BG_HOVER);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, modern::ROUNDING_XS);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return clicked;
}

// Danger button (red)
inline bool DangerButton(const char* label, ImVec2 size = {0, 0}) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.12f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.22f, 0.22f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, modern::ROUNDING_XS);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return clicked;
}

} // namespace draw

// ============================================================================
// Main render
// ============================================================================
void NotesScreen::render() {
    if (!data_loaded_ && !loading_) {
        load_notes();
    }

    // Check async completion
    if (loading_ && load_future_.valid() &&
        load_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        load_future_.get();
        loading_ = false;
        apply_filters();
    }

    // Bloomberg-Modern style guard — all overrides scoped to this screen
    ModernStyleGuard style_guard;

    ui::ScreenFrame frame("##notes_screen", ImVec2(10, 10), modern::BG_BASE);
    if (!frame.begin()) { frame.end(); return; }

    // Toolbar at top
    render_toolbar();

    ImGui::Spacing();

    float toolbar_h = ImGui::GetCursorPosY();
    float status_h = 30.0f;
    float body_h = frame.height() - toolbar_h - status_h - 4;
    if (body_h < 100) body_h = 100;

    // Body: sidebar + list + editor/viewer
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));  // transparent wrapper
    ImGui::BeginChild("##notes_body", ImVec2(0, body_h), false);
    {
        float sidebar_w = 210.0f;
        float list_w = 320.0f;

        // Category sidebar
        ImGui::PushStyleColor(ImGuiCol_ChildBg, modern::BG_SURFACE);
        ImGui::BeginChild("##cat_sidebar", ImVec2(sidebar_w, 0), true);
        render_category_sidebar();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 6);

        // Notes list
        ImGui::PushStyleColor(ImGuiCol_ChildBg, modern::BG_SURFACE);
        ImGui::BeginChild("##notes_list", ImVec2(list_w, 0), true);
        render_notes_list();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 6);

        // Right panel: editor or viewer
        ImGui::PushStyleColor(ImGuiCol_ChildBg, modern::BG_SURFACE);
        ImGui::BeginChild("##note_detail", ImVec2(0, 0), true);
        if (view_mode_ == ViewMode::Editor) {
            render_note_editor();
        } else if (view_mode_ == ViewMode::Viewer && selected_note_) {
            render_note_viewer();
        } else {
            // Empty state — centered message with subtle icon
            auto region = ImGui::GetContentRegionAvail();
            float cx = region.x * 0.5f;
            float cy = region.y * 0.5f;

            ImGui::SetCursorPos(ImVec2(cx - 60, cy - 30));
            ImGui::PushStyleColor(ImGuiCol_Text, modern::TEXT_FAINT);
            ImGui::Text("[  ]");
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(ImVec2(cx - 120, cy));
            ImGui::PushStyleColor(ImGuiCol_Text, modern::TEXT_MUTED);
            ImGui::TextWrapped("Select a note or click 'New Note' to get started");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Status bar
    render_status_bar();

    frame.end();

    // Delete confirmation popup
    render_delete_confirm_popup();
}

// ============================================================================
// Data loading
// ============================================================================
void NotesScreen::load_notes() {
    loading_ = true;
    load_future_ = std::async(std::launch::async, [this]() {
        try {
            all_notes_ = db::ops::get_all_notes();
        } catch (...) {
            all_notes_.clear();
        }
        data_loaded_ = true;
    });
}

void NotesScreen::apply_filters() {
    filtered_notes_.clear();
    for (auto& note : all_notes_) {
        // Archive filter
        if (note.is_archived && !show_archived_) continue;
        if (!note.is_archived && show_archived_) continue;

        // Favorites filter
        if (show_favorites_only_ && !note.is_favorite) continue;

        // Category filter
        if (selected_category_ != "ALL" && note.category != selected_category_) continue;

        // Search filter
        if (search_query_.size() > 0) {
            std::string q = search_query_;
            std::transform(q.begin(), q.end(), q.begin(), ::tolower);

            std::string title_lower = note.title;
            std::transform(title_lower.begin(), title_lower.end(), title_lower.begin(), ::tolower);
            std::string content_lower = note.content;
            std::transform(content_lower.begin(), content_lower.end(), content_lower.begin(), ::tolower);
            std::string tags_lower = note.tags;
            std::transform(tags_lower.begin(), tags_lower.end(), tags_lower.begin(), ::tolower);
            std::string tickers_lower = note.tickers;
            std::transform(tickers_lower.begin(), tickers_lower.end(), tickers_lower.begin(), ::tolower);

            if (title_lower.find(q) == std::string::npos &&
                content_lower.find(q) == std::string::npos &&
                tags_lower.find(q) == std::string::npos &&
                tickers_lower.find(q) == std::string::npos) {
                continue;
            }
        }

        filtered_notes_.push_back(note);
    }

    // Fix selection
    if (selected_note_idx_ >= (int)filtered_notes_.size()) {
        selected_note_idx_ = -1;
        selected_note_ = nullptr;
        if (view_mode_ == ViewMode::Viewer) view_mode_ = ViewMode::List;
    }
}

// ============================================================================
// Toolbar
// ============================================================================
void NotesScreen::render_toolbar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, modern::BG_SURFACE);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, modern::ROUNDING_SM);
    ImGui::BeginChild("##notes_toolbar", ImVec2(0, 50), true);

    ImGui::SetCursorPos(ImVec2(12, 10));

    // New Note — accent button
    if (draw::AccentButton("+ New Note", ImVec2(120, 30))) {
        start_new_note();
    }

    ImGui::SameLine(0, 16);

    // Search — styled input
    ImGui::PushStyleColor(ImGuiCol_FrameBg, modern::BG_CARD);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, modern::ROUNDING_SM);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 7));
    ImGui::SetNextItemWidth(280);
    if (ImGui::InputTextWithHint("##search", "  Search notes...", search_buf_, sizeof(search_buf_))) {
        search_query_ = search_buf_;
        apply_filters();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 16);

    // Favorites toggle — ghost button
    if (draw::GhostButton(show_favorites_only_ ? "* Favorites" : "  Favorites",
                           ImVec2(100, 30), show_favorites_only_)) {
        show_favorites_only_ = !show_favorites_only_;
        apply_filters();
    }

    ImGui::SameLine(0, 6);

    // Archive toggle
    if (draw::GhostButton(show_archived_ ? "# Archived" : "  Archive",
                           ImVec2(95, 30), show_archived_)) {
        show_archived_ = !show_archived_;
        apply_filters();
    }

    ImGui::SameLine();
    float right_x = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 90;
    ImGui::SetCursorPosX(right_x);

    // Refresh — ghost
    if (draw::GhostButton("Refresh", ImVec2(80, 30))) {
        data_loaded_ = false;
        selected_note_idx_ = -1;
        selected_note_ = nullptr;
        view_mode_ = ViewMode::List;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Category Sidebar
// ============================================================================
void NotesScreen::render_category_sidebar() {
    // Section header with accent underline
    ImGui::PushStyleColor(ImGuiCol_Text, modern::ACCENT_BRIGHT);
    ImGui::Text("CATEGORIES");
    ImGui::PopStyleColor();

    // Accent underline
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(p.x, p.y - 2), ImVec2(p.x + 80, p.y),
        ImGui::ColorConvertFloat4ToU32(modern::ACCENT_BRIGHT), 1.0f);
    ImGui::Spacing();
    ImGui::Spacing();

    for (size_t i = 0; i < CATEGORIES.size(); i++) {
        auto& cat = CATEGORIES[i];
        bool is_selected = (selected_category_ == cat.id);

        // Count notes in this category
        int count = 0;
        for (auto& n : all_notes_) {
            if (n.is_archived && !show_archived_) continue;
            if (!n.is_archived && show_archived_) continue;
            if (show_favorites_only_ && !n.is_favorite) continue;
            if (std::string(cat.id) == "ALL" || n.category == cat.id) count++;
        }

        ImGui::PushID((int)i);

        // Selected: accent bg + left bar; unselected: transparent
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, modern::ACCENT_SOFT);
            ImGui::PushStyleColor(ImGuiCol_Text, modern::ACCENT_BRIGHT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, modern::TEXT_NORMAL);
        }
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, modern::BG_HOVER);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, modern::ROUNDING_XS);

        char label[128];
        std::snprintf(label, sizeof(label), " %s  %s", cat.icon, cat.label);

        float btn_h = 32.0f;
        if (ImGui::Button(label, ImVec2(-1, btn_h))) {
            selected_category_ = cat.id;
            apply_filters();
        }

        // Draw left accent bar on selected item
        if (is_selected) {
            ImVec2 btn_min = ImGui::GetItemRectMin();
            draw::AccentBar(ImGui::GetWindowDrawList(), btn_min,
                            btn_h, ImGui::ColorConvertFloat4ToU32(modern::ACCENT_BRIGHT));
        }

        // Count badge — right-aligned
        if (count > 0) {
            char cnt_str[16];
            std::snprintf(cnt_str, sizeof(cnt_str), "%d", count);
            ImVec2 btn_max = ImGui::GetItemRectMax();
            ImVec2 text_size = ImGui::CalcTextSize(cnt_str);
            float badge_x = btn_max.x - text_size.x - 10;
            float badge_y = ImGui::GetItemRectMin().y + (btn_h - text_size.y) * 0.5f;
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(badge_x, badge_y),
                ImGui::ColorConvertFloat4ToU32(modern::TEXT_MUTED), cnt_str);
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::PopID();

        ImGui::Spacing();
    }
}

// ============================================================================
// Notes List
// ============================================================================
void NotesScreen::render_notes_list() {
    if (loading_) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, modern::ACCENT_BRIGHT);
        theme::LoadingSpinner("Loading notes...");
        ImGui::PopStyleColor();
        return;
    }

    if (filtered_notes_.empty()) {
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, modern::TEXT_MUTED);
        ImGui::Text("  No notes found");
        ImGui::PopStyleColor();
        if (!search_query_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, modern::TEXT_FAINT);
            ImGui::Text("  Try adjusting your search");
            ImGui::PopStyleColor();
        }
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)filtered_notes_.size(); i++) {
        auto& note = filtered_notes_[i];
        bool is_selected = (i == selected_note_idx_);

        ImGui::PushID(i);

        // Card background — elevated on select, subtle otherwise
        ImVec4 card_bg = is_selected ? modern::BG_ELEVATED : modern::BG_CARD;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, card_bg);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, modern::ROUNDING_SM);

        ImVec4 note_color = hex_to_imvec4(note.color_code);
        float card_h = 95.0f;

        ImGui::BeginChild("##card", ImVec2(-1, card_h), true,
                          ImGuiWindowFlags_NoScrollbar);

        ImVec2 card_min = ImGui::GetWindowPos();
        ImVec2 card_max = {card_min.x + ImGui::GetWindowWidth(), card_min.y + card_h};

        // Left color accent bar
        draw::AccentBar(dl, card_min, card_h,
                        ImGui::ColorConvertFloat4ToU32(note_color), 3.0f, 1.5f);

        // Subtle glow on selected
        if (is_selected) {
            draw::SoftGlow(dl, card_min, card_max,
                           ImGui::ColorConvertFloat4ToU32(modern::ACCENT_GLOW), 4.0f, modern::ROUNDING_SM);
        }

        // Title row
        ImGui::SetCursorPosX(12);
        if (note.is_favorite) {
            ImGui::TextColored(modern::AMBER, "*");
            ImGui::SameLine();
        }
        ImGui::TextColored(is_selected ? modern::TEXT_BRIGHT : modern::TEXT_NORMAL,
                           "%s", note.title.c_str());

        // Priority & sentiment — pill-style badges
        ImGui::SetCursorPosX(12);
        ImVec4 pri_col = priority_color(note.priority);
        ImVec4 sent_col = sentiment_color(note.sentiment);
        ImGui::TextColored(pri_col, "%s", note.priority.c_str());
        ImGui::SameLine(0, 8);
        ImGui::TextColored(sent_col, "%s", note.sentiment.c_str());

        // Content preview
        if (!note.content.empty()) {
            ImGui::SetCursorPosX(12);
            std::string preview = note.content.substr(0, 75);
            if (note.content.size() > 75) preview += "...";
            ImGui::TextColored(modern::TEXT_MUTED, "%s", preview.c_str());
        }

        // Bottom row: date + word count + tickers
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(modern::TEXT_FAINT, "%s", note.updated_at.substr(0, 10).c_str());
        ImGui::SameLine();
        ImGui::TextColored(modern::TEXT_FAINT, " %lld words", (long long)note.word_count);
        if (!note.tickers.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(modern::ACCENT_BRIGHT, " %s", note.tickers.c_str());
        }

        // Detect click
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
            selected_note_idx_ = i;
            selected_note_ = &filtered_notes_[i];
            view_mode_ = ViewMode::Viewer;
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopID();

        ImGui::Spacing();
    }
}

// ============================================================================
// Note Editor
// ============================================================================
void NotesScreen::render_note_editor() {
    // Header
    ImGui::PushStyleColor(ImGuiCol_Text, modern::ACCENT_BRIGHT);
    ImGui::Text(is_new_note_ ? "CREATE NOTE" : "EDIT NOTE");
    ImGui::PopStyleColor();
    ImVec2 hp = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(hp.x, hp.y - 2), ImVec2(hp.x + 100, hp.y),
        ImGui::ColorConvertFloat4ToU32(modern::ACCENT_BRIGHT), 1.0f);
    ImGui::Spacing();
    ImGui::Spacing();

    // Styled input helper
    auto field_input = [](const char* label, const char* id, char* buf, size_t sz,
                          float label_w, const char* hint = nullptr) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(modern::TEXT_MUTED, "%s", label);
        ImGui::SameLine(label_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, modern::BG_CARD);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, modern::ROUNDING_XS);
        ImGui::SetNextItemWidth(-1);
        if (hint)
            ImGui::InputTextWithHint(id, hint, buf, sz);
        else
            ImGui::InputText(id, buf, sz);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    };

    float lw = 90.0f;

    field_input("Title", "##title", edit_title_, sizeof(edit_title_), lw);
    ImGui::Spacing();

    // Category combo
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(modern::TEXT_MUTED, "Category");
    ImGui::SameLine(lw);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, modern::BG_CARD);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, modern::ROUNDING_XS);
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo("##category", CATEGORIES[(size_t)edit_category_idx_ + 1].label)) {
        for (size_t i = 1; i < CATEGORIES.size(); i++) {
            bool sel = ((int)i - 1 == edit_category_idx_);
            if (ImGui::Selectable(CATEGORIES[i].label, sel))
                edit_category_idx_ = (int)i - 1;
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 16);

    // Priority combo
    ImGui::TextColored(modern::TEXT_MUTED, "Priority");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, modern::BG_CARD);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, modern::ROUNDING_XS);
    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("##priority", PRIORITIES[edit_priority_idx_])) {
        for (int i = 0; i < 3; i++) {
            bool sel = (i == edit_priority_idx_);
            if (ImGui::Selectable(PRIORITIES[i], sel))
                edit_priority_idx_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Sentiment combo
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(modern::TEXT_MUTED, "Sentiment");
    ImGui::SameLine(lw);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, modern::BG_CARD);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, modern::ROUNDING_XS);
    ImGui::SetNextItemWidth(150);
    if (ImGui::BeginCombo("##sentiment", SENTIMENTS[edit_sentiment_idx_])) {
        for (int i = 0; i < 3; i++) {
            bool sel = (i == edit_sentiment_idx_);
            if (ImGui::Selectable(SENTIMENTS[i], sel))
                edit_sentiment_idx_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Color swatches — circular with glow on selected
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(modern::TEXT_MUTED, "Color");
    ImGui::SameLine(lw);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < NUM_COLORS; i++) {
        ImVec4 col = hex_to_imvec4(COLOR_CODES[i]);
        bool sel = (i == edit_color_idx_);

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float radius = 11.0f;
        ImVec2 center = {cursor.x + radius, cursor.y + radius};

        // Glow ring on selected
        if (sel) {
            dl->AddCircleFilled(center, radius + 3.0f,
                ImGui::ColorConvertFloat4ToU32(ImVec4(col.x, col.y, col.z, 0.35f)));
        }
        dl->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(col));
        if (sel) {
            dl->AddCircle(center, radius + 1.0f,
                ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.8f)), 0, 1.5f);
        }

        // Invisible button for interaction
        char cid[16];
        std::snprintf(cid, sizeof(cid), "##col%d", i);
        if (ImGui::InvisibleButton(cid, ImVec2(radius * 2 + 4, radius * 2 + 2))) {
            edit_color_idx_ = i;
        }
        if (i < NUM_COLORS - 1) ImGui::SameLine(0, 6);
    }

    ImGui::Spacing();

    field_input("Tags", "##tags", edit_tags_, sizeof(edit_tags_), lw, "comma-separated tags");
    field_input("Tickers", "##tickers", edit_tickers_, sizeof(edit_tickers_), lw, "e.g. AAPL, MSFT");

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(modern::TEXT_MUTED, "Reminder");
    ImGui::SameLine(lw);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, modern::BG_CARD);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, modern::ROUNDING_XS);
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##reminder", "YYYY-MM-DD", edit_reminder_, sizeof(edit_reminder_));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Thin separator
    ImVec2 sp = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(
        sp, ImVec2(sp.x + ImGui::GetContentRegionAvail().x, sp.y),
        ImGui::ColorConvertFloat4ToU32(modern::BORDER_SUBTLE));
    ImGui::Spacing();
    ImGui::Spacing();

    // Content area
    ImGui::TextColored(modern::TEXT_MUTED, "Content");
    ImGui::Spacing();
    float content_h = ImGui::GetContentRegionAvail().y - 50;
    if (content_h < 100) content_h = 100;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, modern::BG_CARD);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, modern::ROUNDING_XS);
    ImGui::InputTextMultiline("##content", edit_content_, sizeof(edit_content_),
                              ImVec2(-1, content_h));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Action buttons
    ImGui::Spacing();
    if (draw::AccentButton("Save", ImVec2(110, 32))) {
        save_note();
    }
    ImGui::SameLine(0, 10);
    if (draw::GhostButton("Cancel", ImVec2(110, 32))) {
        view_mode_ = ViewMode::List;
    }
}

// ============================================================================
// Note Viewer
// ============================================================================
void NotesScreen::render_note_viewer() {
    if (!selected_note_) return;
    auto& note = *selected_note_;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Title with color accent
    ImVec4 note_color = hex_to_imvec4(note.color_code);

    // Color dot before title
    ImVec2 tp = ImGui::GetCursorScreenPos();
    dl->AddCircleFilled(ImVec2(tp.x + 6, tp.y + 8), 5.0f,
                        ImGui::ColorConvertFloat4ToU32(note_color));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 18);

    ImGui::PushStyleColor(ImGuiCol_Text, modern::TEXT_BRIGHT);
    ImGui::TextWrapped("%s", note.title.c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Thin accent line
    ImVec2 lp = ImGui::GetCursorScreenPos();
    dl->AddLine(lp, ImVec2(lp.x + ImGui::GetContentRegionAvail().x, lp.y),
                ImGui::ColorConvertFloat4ToU32(modern::BORDER_SUBTLE));
    ImGui::Spacing();
    ImGui::Spacing();

    // Metadata grid — label:value pairs with refined colors
    auto meta_row = [](const char* label, const char* value, ImVec4 color = modern::TEXT_NORMAL) {
        ImGui::TextColored(modern::TEXT_MUTED, "%-12s", label);
        ImGui::SameLine(110);
        ImGui::TextColored(color, "%s", value);
    };

    meta_row("Category", note.category.c_str());
    meta_row("Priority", note.priority.c_str(), priority_color(note.priority));
    meta_row("Sentiment", note.sentiment.c_str(), sentiment_color(note.sentiment));
    {
        char wc[32];
        std::snprintf(wc, sizeof(wc), "%lld", (long long)note.word_count);
        meta_row("Words", wc);
    }
    if (!note.tickers.empty()) meta_row("Tickers", note.tickers.c_str(), modern::ACCENT_BRIGHT);
    if (!note.tags.empty()) meta_row("Tags", note.tags.c_str());
    meta_row("Created", note.created_at.c_str());
    meta_row("Updated", note.updated_at.c_str());
    if (note.reminder_date) {
        meta_row("Reminder", note.reminder_date->c_str(), modern::AMBER);
    }

    ImGui::Spacing();

    // Separator
    ImVec2 sp = ImGui::GetCursorScreenPos();
    dl->AddLine(sp, ImVec2(sp.x + ImGui::GetContentRegionAvail().x, sp.y),
                ImGui::ColorConvertFloat4ToU32(modern::BORDER_SUBTLE));
    ImGui::Spacing();
    ImGui::Spacing();

    // Action buttons — modern styled
    if (draw::GhostButton(note.is_favorite ? "* Unfavorite" : "  Favorite",
                           ImVec2(100, 30), note.is_favorite)) {
        db::ops::toggle_note_favorite(note.id);
        data_loaded_ = false;
    }
    ImGui::SameLine(0, 6);
    if (draw::GhostButton("Edit", ImVec2(70, 30))) {
        start_editing(note);
    }
    ImGui::SameLine(0, 6);
    if (draw::GhostButton(note.is_archived ? "Unarchive" : "Archive", ImVec2(95, 30))) {
        db::ops::archive_note(note.id);
        data_loaded_ = false;
    }
    ImGui::SameLine(0, 6);
    if (draw::DangerButton("Delete", ImVec2(75, 30))) {
        confirm_delete(note.id);
    }

    ImGui::Spacing();

    // Separator
    ImVec2 sp2 = ImGui::GetCursorScreenPos();
    dl->AddLine(sp2, ImVec2(sp2.x + ImGui::GetContentRegionAvail().x, sp2.y),
                ImGui::ColorConvertFloat4ToU32(modern::BORDER_SUBTLE));
    ImGui::Spacing();
    ImGui::Spacing();

    // Content display
    ImGui::BeginChild("##note_content_view", ImVec2(0, 0), false);
    ImGui::PushStyleColor(ImGuiCol_Text, modern::TEXT_NORMAL);
    ImGui::TextWrapped("%s", note.content.c_str());
    ImGui::PopStyleColor();
    ImGui::EndChild();
}

// ============================================================================
// Status Bar
// ============================================================================
void NotesScreen::render_status_bar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, modern::BG_BASE);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, modern::ROUNDING_XS);
    ImGui::BeginChild("##notes_status", ImVec2(0, 28), true);

    ImGui::SetCursorPos(ImVec2(12, 5));

    int total = (int)all_notes_.size();
    int showing = (int)filtered_notes_.size();
    int favorites = 0;
    int archived = 0;
    for (auto& n : all_notes_) {
        if (n.is_favorite) favorites++;
        if (n.is_archived) archived++;
    }

    // Status segments with accent highlights
    ImGui::TextColored(modern::TEXT_FAINT, "Total");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(modern::TEXT_NORMAL, "%d", total);
    ImGui::SameLine(0, 16);

    ImGui::TextColored(modern::TEXT_FAINT, "Showing");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(modern::ACCENT_BRIGHT, "%d", showing);
    ImGui::SameLine(0, 16);

    ImGui::TextColored(modern::TEXT_FAINT, "Favorites");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(modern::AMBER, "%d", favorites);
    ImGui::SameLine(0, 16);

    ImGui::TextColored(modern::TEXT_FAINT, "Archived");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(modern::TEXT_MUTED, "%d", archived);
    ImGui::SameLine(0, 16);

    ImGui::TextColored(modern::TEXT_FAINT, "Category:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(modern::ACCENT_BRIGHT, "%s", selected_category_.c_str());

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Delete Confirmation Popup
// ============================================================================
void NotesScreen::render_delete_confirm_popup() {
    if (!show_delete_confirm_) return;

    ImGui::OpenPopup("Delete Note?");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360, 150));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, modern::BG_ELEVATED);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, modern::ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));

    if (ImGui::BeginPopupModal("Delete Note?", &show_delete_confirm_,
                                ImGuiWindowFlags_NoResize)) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, modern::TEXT_NORMAL);
        ImGui::TextWrapped("Are you sure you want to delete this note? This action cannot be undone.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Spacing();

        if (draw::DangerButton("Delete", ImVec2(130, 32))) {
            do_delete();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 10);
        if (draw::GhostButton("Cancel", ImVec2(130, 32))) {
            show_delete_confirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// ============================================================================
// Editor helpers
// ============================================================================
void NotesScreen::start_new_note() {
    is_new_note_ = true;
    editing_note_ = {};
    editing_note_.category = "GENERAL";
    editing_note_.priority = "MEDIUM";
    editing_note_.sentiment = "NEUTRAL";
    editing_note_.color_code = COLOR_CODES[0];

    std::memset(edit_title_, 0, sizeof(edit_title_));
    std::memset(edit_content_, 0, sizeof(edit_content_));
    std::memset(edit_tags_, 0, sizeof(edit_tags_));
    std::memset(edit_tickers_, 0, sizeof(edit_tickers_));
    std::memset(edit_reminder_, 0, sizeof(edit_reminder_));
    edit_category_idx_ = 6;  // GENERAL (index in CATEGORIES minus 1 for ALL)
    edit_priority_idx_ = 1;  // MEDIUM
    edit_sentiment_idx_ = 2; // NEUTRAL
    edit_color_idx_ = 0;

    view_mode_ = ViewMode::Editor;
}

void NotesScreen::start_editing(db::Note& note) {
    is_new_note_ = false;
    editing_note_ = note;

    std::strncpy(edit_title_, note.title.c_str(), sizeof(edit_title_) - 1);
    std::strncpy(edit_content_, note.content.c_str(), sizeof(edit_content_) - 1);
    std::strncpy(edit_tags_, note.tags.c_str(), sizeof(edit_tags_) - 1);
    std::strncpy(edit_tickers_, note.tickers.c_str(), sizeof(edit_tickers_) - 1);
    if (note.reminder_date) {
        std::strncpy(edit_reminder_, note.reminder_date->c_str(), sizeof(edit_reminder_) - 1);
    } else {
        edit_reminder_[0] = '\0';
    }

    edit_category_idx_ = category_index(note.category);
    edit_priority_idx_ = (note.priority == "HIGH") ? 0 : (note.priority == "MEDIUM") ? 1 : 2;
    edit_sentiment_idx_ = (note.sentiment == "BULLISH") ? 0 : (note.sentiment == "BEARISH") ? 1 : 2;

    // Find color index
    edit_color_idx_ = 0;
    for (int i = 0; i < NUM_COLORS; i++) {
        if (note.color_code == COLOR_CODES[i]) { edit_color_idx_ = i; break; }
    }

    view_mode_ = ViewMode::Editor;
}

void NotesScreen::save_note() {
    editing_note_.title = edit_title_;
    editing_note_.content = edit_content_;
    editing_note_.tags = edit_tags_;
    editing_note_.tickers = edit_tickers_;
    editing_note_.category = CATEGORIES[(size_t)edit_category_idx_ + 1].id;
    editing_note_.priority = PRIORITIES[edit_priority_idx_];
    editing_note_.sentiment = SENTIMENTS[edit_sentiment_idx_];
    editing_note_.color_code = COLOR_CODES[edit_color_idx_];

    std::string rem = edit_reminder_;
    if (!rem.empty()) {
        editing_note_.reminder_date = rem;
    } else {
        editing_note_.reminder_date = std::nullopt;
    }

    try {
        if (is_new_note_) {
            db::ops::create_note(editing_note_);
        } else {
            db::ops::update_note(editing_note_);
        }
    } catch (...) {
        // silently fail — could add error display
    }

    view_mode_ = ViewMode::List;
    data_loaded_ = false;
    selected_note_idx_ = -1;
    selected_note_ = nullptr;
}

void NotesScreen::confirm_delete(int64_t id) {
    delete_target_id_ = id;
    show_delete_confirm_ = true;
}

void NotesScreen::do_delete() {
    try {
        db::ops::delete_note(delete_target_id_);
    } catch (...) {}
    show_delete_confirm_ = false;
    data_loaded_ = false;
    selected_note_idx_ = -1;
    selected_note_ = nullptr;
    view_mode_ = ViewMode::List;
}

int NotesScreen::category_index(const std::string& cat) const {
    for (size_t i = 1; i < CATEGORIES.size(); i++) {
        if (cat == CATEGORIES[i].id) return (int)i - 1;
    }
    return 6; // GENERAL
}

} // namespace fincept::notes
