#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QGridLayout>
#include <QHash>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

/// Portfolio Summary Widget — reads holdings from SQLite (portfolio table),
/// fetches live prices via yfinance, computes P&L and portfolio value.
/// Falls back to a demo portfolio if no DB holdings are found.
///
/// Once `load_holdings()` produces the holdings set the widget subscribes
/// to `market:quote:<sym>` on the DataHub for each holding. Any change to
/// the holdings set (e.g., refresh after a DB write) rewires subscriptions.
class PortfolioSummaryWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit PortfolioSummaryWidget(QWidget* parent = nullptr);

    struct Holding {
        QString symbol;
        double shares = 0;
        double avg_cost = 0;
    };

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void apply_styles();

  public:
    void load_holdings();
    void fetch_prices(const QVector<Holding>& holdings);
    void render(const QVector<Holding>& holdings, const QVector<services::QuoteData>& quotes);

    /// Re-subscribe to `market:quote:<sym>` for every holding. Drops old
    /// subscriptions first — holdings set may have changed since last call.
    void hub_resubscribe(const QVector<Holding>& holdings);
    void hub_unsubscribe_all();
    /// Rebuild quotes vector from `row_cache_` in `last_holdings_` order
    /// and re-render.
    void rebuild_from_cache();

    // Summary labels
    QLabel* total_value_lbl_ = nullptr;
    QLabel* day_pnl_lbl_ = nullptr;
    QLabel* total_pnl_lbl_ = nullptr;
    QLabel* num_holdings_lbl_ = nullptr;

    // Holdings list layout
    QVBoxLayout* list_layout_ = nullptr;

    // Widgets needing theme refresh
    QWidget* summary_card_ = nullptr;
    QWidget* header_row_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QVector<QLabel*> metric_labels_;
    QVector<QLabel*> metric_values_;
    QVector<QLabel*> header_labels_;

    // Cached for theme-change re-render
    QVector<Holding> last_holdings_;
    QVector<services::QuoteData> last_quotes_;

    QHash<QString, services::QuoteData> row_cache_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
