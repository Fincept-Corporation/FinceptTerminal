#pragma once
// StorageSection.h — disk usage, data categories, file management, SQL console,
// and danger zone for clearing all user data.

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class StorageSection : public QWidget {
    Q_OBJECT
  public:
    explicit StorageSection(QWidget* parent = nullptr);

    /// Recompute all disk-usage statistics and refresh per-category counts.
    void refresh_storage_stats();

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    void build_ui();

    // Stat boxes
    QLabel* storage_count_       = nullptr;
    QLabel* storage_main_db_     = nullptr;
    QLabel* storage_cache_db_    = nullptr;
    QLabel* storage_log_size_    = nullptr;
    QLabel* storage_ws_size_     = nullptr;
    QLabel* storage_total_size_  = nullptr;
    QVBoxLayout* storage_categories_ = nullptr;

    // SQL console
    QLineEdit*   sql_input_         = nullptr;
    QComboBox*   sql_db_selector_   = nullptr;
    QLabel*      sql_status_        = nullptr;
    QVBoxLayout* sql_results_layout_ = nullptr;
};

} // namespace fincept::screens
