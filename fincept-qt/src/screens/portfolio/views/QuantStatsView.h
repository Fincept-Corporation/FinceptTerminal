// src/screens/portfolio/views/QuantStatsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QWidget>

namespace fincept::screens {

/// QuantStats report view with metrics, returns, drawdown, rolling, and Monte Carlo tabs.
class QuantStatsView : public QWidget {
    Q_OBJECT
  public:
    explicit QuantStatsView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  private:
    void build_ui();
    void update_metrics();
    void run_quantstats();
    void run_monte_carlo();

    QTabWidget* tabs_ = nullptr;

    // Metrics tab
    QTableWidget* metrics_table_ = nullptr;

    // Returns tab
    QWidget* returns_panel_ = nullptr;

    // Drawdown tab
    QWidget* drawdown_panel_ = nullptr;

    // Rolling tab
    QWidget* rolling_panel_ = nullptr;

    // Monte Carlo tab
    QPushButton* mc_run_btn_ = nullptr;
    QLabel* mc_status_ = nullptr;
    QWidget* mc_results_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QString currency_;
    bool mc_running_ = false;
};

} // namespace fincept::screens
