#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"
#include "ui/tables/DataTable.h"

#include <QHash>
#include <QMap>

#include <functional>

namespace fincept::screens::widgets {

/// Reusable quote-table widget. Fetches symbols via MarketDataService and
/// displays them in a DataTable with customizable formatting.
///
/// The widget subscribes to `market:quote:<sym>` on the DataHub for each
/// symbol. Subscriptions are attached in `showEvent()` and torn down in
/// `hideEvent()` so the hub sees an accurate subscriber count (CLAUDE.md
/// P3 / D3).
class QuoteTableWidget : public BaseWidget {
    Q_OBJECT
  public:
    /// label_map: maps raw symbol -> display name (e.g. "^GSPC" -> "S&P 500").
    /// price_decimals: number of decimal places for price column.
    explicit QuoteTableWidget(const QString& title, const QStringList& symbols,
                              const QMap<QString, QString>& label_map = {}, int price_decimals = 2,
                              const QString& accent_color = {}, QWidget* parent = nullptr);

    void refresh_data();

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void apply_styles();
    void populate(const QVector<fincept::services::QuoteData>& quotes);

    /// Hub path: subscribe to every symbol's quote topic and apply updates
    /// into the row cache + redraw. Called from showEvent.
    void hub_subscribe_all();
    /// Hub path: unsubscribe and clear row cache. Called from hideEvent.
    void hub_unsubscribe_all();
    /// Render current `row_cache_` in the order the symbols were supplied.
    void render_from_cache();

    QStringList symbols_;
    QMap<QString, QString> label_map_;
    int price_decimals_;
    ui::DataTable* table_ = nullptr;

    /// Per-symbol latest quote, keyed by raw symbol. Used to rebuild the
    /// table preserving the declared symbol order when any one row
    /// updates.
    QHash<QString, fincept::services::QuoteData> row_cache_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
