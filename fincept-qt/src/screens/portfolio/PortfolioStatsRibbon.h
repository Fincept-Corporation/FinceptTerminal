// src/screens/portfolio/PortfolioStatsRibbon.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QWidget>

#include <optional>

class QComboBox;
class QLabel;

namespace fincept::ui {
class KpiStrip;
class StatTile;
} // namespace fincept::ui

namespace fincept::screens {

/// Stats ribbon — the KPI row beneath the command bar.
///
///   [PERIOD ▾] NAV (sparkline) · TODAY · RETURN <period> vs benchmark · UNREALISED P&L
///   (price / FX split) · REALISED + INCOME · CASH · VOLATILITY · SHARPE · BETA · MAX DD
///
/// Window-aware: the period selector picks the ComputedMetrics window the return
/// and risk tiles describe (window_changed() asks the owner to compute it). Every
/// figure is in the portfolio's base currency; an incomplete valuation is shown on
/// the NAV tile (warning tone + what is missing) instead of a silently low total.
class PortfolioStatsRibbon : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioStatsRibbon(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    /// Metrics for window() — others are ignored.
    void set_metrics(const portfolio::ComputedMetrics& metrics);
    portfolio::PerfWindow window() const { return window_; }
    void refresh_theme();

  signals:
    void window_changed(fincept::portfolio::PerfWindow window);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();
    void render_summary();
    void render_metrics();
    void set_risk_loading();
    QString window_label() const;

    QLabel* period_label_ = nullptr;
    QComboBox* period_cb_ = nullptr;
    ui::KpiStrip* strip_ = nullptr;
    ui::StatTile* nav_ = nullptr;
    ui::StatTile* day_ = nullptr;
    ui::StatTile* ret_ = nullptr;
    ui::StatTile* upnl_ = nullptr;
    ui::StatTile* real_ = nullptr;
    ui::StatTile* cash_ = nullptr;
    ui::StatTile* vol_ = nullptr;
    ui::StatTile* sharpe_ = nullptr;
    ui::StatTile* beta_ = nullptr;
    ui::StatTile* mdd_ = nullptr;

    portfolio::PerfWindow window_ = portfolio::PerfWindow::YTD;
    std::optional<portfolio::PortfolioSummary> summary_;
    std::optional<portfolio::ComputedMetrics> metrics_; // for window_ and summary_'s portfolio
};

} // namespace fincept::screens
