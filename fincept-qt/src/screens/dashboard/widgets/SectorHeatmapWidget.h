#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QGridLayout>
#include <QHash>

namespace fincept::screens::widgets {

/// Sector heatmap — fetches sector ETFs (XLK, XLV, XLF, etc.) via yfinance
/// and renders a color-intensity grid.
///
/// Subscribes to `market:quote:<ETF>` on the DataHub for each sector ETF.
/// On every delivery the grid is rebuilt from `row_cache_` so intensity
/// tints stay in sync.
class SectorHeatmapWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit SectorHeatmapWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void apply_styles();
    void refresh_data();
    void populate(const QVector<services::QuoteData>& quotes);

    void hub_subscribe_all();
    void hub_unsubscribe_all();
    /// Rebuild `QVector<QuoteData>` from the row cache (in declared symbol
    /// order) and hand to `populate()`.
    void rebuild_from_cache();

    QGridLayout* grid_ = nullptr;
    QWidget* grid_container_ = nullptr;

    static QStringList sector_symbols();
    static QMap<QString, QString> sector_labels();

    QHash<QString, services::QuoteData> row_cache_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
