#pragma once
// Notes Screen — Full CRUD note-taking system with categories, priorities,
// sentiments, search/filter, editor, viewer, and SQLite persistence.
// Mirrors the Tauri NotesTab component.

#include "storage/database.h"
#include <string>
#include <vector>
#include <future>

namespace fincept::notes {

enum class ViewMode { List, Editor, Viewer };

class NotesScreen {
public:
    void render();

private:
    // Data
    std::vector<db::Note> all_notes_;
    std::vector<db::Note> filtered_notes_;
    bool data_loaded_ = false;
    bool loading_ = false;
    std::future<void> load_future_;

    // Selection
    int selected_note_idx_ = -1;
    db::Note* selected_note_ = nullptr;

    // View
    ViewMode view_mode_ = ViewMode::List;

    // Filters
    std::string selected_category_ = "ALL";
    std::string search_query_;
    char search_buf_[256] = {};
    bool show_archived_ = false;
    bool show_favorites_only_ = false;

    // Editor state
    db::Note editing_note_;
    bool is_new_note_ = false;
    char edit_title_[256] = {};
    char edit_content_[8192] = {};
    char edit_tags_[512] = {};
    char edit_tickers_[256] = {};
    char edit_reminder_[64] = {};
    int edit_category_idx_ = 7;  // GENERAL
    int edit_priority_idx_ = 1;  // MEDIUM
    int edit_sentiment_idx_ = 2; // NEUTRAL
    int edit_color_idx_ = 0;     // Orange

    // Delete confirmation
    bool show_delete_confirm_ = false;
    int64_t delete_target_id_ = 0;

    // Methods
    void load_notes();
    void apply_filters();

    void render_toolbar();
    void render_category_sidebar();
    void render_notes_list();
    void render_note_editor();
    void render_note_viewer();
    void render_status_bar();
    void render_delete_confirm_popup();

    void start_new_note();
    void start_editing(db::Note& note);
    void save_note();
    void confirm_delete(int64_t id);
    void do_delete();

    int category_index(const std::string& cat) const;
};

} // namespace fincept::notes
