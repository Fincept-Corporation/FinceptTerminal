// src/screens/portfolio/views/RiskManagementView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Risk Management detail view with sub-tabs for Risk Overview, Stress Test,
/// and Risk Contribution analysis.
class RiskManagementView : public QWidget {
    Q_OBJECT
  public:
    explicit RiskManagementView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);
    void set_metrics(const portfolio::ComputedMetrics& metrics);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();
    void update_overview();
    void update_stress_test();
    void update_contribution();

    QTabWidget* tabs_ = nullptr;
    int overview_tab_index_ = -1;
    int stress_tab_index_ = -1;
    int contrib_tab_index_ = -1;

    // Overview tab
    QWidget* overview_panel_ = nullptr;

    // Stress test tab
    QTableWidget* stress_table_ = nullptr;
    QLabel* stress_title_ = nullptr;
    QLabel* stress_note_ = nullptr;

    // Risk contribution tab
    QTableWidget* contrib_table_ = nullptr;
    QLabel* contrib_title_ = nullptr;

    portfolio::PortfolioSummary summary_;
    portfolio::ComputedMetrics metrics_;
    QString currency_;
    bool has_data_ = false;
};

} // namespace fincept::screens
