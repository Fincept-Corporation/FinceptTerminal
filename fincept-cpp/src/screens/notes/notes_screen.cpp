// Notes Screen — Full CRUD implementation
// Mirrors Tauri NotesTab: category sidebar, notes list, editor, viewer

#include "notes_screen.h"
#include "notes_types.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace fincept::notes {

using namespace theme::colors;

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

    // Use ScreenFrame to render inline in the main content area (not a separate window)
    ui::ScreenFrame frame("##notes_screen", ImVec2(8, 8), theme::colors::BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    // Toolbar at top
    render_toolbar();

    float toolbar_h = ImGui::GetCursorPosY();
    float status_h = 28.0f;
    float body_h = frame.height() - toolbar_h - status_h;
    if (body_h < 100) body_h = 100;

    // Body: sidebar + list + editor/viewer
    ImGui::BeginChild("##notes_body", ImVec2(0, body_h), false);
    {
        float sidebar_w = 200.0f;
        float list_w = 300.0f;

        // Category sidebar
        ImGui::BeginChild("##cat_sidebar", ImVec2(sidebar_w, 0), true);
        render_category_sidebar();
        ImGui::EndChild();

        ImGui::SameLine();

        // Notes list
        ImGui::BeginChild("##notes_list", ImVec2(list_w, 0), true);
        render_notes_list();
        ImGui::EndChild();

        ImGui::SameLine();

        // Right panel: editor or viewer
        ImGui::BeginChild("##note_detail", ImVec2(0, 0), true);
        if (view_mode_ == ViewMode::Editor) {
            render_note_editor();
        } else if (view_mode_ == ViewMode::Viewer && selected_note_) {
            render_note_viewer();
        } else {
            // Empty state
            auto region = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPos(ImVec2(region.x * 0.5f - 100, region.y * 0.5f - 20));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
            ImGui::TextWrapped("Select a note to view or click 'New Note' to create one");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    // Status bar
    render_status_bar();

    frame.end();

    // Delete confirmation popup (rendered outside frame so it overlays properly)
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
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##notes_toolbar", ImVec2(0, 44), true);

    ImGui::SetCursorPos(ImVec2(8, 8));

    // New Note button
    if (theme::AccentButton("+ New Note", ImVec2(110, 28))) {
        start_new_note();
    }

    ImGui::SameLine(0, 16);

    // Search
    ImGui::SetNextItemWidth(250);
    if (ImGui::InputTextWithHint("##search", "Search notes...", search_buf_, sizeof(search_buf_))) {
        search_query_ = search_buf_;
        apply_filters();
    }

    ImGui::SameLine(0, 16);

    // Favorites toggle
    ImGui::PushStyleColor(ImGuiCol_Button, show_favorites_only_ ? ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.3f) : ImVec4(0,0,0,0));
    if (ImGui::Button(show_favorites_only_ ? "* Favorites" : "  Favorites", ImVec2(100, 28))) {
        show_favorites_only_ = !show_favorites_only_;
        apply_filters();
    }
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 8);

    // Archive toggle
    ImGui::PushStyleColor(ImGuiCol_Button, show_archived_ ? ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.3f) : ImVec4(0,0,0,0));
    if (ImGui::Button(show_archived_ ? "# Archived" : "  Archive", ImVec2(90, 28))) {
        show_archived_ = !show_archived_;
        apply_filters();
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    float right_x = ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 80;
    ImGui::SetCursorPosX(right_x);

    // Refresh
    if (ImGui::Button("Refresh", ImVec2(72, 28))) {
        data_loaded_ = false;
        selected_note_idx_ = -1;
        selected_note_ = nullptr;
        view_mode_ = ViewMode::List;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Category Sidebar
// ============================================================================
void NotesScreen::render_category_sidebar() {
    theme::SectionHeader("Categories");
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

        // Color the selected item
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        char label[128];
        std::snprintf(label, sizeof(label), "%s %s  (%d)##cat_%zu", cat.icon, cat.label, count, i);
        if (ImGui::Button(label, ImVec2(-1, 30))) {
            selected_category_ = cat.id;
            apply_filters();
        }

        ImGui::PopStyleColor(2);
    }
}

// ============================================================================
// Notes List
// ============================================================================
void NotesScreen::render_notes_list() {
    if (loading_) {
        ImGui::TextColored(TEXT_DIM, "Loading notes...");
        return;
    }

    if (filtered_notes_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "No notes found");
        if (!search_query_.empty()) {
            ImGui::TextColored(TEXT_DIM, "Try adjusting your search");
        }
        return;
    }

    for (int i = 0; i < (int)filtered_notes_.size(); i++) {
        auto& note = filtered_notes_[i];
        bool is_selected = (i == selected_note_idx_);

        ImGui::PushID(i);

        // Card background
        ImVec4 card_bg = is_selected ? ACCENT_BG : BG_WIDGET;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, card_bg);

        // Color-coded left border via a small colored rect
        ImVec4 note_color = hex_to_imvec4(note.color_code);

        float card_h = 90.0f;
        ImGui::BeginChild("##card", ImVec2(-1, card_h), true,
                          ImGuiWindowFlags_NoScrollbar);

        // Left color bar
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(p.x - 4, p.y - 4),
            ImVec2(p.x - 1, p.y + card_h - 8),
            ImGui::ColorConvertFloat4ToU32(note_color),
            2.0f);

        // Title row with favorite star
        ImGui::SetCursorPosX(8);
        if (note.is_favorite) {
            ImGui::TextColored(ImVec4(1, 0.85f, 0, 1), "*");
            ImGui::SameLine();
        }
        ImGui::TextColored(TEXT_PRIMARY, "%s", note.title.c_str());

        // Priority & sentiment badges
        ImGui::SetCursorPosX(8);
        ImGui::TextColored(priority_color(note.priority), "[%s]", note.priority.c_str());
        ImGui::SameLine();
        ImGui::TextColored(sentiment_color(note.sentiment), "[%s]", note.sentiment.c_str());

        // Content preview (max ~80 chars)
        if (!note.content.empty()) {
            ImGui::SetCursorPosX(8);
            std::string preview = note.content.substr(0, 80);
            if (note.content.size() > 80) preview += "...";
            ImGui::TextColored(TEXT_DIM, "%s", preview.c_str());
        }

        // Bottom row: date + word count + tickers
        ImGui::SetCursorPosX(8);
        ImGui::TextColored(TEXT_DIM, "%s", note.updated_at.substr(0, 10).c_str());
        ImGui::SameLine();
        ImGui::TextColored(TEXT_DIM, "| %lld words", (long long)note.word_count);
        if (!note.tickers.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ACCENT, "| %s", note.tickers.c_str());
        }

        // Detect click on the card
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
            selected_note_idx_ = i;
            selected_note_ = &filtered_notes_[i];
            view_mode_ = ViewMode::Viewer;
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopID();

        ImGui::Spacing();
    }
}

// ============================================================================
// Note Editor
// ============================================================================
void NotesScreen::render_note_editor() {
    theme::SectionHeader(is_new_note_ ? "Create Note" : "Edit Note");
    ImGui::Spacing();

    float label_w = 90.0f;

    // Title
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(TEXT_SECONDARY, "Title");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##title", edit_title_, sizeof(edit_title_));

    ImGui::Spacing();

    // Category
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(TEXT_SECONDARY, "Category");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo("##category", CATEGORIES[(size_t)edit_category_idx_ + 1].label)) {
        for (size_t i = 1; i < CATEGORIES.size(); i++) {  // skip "ALL"
            bool sel = ((int)i - 1 == edit_category_idx_);
            if (ImGui::Selectable(CATEGORIES[i].label, sel)) {
                edit_category_idx_ = (int)i - 1;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0, 16);

    // Priority
    ImGui::TextColored(TEXT_SECONDARY, "Priority");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("##priority", PRIORITIES[edit_priority_idx_])) {
        for (int i = 0; i < 3; i++) {
            bool sel = (i == edit_priority_idx_);
            if (ImGui::Selectable(PRIORITIES[i], sel)) {
                edit_priority_idx_ = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // Sentiment
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(TEXT_SECONDARY, "Sentiment");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(150);
    if (ImGui::BeginCombo("##sentiment", SENTIMENTS[edit_sentiment_idx_])) {
        for (int i = 0; i < 3; i++) {
            bool sel = (i == edit_sentiment_idx_);
            if (ImGui::Selectable(SENTIMENTS[i], sel)) {
                edit_sentiment_idx_ = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // Color swatches
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(TEXT_SECONDARY, "Color");
    ImGui::SameLine(label_w);
    for (int i = 0; i < NUM_COLORS; i++) {
        ImVec4 col = hex_to_imvec4(COLOR_CODES[i]);
        bool sel = (i == edit_color_idx_);
        if (sel) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 1));
        }
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x * 1.2f, col.y * 1.2f, col.z * 1.2f, 1));
        char cid[16];
        std::snprintf(cid, sizeof(cid), "##col%d", i);
        if (ImGui::Button(cid, ImVec2(24, 24))) {
            edit_color_idx_ = i;
        }
        ImGui::PopStyleColor(2);
        if (sel) {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
        if (i < NUM_COLORS - 1) ImGui::SameLine(0, 4);
    }

    ImGui::Spacing();

    // Tags
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(TEXT_SECONDARY, "Tags");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##tags", "comma-separated tags", edit_tags_, sizeof(edit_tags_));

    // Tickers
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(TEXT_SECONDARY, "Tickers");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##tickers", "e.g. AAPL, MSFT, GOOG", edit_tickers_, sizeof(edit_tickers_));

    // Reminder
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(TEXT_SECONDARY, "Reminder");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##reminder", "YYYY-MM-DD", edit_reminder_, sizeof(edit_reminder_));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Content
    ImGui::TextColored(TEXT_SECONDARY, "Content");
    ImGui::Spacing();
    float content_h = ImGui::GetContentRegionAvail().y - 44;
    if (content_h < 100) content_h = 100;
    ImGui::InputTextMultiline("##content", edit_content_, sizeof(edit_content_),
                              ImVec2(-1, content_h));

    // Buttons
    ImGui::Spacing();
    if (theme::AccentButton("Save", ImVec2(100, 30))) {
        save_note();
    }
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Cancel", ImVec2(100, 30))) {
        view_mode_ = ViewMode::List;
    }
}

// ============================================================================
// Note Viewer
// ============================================================================
void NotesScreen::render_note_viewer() {
    if (!selected_note_) return;
    auto& note = *selected_note_;

    // Title with color accent
    ImVec4 note_color = hex_to_imvec4(note.color_code);
    ImGui::PushStyleColor(ImGuiCol_Text, note_color);
    ImGui::TextWrapped("%s", note.title.c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Metadata grid
    auto meta_row = [](const char* label, const char* value, ImVec4 color = TEXT_PRIMARY) {
        ImGui::TextColored(TEXT_DIM, "%-12s", label);
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
    if (!note.tickers.empty()) meta_row("Tickers", note.tickers.c_str(), ACCENT);
    if (!note.tags.empty()) meta_row("Tags", note.tags.c_str());
    meta_row("Created", note.created_at.c_str());
    meta_row("Updated", note.updated_at.c_str());
    if (note.reminder_date) {
        meta_row("Reminder", note.reminder_date->c_str(), WARNING);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Action buttons
    // Favorite
    if (ImGui::Button(note.is_favorite ? "* Unfavorite" : "  Favorite", ImVec2(100, 28))) {
        db::ops::toggle_note_favorite(note.id);
        data_loaded_ = false; // reload
    }
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Edit", ImVec2(70, 28))) {
        start_editing(note);
    }
    ImGui::SameLine(0, 8);
    if (ImGui::Button(note.is_archived ? "Unarchive" : "Archive", ImVec2(90, 28))) {
        db::ops::archive_note(note.id);
        data_loaded_ = false;
    }
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1));
    if (ImGui::Button("Delete", ImVec2(70, 28))) {
        confirm_delete(note.id);
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Content display
    ImGui::BeginChild("##note_content_view", ImVec2(0, 0), false);
    ImGui::TextWrapped("%s", note.content.c_str());
    ImGui::EndChild();
}

// ============================================================================
// Status Bar
// ============================================================================
void NotesScreen::render_status_bar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##notes_status", ImVec2(0, 26), true);

    ImGui::SetCursorPos(ImVec2(8, 4));

    int total = (int)all_notes_.size();
    int showing = (int)filtered_notes_.size();
    int favorites = 0;
    int archived = 0;
    for (auto& n : all_notes_) {
        if (n.is_favorite) favorites++;
        if (n.is_archived) archived++;
    }

    ImGui::TextColored(TEXT_DIM,
        "Total: %d | Showing: %d | Favorites: %d | Archived: %d | Category: %s",
        total, showing, favorites, archived, selected_category_.c_str());

    ImGui::EndChild();
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
    ImGui::SetNextWindowSize(ImVec2(340, 130));

    if (ImGui::BeginPopupModal("Delete Note?", &show_delete_confirm_,
                                ImGuiWindowFlags_NoResize)) {
        ImGui::Spacing();
        ImGui::TextWrapped("Are you sure you want to delete this note? This action cannot be undone.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1));
        if (ImGui::Button("Delete", ImVec2(120, 30))) {
            do_delete();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 8);
        if (ImGui::Button("Cancel", ImVec2(120, 30))) {
            show_delete_confirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
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
