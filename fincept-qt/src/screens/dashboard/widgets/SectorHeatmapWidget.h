#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QFrame>
#include <QGridLayout>
#include <QHash>
#include <QLabel>
#include <QVector>

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
    void retranslateUi() override;

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

    /// One persistent heat cell per sector. Cells are created once and then
    /// only have their text / background updated — the previous implementation
    /// destroyed and rebuilt 12 cells (36 inline setStyleSheet calls) on every
    /// single hub delivery.
    struct Cell {
        QFrame* frame = nullptr;
        QLabel* name = nullptr;
        QLabel* chg = nullptr;
        QString last_bg;   ///< last applied background css, to skip re-parses
        int last_sign = 0; ///< -1 / +1, 0 = never set
    };
    QVector<Cell> cells_;
    Cell& cell_at(int index);

    static QStringList sector_symbols();
    static QMap<QString, QString> sector_labels();

    QHash<QString, services::QuoteData> row_cache_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
