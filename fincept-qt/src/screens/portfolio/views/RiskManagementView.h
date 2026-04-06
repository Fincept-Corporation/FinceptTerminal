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

  private:
    void build_ui();
    void update_overview();
    void update_stress_test();
    void update_contribution();

    QTabWidget* tabs_ = nullptr;

    // Overview tab
    QWidget* overview_panel_ = nullptr;

    // Stress test tab
    QTableWidget* stress_table_ = nullptr;

    // Risk contribution tab
    QTableWidget* contrib_table_ = nullptr;

    portfolio::PortfolioSummary summary_;
    portfolio::ComputedMetrics  metrics_;
    QString currency_;
};

} // namespace fincept::screens
