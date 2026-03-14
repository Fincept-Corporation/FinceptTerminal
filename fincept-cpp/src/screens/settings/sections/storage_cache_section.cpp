#include "storage_cache_section.h"
#include "storage/database.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>

namespace fincept::settings {

void StorageCacheSection::init() {
    if (initialized_) return;
    refresh_cache_stats();
    initialized_ = true;
}

void StorageCacheSection::refresh_cache_stats() {
    auto stats = db::ops::cache_stats();
    cache_entries_ = stats.total_entries;
    cache_size_ = stats.total_size_bytes;
    cache_expired_ = stats.expired_entries;
}

void StorageCacheSection::load_table_list() {
    table_names_.clear();
    try {
        auto& database = db::Database::instance();
        std::lock_guard<std::recursive_mutex> lock(database.mutex());
        auto stmt = database.prepare(
            "SELECT name FROM sqlite_master WHERE type='table' "
            "AND name NOT LIKE 'sqlite_%' ORDER BY name");
        while (stmt.step()) {
            table_names_.push_back(stmt.col_text(0));
        }
    } catch (...) {
        table_names_.clear();
    }
}

void StorageCacheSection::load_table_data(const std::string& table_name, int offset) {
    column_names_.clear();
    table_rows_.clear();
    current_page_ = offset / ROWS_PER_PAGE;

    try {
        auto& database = db::Database::instance();
        std::lock_guard<std::recursive_mutex> lock(database.mutex());

        // Get column names via PRAGMA
        std::string pragma = "PRAGMA table_info(" + table_name + ")";
        auto info_stmt = database.prepare(pragma.c_str());
        while (info_stmt.step()) {
            column_names_.push_back(info_stmt.col_text(1)); // column name
        }

        // Get rows
        char sql[512];
        std::snprintf(sql, sizeof(sql),
            "SELECT * FROM \"%s\" LIMIT %d OFFSET %d",
            table_name.c_str(), ROWS_PER_PAGE, offset);

        auto data_stmt = database.prepare(sql);
        int n_cols = static_cast<int>(column_names_.size());
        while (data_stmt.step()) {
            std::vector<std::string> row;
            row.reserve(n_cols);
            for (int i = 0; i < n_cols; i++) {
                row.push_back(data_stmt.col_text(i));
            }
            table_rows_.push_back(std::move(row));
        }
    } catch (...) {
        column_names_.clear();
        table_rows_.clear();
    }
}

void StorageCacheSection::execute_query() {
    query_result_.clear();
    query_error_ = false;

    std::string sql(query_input_);
    if (sql.empty()) {
        query_result_ = "Empty query";
        query_error_ = true;
        return;
    }

    try {
        auto& database = db::Database::instance();
        std::lock_guard<std::recursive_mutex> lock(database.mutex());

        // Check if it's a SELECT query
        std::string upper_sql = sql;
        for (auto& c : upper_sql) c = static_cast<char>(std::toupper(c));

        if (upper_sql.find("SELECT") == 0 || upper_sql.find("PRAGMA") == 0) {
            auto stmt = database.prepare(sql.c_str());
            std::string result;
            int row_count = 0;
            while (stmt.step() && row_count < 100) {
                // Get column count from first row
                int n_cols = stmt.col_count();
                for (int i = 0; i < n_cols; i++) {
                    if (i > 0) result += " | ";
                    result += stmt.col_text(i);
                }
                result += "\n";
                row_count++;
            }
            if (result.empty()) result = "(no results)";
            query_result_ = result;
        } else {
            database.exec(sql.c_str());
            query_result_ = "Query executed successfully";
        }
    } catch (const std::exception& e) {
        query_result_ = std::string("Error: ") + e.what();
        query_error_ = true;
    }
}

// ============================================================================
// Cache panel
// ============================================================================
void StorageCacheSection::render_cache_panel() {
    using namespace theme::colors;

    theme::SectionHeader("CACHE MANAGEMENT");

    // Stats
    if (ImGui::BeginTable("##cache_stats", 3, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Total Entries");
        ImGui::TableSetupColumn("Size (bytes)");
        ImGui::TableSetupColumn("Expired");
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TEXT_PRIMARY, "%lld", (long long)cache_entries_);
        ImGui::TableNextColumn();
        ImGui::TextColored(TEXT_PRIMARY, "%lld", (long long)cache_size_);
        ImGui::TableNextColumn();
        ImGui::TextColored(cache_expired_ > 0 ? WARNING : SUCCESS,
            "%lld", (long long)cache_expired_);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    if (theme::AccentButton("Refresh Stats")) {
        refresh_cache_stats();
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ERROR_RED);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
    if (ImGui::Button("Clear All Cache")) {
        db::ops::cache_clear_all();
        refresh_cache_stats();
        status_ = "Cache cleared";
        status_time_ = ImGui::GetTime();
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (theme::SecondaryButton("Cleanup Expired")) {
        int64_t removed = db::ops::cache_cleanup();
        refresh_cache_stats();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Removed %lld expired entries", (long long)removed);
        status_ = buf;
        status_time_ = ImGui::GetTime();
    }
}

// ============================================================================
// Database browser
// ============================================================================
void StorageCacheSection::render_db_login() {
    using namespace theme::colors;

    ImGui::TextColored(TEXT_DIM, "Enter admin password to access database browser.");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(300);
    ImGui::InputText("##db_pass", db_password_, sizeof(db_password_),
                     ImGuiInputTextFlags_Password);
    ImGui::SameLine();

    if (theme::AccentButton("Login")) {
        // Simple password check via settings
        auto stored = db::ops::get_setting("admin_password");
        if (!stored) {
            // No password set — first time, set it
            setting_password_ = true;
        } else if (*stored == std::string(db_password_)) {
            db_authenticated_ = true;
            load_table_list();
        }
        std::memset(db_password_, 0, sizeof(db_password_));
    }

    if (setting_password_) {
        ImGui::Spacing();
        ImGui::TextColored(WARNING, "No admin password set. Create one:");
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##db_new_pass", db_new_password_, sizeof(db_new_password_),
                         ImGuiInputTextFlags_Password);
        ImGui::SameLine();
        if (theme::AccentButton("Set Password")) {
            if (std::strlen(db_new_password_) >= 4) {
                db::ops::save_setting("admin_password",
                    std::string(db_new_password_), "admin");
                db_authenticated_ = true;
                setting_password_ = false;
                load_table_list();
            }
            std::memset(db_new_password_, 0, sizeof(db_new_password_));
        }
    }
}

void StorageCacheSection::render_table_browser() {
    using namespace theme::colors;

    float avail_w = ImGui::GetContentRegionAvail().x;
    float list_w = 180.0f;
    float content_w = avail_w - list_w - 8;

    // Table list
    ImGui::BeginChild("##table_list", ImVec2(list_w, 300), ImGuiChildFlags_Borders);
    ImGui::TextColored(ACCENT, "TABLES");
    ImGui::Separator();

    for (int i = 0; i < (int)table_names_.size(); i++) {
        if (ImGui::Selectable(table_names_[i].c_str(), i == selected_table_)) {
            selected_table_ = i;
            load_table_data(table_names_[i]);
        }
    }
    ImGui::EndChild();
    ImGui::SameLine(0, 8);

    // Table data
    ImGui::BeginChild("##table_data", ImVec2(content_w, 300), ImGuiChildFlags_Borders);

    if (selected_table_ >= 0 && selected_table_ < (int)table_names_.size()) {
        char header[128];
        std::snprintf(header, sizeof(header), "%s (%d rows)",
            table_names_[selected_table_].c_str(), (int)table_rows_.size());
        ImGui::TextColored(ACCENT, "%s", header);
        ImGui::Separator();

        if (!column_names_.empty()) {
            int n_cols = static_cast<int>(column_names_.size());
            if (ImGui::BeginTable("##data_table", n_cols,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX |
                    ImGuiTableFlags_ScrollY, ImVec2(0, 220))) {

                for (auto& col : column_names_) {
                    ImGui::TableSetupColumn(col.c_str());
                }
                ImGui::TableHeadersRow();

                for (auto& row : table_rows_) {
                    ImGui::TableNextRow();
                    for (int c = 0; c < n_cols && c < (int)row.size(); c++) {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(row[c].c_str());
                    }
                }
                ImGui::EndTable();
            }

            // Pagination
            ImGui::Spacing();
            if (current_page_ > 0) {
                if (theme::SecondaryButton("<< Prev")) {
                    load_table_data(table_names_[selected_table_],
                        (current_page_ - 1) * ROWS_PER_PAGE);
                }
                ImGui::SameLine();
            }
            char page_label[32];
            std::snprintf(page_label, sizeof(page_label), "Page %d", current_page_ + 1);
            ImGui::TextColored(TEXT_DIM, "%s", page_label);
            ImGui::SameLine();
            if ((int)table_rows_.size() == ROWS_PER_PAGE) {
                if (theme::SecondaryButton("Next >>")) {
                    load_table_data(table_names_[selected_table_],
                        (current_page_ + 1) * ROWS_PER_PAGE);
                }
            }
        }
    } else {
        ImGui::TextColored(TEXT_DIM, "Select a table from the list.");
    }

    ImGui::EndChild();

    // Custom query
    ImGui::Spacing();
    theme::SectionHeader("CUSTOM QUERY");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextMultiline("##query", query_input_, sizeof(query_input_),
                              ImVec2(-1, 60));

    if (theme::AccentButton("Execute")) {
        execute_query();
    }
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "SELECT, PRAGMA, INSERT, UPDATE, DELETE");

    if (!query_result_.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
        ImGui::BeginChild("##query_result", ImVec2(-1, 100), ImGuiChildFlags_Borders);
        ImGui::TextColored(query_error_ ? ERROR_RED : SUCCESS, "%s",
                           query_result_.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}

void StorageCacheSection::render_db_browser() {
    using namespace theme::colors;

    theme::SectionHeader("DATABASE BROWSER");

    if (!db_authenticated_) {
        render_db_login();
    } else {
        // Logout button
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
        if (theme::SecondaryButton("Lock")) {
            db_authenticated_ = false;
            table_names_.clear();
            selected_table_ = -1;
        }
        render_table_browser();
    }
}

// ============================================================================
// Main render
// ============================================================================
void StorageCacheSection::render() {
    init();

    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "STORAGE & CACHE");
    ImGui::Separator();

    // Status message
    if (!status_.empty()) {
        double elapsed = ImGui::GetTime() - status_time_;
        if (elapsed < 5.0) {
            ImGui::TextColored(SUCCESS, "%s", status_.c_str());
        } else {
            status_.clear();
        }
    }

    ImGui::Spacing();

    ImGui::BeginChild("##storage_content", ImVec2(0, 0));

    render_cache_panel();
    ImGui::Spacing();
    render_db_browser();

    ImGui::EndChild();
}

} // namespace fincept::settings
