#pragma once
// Quote-derived market tiles: FX cross matrix and market breadth.

#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataTypes.h"

#include <QHash>
#include <QJsonObject>

class QComboBox;
class QLabel;

namespace fincept::ui {
class KpiStrip;
class MatrixHeatmap;
} // namespace fincept::ui

namespace fincept::screens::widgets {

/// FX cross matrix (type id "fx_matrix"): today's % move of every row currency
/// against every column currency, derived from each currency's USD leg
/// (EURUSD=X, USDJPY=X, ...), on the diverging scale. The region's base
/// currency comes first. Hover shows the cross rate.
class FxMatrixWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit FxMatrixWidget(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void on_topic_errors_changed() override;
    void on_theme_changed() override;

  private:
    void subscribe_all();
    void render();
    QStringList currencies() const;

    ui::MatrixHeatmap* matrix_ = nullptr;
    QLabel* footer_ = nullptr;
    QHash<QString, services::QuoteData> legs_; ///< currency -> its USD-leg quote
    bool hub_active_ = false;
};

/// Market breadth (type id "market_breadth") of a STATED universe — the
/// region's large-cap basket (MarketProfile movers) or its sector set. It
/// never claims exchange-wide breadth (no source carries it): the title and
/// footer name the basket and its size. Advancers / decliners, share above the
/// 50-day average and near the 52-week high / low, average move.
/// Config: {"universe": "large_caps"|"sectors"}.
class MarketBreadthWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit MarketBreadthWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);
    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void on_theme_changed() override;

  private:
    QStringList universe() const;
    void subscribe_all();
    void render();

    QComboBox* universe_combo_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    QLabel* footer_ = nullptr;
    QString universe_ = QStringLiteral("large_caps");
    QString region_pin_; ///< config "market_region" (RegionPin.h); "" = follow the selector
    QHash<QString, services::QuoteData> quotes_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
