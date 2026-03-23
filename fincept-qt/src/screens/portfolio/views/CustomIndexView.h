// src/screens/portfolio/views/CustomIndexView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QChartView>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Custom Index creation and tracking view with 10 calculation methods.
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

    QTabWidget* tabs_ = nullptr;

    // Create panel
    QLineEdit* name_edit_    = nullptr;
    QComboBox* method_cb_    = nullptr;
    QLineEdit* base_edit_    = nullptr;
    QTableWidget* const_table_ = nullptr;
    QPushButton* create_btn_ = nullptr;

    // Index list
    QTableWidget* index_list_table_ = nullptr;

    // Performance
    QChartView* perf_chart_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QString currency_;
};

} // namespace fincept::screens
