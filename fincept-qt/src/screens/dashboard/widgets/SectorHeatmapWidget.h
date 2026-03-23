#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QGridLayout>

namespace fincept::screens::widgets {

/// Sector heatmap — fetches sector ETFs (XLK, XLV, XLF, etc.) via yfinance
/// and renders a color-intensity grid.
class SectorHeatmapWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit SectorHeatmapWidget(QWidget* parent = nullptr);

  private:
    void refresh_data();
    void populate(const QVector<services::QuoteData>& quotes);

    QGridLayout* grid_ = nullptr;
    QWidget* grid_container_ = nullptr;

    static QStringList sector_symbols();
    static QMap<QString, QString> sector_labels();
};

} // namespace fincept::screens::widgets
