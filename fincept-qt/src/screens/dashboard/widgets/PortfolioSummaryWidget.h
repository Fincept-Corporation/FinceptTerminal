#pragma once
#include "screens/dashboard/widgets/PortfolioTileBase.h"

class QButtonGroup;
class QLabel;
class QTimer;

namespace fincept::ui {
class DataTable;
class DonutChart;
class KpiStrip;
} // namespace fincept::ui

namespace fincept::screens::widgets {

/// Portfolio summary tile — the selected portfolio as PortfolioService values
/// it (PortfolioService::load_summary → summary_loaded): every total in the
/// portfolio's base currency (holdings converted at spot FX by the service),
/// value, day P&L, unrealised P&L, gainers / losers, an allocation donut by
/// sector or by holding, and the largest holdings with weight bars. When the
/// service could not price or convert every holding the tile says which ones
/// the totals leave out. Config: {"portfolio_id": "<id>", "alloc": "sector"|"holding"}.
class PortfolioSummaryWidget : public PortfolioTileBase {
    Q_OBJECT
  public:
    explicit PortfolioSummaryWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_portfolio_selected() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void on_theme_changed() override;
    void retranslateUi() override;

  private:
    void render();

    ui::KpiStrip* kpis_ = nullptr;
    ui::DonutChart* donut_ = nullptr;
    ui::DataTable* table_ = nullptr;
    QButtonGroup* alloc_mode_ = nullptr;
    QLabel* footer_ = nullptr;
    QTimer* refresh_timer_ = nullptr;
    portfolio::PortfolioSummary summary_;
    bool have_summary_ = false;
    bool by_holding_ = false;
};

} // namespace fincept::screens::widgets
