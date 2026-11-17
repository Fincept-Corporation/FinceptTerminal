#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataTypes.h"
#include "services/markets/MarketPerformance.h"

#include <QHash>
#include <QJsonObject>

class QButtonGroup;
class QLabel;

namespace fincept::ui {
class Treemap;
}

namespace fincept::screens::widgets {

/// Sector map — the MarketProfile region's sector set (SPDR sector ETFs, STOXX
/// Europe 600 sector ETFs, NSE sectoral indices, TOPIX-17 ETFs, ...) on a
/// squarified treemap: area = approximate index weight where the profile knows
/// it (else equal), colour = return over 1D / 1W / 1M / YTD on the theme's
/// diverging scale, clamped per period. The footer names the set, so it never
/// claims to be something it is not. Config: {"period": "1D"|"1W"|"1M"|"YTD"}.
class SectorHeatmapWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit SectorHeatmapWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void on_theme_changed() override;
    void retranslateUi() override;
    void on_topic_errors_changed() override;

  private:
    void hub_subscribe_all();
    void render();
    double value_for(const QString& sym) const;
    void update_title();
    /// Drop the shown sectors and subscribe the (new) region's set.
    void reload_region();

    QString region_pin_; ///< config "market_region" (RegionPin.h); "" = follow the selector
    ui::Treemap* map_ = nullptr;
    QButtonGroup* periods_ = nullptr;
    QLabel* footer_ = nullptr;
    int period_ = 0; ///< 0 = 1D, 1 = 1W, 2 = 1M, 3 = YTD
    QHash<QString, services::QuoteData> quotes_;
    QHash<QString, services::PerformanceStats> stats_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
