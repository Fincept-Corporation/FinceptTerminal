#pragma once
// StorageSection.h — disk usage, data categories, file management, a read-only
// SQL console, and the danger zone for clearing all user data.

#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::storage_sql_runner {
struct Result;
}

namespace fincept::screens {

class StorageSection : public QWidget {
    Q_OBJECT
  public:
    explicit StorageSection(QWidget* parent = nullptr);

    /// Recompute all disk-usage statistics and refresh per-category counts.
    void refresh_storage_stats();

  protected:
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();

    /// One stylesheet for the whole section, keyed by object names; re-run on
    /// theme change. Replaces ~70 per-widget setStyleSheet() calls.
    void apply_theme();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    // ── SQL console ──────────────────────────────────────────────────────────
    /// Validate the statement (read-only, no credential tables) and run it off
    /// the UI thread on a private read-only connection.
    void run_sql();
    void show_sql_result(const storage_sql_runner::Result& result);
    /// `state` is "ok" / "error" / "warn" / "idle" — colours live in apply_theme().
    void set_sql_status(const QString& text, const char* state);
    void copy_sql_result();

    // Stat boxes
    QLabel* storage_count_ = nullptr;
    QLabel* storage_main_db_ = nullptr;
    QLabel* storage_cache_db_ = nullptr;
    QLabel* storage_log_size_ = nullptr;
    QLabel* storage_ws_size_ = nullptr;
    QLabel* storage_total_size_ = nullptr;
    QVBoxLayout* storage_categories_ = nullptr;

    // SQL console
    QLineEdit* sql_input_ = nullptr;
    QComboBox* sql_db_selector_ = nullptr;
    QLabel* sql_status_ = nullptr;
    QTableWidget* sql_table_ = nullptr;
    QPushButton* sql_copy_btn_ = nullptr;
    bool sql_running_ = false;

    // ── Fixed text widgets / panel titles (captured for retranslateUi) ────────
    QLabel* page_title_ = nullptr;
    QLabel* page_info_ = nullptr;
    QPushButton* disk_refresh_btn_ = nullptr;
    QLabel* disk_panel_title_ = nullptr;
    QLabel* main_db_box_lbl_ = nullptr;
    QLabel* cache_db_box_lbl_ = nullptr;
    QLabel* log_box_lbl_ = nullptr;
    QLabel* ws_box_lbl_ = nullptr;
    QLabel* total_box_lbl_ = nullptr;
    QLabel* cache_entries_row_lbl_ = nullptr;

    QLabel* categories_panel_title_ = nullptr;
    QLabel* cat_hdr_category_ = nullptr;
    QLabel* cat_hdr_entries_ = nullptr;
    QLabel* cat_hdr_action_ = nullptr;

    QLabel* files_panel_title_ = nullptr;
    QLabel* file_hdr_store_ = nullptr;
    QLabel* file_hdr_size_ = nullptr;
    QLabel* file_hdr_action_ = nullptr;
    QLabel* file_row_logs_lbl_ = nullptr;
    QLabel* file_row_ws_lbl_ = nullptr;
    QLabel* file_row_ui_lbl_ = nullptr;
    QLabel* cache_subcat_lbl_ = nullptr;

    QLabel* sql_panel_title_ = nullptr;
    QLabel* sql_hint_lbl_ = nullptr;
    QPushButton* sql_exec_btn_ = nullptr;
    QLabel* sql_quick_lbl_ = nullptr;

    QLabel* danger_panel_title_ = nullptr;
    QLabel* clear_cache_title_ = nullptr;
    QLabel* clear_cache_desc_ = nullptr;
    QPushButton* clear_cache_btn_ = nullptr;
    QLabel* nuke_title_ = nullptr;
    QLabel* nuke_desc_ = nullptr;
    QPushButton* nuke_btn_ = nullptr;
};

} // namespace fincept::screens
