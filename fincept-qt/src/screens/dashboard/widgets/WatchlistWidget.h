#pragma once
#include "screens/dashboard/widgets/QuoteTableWidget.h"

#include <QHash>
#include <QLineEdit>

namespace fincept::screens::widgets {

/// Watchlist widget — user provides comma-separated symbols, fetches live data via yfinance.
///
/// The widget subscribes to `market:quote:<sym>` on the DataHub for each
/// symbol in the user's current symbol set. Because the symbol set is
/// dynamic (user edits the input), re-subscribe on every GO click and on
/// showEvent; unsubscribe on hideEvent.
class WatchlistWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit WatchlistWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void apply_styles();
    void refresh_data();
    void populate(const QVector<services::QuoteData>& quotes);

    /// (Re)subscribe to the hub for the current `symbols_` set.
    void hub_resubscribe();
    /// Tear down all hub subscriptions for this widget.
    void hub_unsubscribe_all();
    /// Redraw the table from `row_cache_` in `symbols_` order.
    void render_from_cache();

    QLineEdit* symbols_input_ = nullptr;
    QLabel* symbols_label_ = nullptr;
    QPushButton* go_btn_ = nullptr;
    ui::DataTable* table_ = nullptr;
    QStringList symbols_;

    QHash<QString, fincept::services::QuoteData> row_cache_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
