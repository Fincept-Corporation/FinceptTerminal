#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QVector>

class QButtonGroup;
class QLabel;

namespace fincept::ui {
class CandleChart;
}

namespace fincept::screens::widgets {

/// Dashboard tile showing an OHLC candlestick chart (ui::CandleChart: volume
/// pane, crosshair with O/H/L/C/V readout, wheel zoom, drag pan, HiDPI cache,
/// export) for one ticker, with a timeframe selector:
///   1D = 1 day of 5-minute bars, 5D = 15-minute, 1M = hourly,
///   6M / 1Y = daily (1Y shares the performance history fetch), 5Y = weekly.
///
/// Pure DataHub consumer of `market:history:<SYM>:<period>:<interval>`
/// (MarketDataService::history_topic). Config: {"symbol": "...", "tf": "1Y"}.
/// Follows the tile's symbol link group. Visibility-driven subscribe /
/// unsubscribe per CLAUDE.md P3 / D3.
class DashboardCandleWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit DashboardCandleWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    QDialog* make_config_dialog(QWidget* parent) override;
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;
    /// Follows the tile's symbol link group.
    void on_linked_symbol(const SymbolRef& ref) override;
    SymbolRef linked_symbol() const override;

  private:
    QString history_topic() const;
    void hub_resubscribe();
    void refresh_data();
    void update_title();

    QString symbol_;
    int tf_ = 4; ///< index into the timeframe table (default 1Y)
    ui::CandleChart* chart_ = nullptr;
    QButtonGroup* tfs_ = nullptr;
    QLabel* footer_ = nullptr;
    QString currency_;
    int price_digits_ = -1;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
