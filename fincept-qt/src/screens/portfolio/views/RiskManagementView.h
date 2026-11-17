// src/screens/portfolio/views/RiskManagementView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTabWidget;

namespace fincept::ui {
class BarChart;
class EmptyState;
class KpiStrip;
} // namespace fincept::ui

namespace fincept::screens::pfmt {
class PortfolioNoticeBar;
}

namespace fincept::screens {

class RiskWhatIfPanel;
class StressTestPanel;

/// RISK — ex-ante risk of the current book (trailing daily returns, base currency):
///   OVERVIEW      volatility, beta, tracking error, diversification, FX share of
///                 variance; VaR / CVaR by confidence and horizon — historical,
///                 parametric and Monte Carlo — as a table and grouped bars;
///   CONTRIBUTION  risk share vs weight bars per holding, marginal and component
///                 VaR, currency risk by currency;
///   STRESS        presets, user scenarios, historical replay (StressTestPanel);
///   WHAT-IF       target weights → before / after risk (RiskWhatIfPanel).
/// Confidence, horizon and Monte Carlo paths are selectable; RECALCULATE runs the
/// analytics with them. Loading, failure (with RETRY) and partial coverage are stated.
class RiskManagementView : public QWidget {
    Q_OBJECT
  public:
    explicit RiskManagementView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);
    void set_metrics(const portfolio::ComputedMetrics& metrics);
    void set_analytics(const portfolio::MarketAnalytics& analytics);
    /// Selects the STRESS tab (ECONOMICS › MACRO SHOCKS "OPEN RISK › STRESS").
    void show_stress_tab();

  signals:
    /// The empty-book state's BUY action (also forwarded from WHAT-IF).
    void add_position_requested();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void apply_styles();
    void recalculate(bool force);
    portfolio::RiskOptions options() const;
    bool matches_options(const portfolio::MarketAnalytics& a) const;
    void render();
    void render_overview();
    void render_contribution();
    void render_notice();

    QStackedWidget* body_ = nullptr; ///< 0 = risk views, 1 = empty book
    ui::EmptyState* empty_ = nullptr;
    pfmt::PortfolioNoticeBar* notice_ = nullptr;
    QComboBox* conf_cb_ = nullptr;
    QComboBox* horizon_cb_ = nullptr;
    QComboBox* paths_cb_ = nullptr;
    QPushButton* recalc_btn_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    QTableWidget* var_table_ = nullptr;
    ui::BarChart* var_chart_ = nullptr;
    QLabel* var_note_ = nullptr;
    ui::BarChart* contrib_chart_ = nullptr;
    QTableWidget* contrib_table_ = nullptr;
    QTableWidget* fx_table_ = nullptr;
    QLabel* contrib_note_ = nullptr;
    StressTestPanel* stress_ = nullptr;
    RiskWhatIfPanel* whatif_ = nullptr;

    portfolio::PortfolioSummary summary_;
    portfolio::ComputedMetrics metrics_;
    portfolio::MarketAnalytics analytics_;
    QString currency_;
    QString error_;          ///< last analytics failure for this book ("" = none)
    bool requested_ = false; ///< a custom-options run is in flight
    bool has_data_ = false;
};

} // namespace fincept::screens
