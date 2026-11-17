#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "screens/dashboard/widgets/QuoteGrid.h"
#include "services/markets/MarketPerformance.h"

#include <QHash>
#include <QJsonObject>

class QLabel;

namespace fincept::screens::widgets {

/// Market performance tile (type id "market_performance"; the old "performance"
/// id now shows the portfolio — see PortfolioPerformanceWidget).
///
/// One row per index of the user's MarketProfile region (or the tile's own list):
/// 1D from the live quote, 1W / 1M / YTD / 1Y from the shared daily history
/// (`MarketDataService::performance_history_topic`, compute_performance()), each
/// cell shaded on the theme's diverging scale. Periods drop out as the tile narrows.
class PerformanceWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit PerformanceWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void retranslateUi() override;
    void on_topic_errors_changed() override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    void set_entries(const QVector<QuoteGridEntry>& entries);
    void refresh_data();
    void hub_subscribe_all();
    void render();
    void update_columns();
    void update_title();

    ui::DataTable* table_ = nullptr;
    QLabel* footer_ = nullptr;
    QVector<QuoteGridEntry> entries_;
    bool follow_region_ = true;
    QString region_pin_; ///< config "market_region" (RegionPin.h); "" = follow the selector
    QString list_label_; ///< config "list_label": a template's named list ("Asia-Pacific")
    QHash<QString, services::QuoteData> quotes_;
    QHash<QString, services::PerformanceStats> stats_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
