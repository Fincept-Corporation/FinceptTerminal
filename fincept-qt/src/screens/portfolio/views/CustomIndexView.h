// src/screens/portfolio/views/CustomIndexView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "storage/repositories/CustomIndexRepository.h"

#include <QChart>
#include <QChartView>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::ui {
class EmptyState;
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

/// Custom Index creation and tracking view.
/// Indices are persisted to SQLite via CustomIndexRepository.
///
/// PERFORMANCE never waits for a click elsewhere: it opens on the first index
/// (or the one last picked in its combo / MY INDICES), local data only; no index
/// yet, or a single recorded level, is a designed empty state with its action.
class CustomIndexView : public QWidget {
    Q_OBJECT
  public:
    explicit CustomIndexView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();
    /// Re-colours what the theme cannot restyle: table item colours and the chart.
    void refresh_theme();
    QWidget* build_create_panel();
    QWidget* build_index_list_panel();
    QWidget* build_performance_panel();

    void update_constituents();
    void create_index();
    void load_indices();
    /// MY INDICES table from loaded_indices_ / latest_values_ (no database read).
    void render_index_list();
    void delete_selected_index();
    void show_index_performance(const QString& index_id, const QString& name);
    /// Keep the PERFORMANCE combo in step with the indices and draw the selected
    /// one when that tab is on screen (otherwise on its next activation).
    void refresh_performance();

    /// Compute current index value from holdings using the given method.
    double compute_index_value(const CustomIndex& idx) const;

    // ── Tab container ─────────────────────────────────────────────────────────
    QTabWidget* tabs_ = nullptr;
    int create_tab_index_ = -1;
    int my_indices_tab_index_ = -1;
    int performance_tab_index_ = -1;

    // ── CREATE tab ────────────────────────────────────────────────────────────
    QLineEdit* name_edit_ = nullptr;
    QComboBox* method_cb_ = nullptr;
    QLineEdit* base_edit_ = nullptr;
    QTableWidget* const_table_ = nullptr;
    QPushButton* create_btn_ = nullptr;
    QLabel* create_status_ = nullptr;
    QLabel* create_title_ = nullptr;
    QLabel* name_field_label_ = nullptr;
    QLabel* method_field_label_ = nullptr;
    QLabel* base_field_label_ = nullptr;
    QLabel* method_desc_ = nullptr;
    QLabel* const_title_ = nullptr;

    // ── MY INDICES tab ────────────────────────────────────────────────────────
    QTableWidget* index_list_table_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    ui::EmptyState* list_empty_ = nullptr; ///< shown instead of the table when there is no index
    QLabel* indices_title_ = nullptr;

    // ── PERFORMANCE tab ───────────────────────────────────────────────────────
    QStackedWidget* perf_stack_ = nullptr; // 0=empty state, 1=tiles + chart
    ui::EmptyState* perf_empty_ = nullptr;
    ui::KpiStrip* perf_kpis_ = nullptr;
    ui::TimeSeriesChart* perf_chart_ = nullptr;
    QComboBox* perf_index_cb_ = nullptr;
    QLabel* perf_index_label_ = nullptr;
    QLabel* perf_title_ = nullptr;
    QString perf_index_id_;   ///< index shown (empty = the first one)
    QString perf_combo_sig_;  ///< ids + names in the combo (rebuilt only when they change)
    bool perf_dirty_ = true;  ///< redraw on the next activation of the tab
    QString last_perf_index_name_;
    bool last_perf_has_data_ = false;

    // ── State ─────────────────────────────────────────────────────────────────
    portfolio::PortfolioSummary summary_;
    QString currency_;
    QVector<CustomIndex> loaded_indices_; // cached from DB
    QHash<QString, double> latest_values_; ///< index id -> latest recorded level (read with loaded_indices_)
};

} // namespace fincept::screens
