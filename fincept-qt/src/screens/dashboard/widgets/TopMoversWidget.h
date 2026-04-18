#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"
#include "ui/tables/DataTable.h"

#include <QHash>
#include <QPushButton>

#include <algorithm>

namespace fincept::screens::widgets {

/// Top movers widget with gainers/losers tab toggle. Fetches real data via yfinance.
///
/// Subscribes to `market:quote:<sym>` on the DataHub for the fixed mover
/// symbol set. The row cache is re-sorted into `all_quotes_` on every
/// delivery so the gainers/losers split stays current.
class TopMoversWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit TopMoversWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void apply_styles();
    void refresh_data();
    void show_tab(bool gainers);

    void hub_subscribe_all();
    void hub_unsubscribe_all();
    /// Recompute `all_quotes_` from `row_cache_`, sort, and redraw the tab.
    void rebuild_from_cache();

    ui::DataTable* table_ = nullptr;
    QPushButton* gainers_btn_ = nullptr;
    QPushButton* losers_btn_ = nullptr;
    QVector<services::QuoteData> all_quotes_;
    bool showing_gainers_ = true;

    QHash<QString, services::QuoteData> row_cache_;
    QStringList symbols_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
