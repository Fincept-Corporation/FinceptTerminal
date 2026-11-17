#pragma once
#include "screens/dashboard/widgets/PortfolioTileBase.h"

class QLabel;

namespace fincept::ui {
class KpiStrip;
class DataTable;
} // namespace fincept::ui

namespace fincept::screens::widgets {

/// Portfolio risk tile (type id "risk_metrics").
///
/// Ex-ante risk of the selected portfolio at its current weights, from one
/// PortfolioService::fetch_market_analytics() run (cached by the service per
/// portfolio / day / holdings): annualised volatility, beta and correlation vs
/// the benchmark, 1-day 95 % VaR and expected shortfall (as money of the NAV),
/// tracking error, diversification ratio, and each holding's share of the
/// portfolio variance. Market-wide risk is the separate "market_risk" tile.
class PortfolioRiskWidget : public PortfolioTileBase {
    Q_OBJECT
  public:
    explicit PortfolioRiskWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

  protected:
    void on_portfolio_selected() override;
    void showEvent(QShowEvent* e) override;
    void on_theme_changed() override;
    void retranslateUi() override;

  private:
    void render(const portfolio::MarketAnalytics& a);
    void set_waiting(const QString& text);

    ui::KpiStrip* kpis_ = nullptr;
    ui::DataTable* table_ = nullptr;
    QLabel* footer_ = nullptr;
    portfolio::PortfolioSummary summary_;
    bool have_summary_ = false;
    bool analytics_requested_ = false;
};

} // namespace fincept::screens::widgets
