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

namespace fincept::screens {

/// Custom Index creation and tracking view with 10 calculation methods.
/// Indices are persisted to SQLite via CustomIndexRepository.
class CustomIndexView : public QWidget {
    Q_OBJECT
  public:
    explicit CustomIndexView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  private:
    void build_ui();
    QWidget* build_create_panel();
    QWidget* build_index_list_panel();
    QWidget* build_performance_panel();

    void update_constituents();
    void create_index();
    void load_indices();
    void delete_selected_index();
    void show_index_performance(const QString& index_id, const QString& name);

    /// Compute current index value from holdings using the given method.
    double compute_index_value(const CustomIndex& idx) const;

    // ── Tab container ─────────────────────────────────────────────────────────
    QTabWidget* tabs_ = nullptr;

    // ── CREATE tab ────────────────────────────────────────────────────────────
    QLineEdit*   name_edit_  = nullptr;
    QComboBox*   method_cb_  = nullptr;
    QLineEdit*   base_edit_  = nullptr;
    QTableWidget* const_table_ = nullptr;
    QPushButton* create_btn_   = nullptr;
    QLabel*      create_status_ = nullptr;

    // ── MY INDICES tab ────────────────────────────────────────────────────────
    QTableWidget* index_list_table_ = nullptr;
    QPushButton*  delete_btn_       = nullptr;
    QLabel*       list_empty_msg_   = nullptr;

    // ── PERFORMANCE tab ───────────────────────────────────────────────────────
    QStackedWidget* perf_stack_      = nullptr;  // 0=placeholder, 1=chart
    QChartView*     perf_chart_view_ = nullptr;
    QLabel*         perf_title_      = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    portfolio::PortfolioSummary summary_;
    QString currency_;
    QVector<CustomIndex> loaded_indices_;  // cached from DB
};

} // namespace fincept::screens
