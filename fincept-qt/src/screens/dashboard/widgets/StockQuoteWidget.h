#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QGridLayout>
#include <QLabel>

namespace fincept::screens::widgets {

/// Single stock quote card — fetches real-time data via yfinance.
/// Shows price, change, high/low/open/volume in a detail layout.
///
/// Subscribes to `market:quote:<symbol_>` on the DataHub. `set_symbol()`
/// re-subscribes to the new topic. Visibility-driven subscribe/unsubscribe
/// per CLAUDE.md P3 / D3.
class StockQuoteWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit StockQuoteWidget(const QString& symbol = "AAPL", QWidget* parent = nullptr);

    void set_symbol(const QString& symbol);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void apply_styles();
    void refresh_data();
    void populate(const services::QuoteData& quote);

    /// (Re)subscribe to `market:quote:<symbol_>`. Called on show and on
    /// `set_symbol()`.
    void hub_resubscribe();
    void hub_unsubscribe_all();

    QString symbol_;

    QLabel* price_label_ = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* arrow_label_ = nullptr;
    QLabel* ticker_label_ = nullptr;
    QFrame* sep_ = nullptr;
    QLabel* open_val_ = nullptr;
    QLabel* high_val_ = nullptr;
    QLabel* low_val_ = nullptr;
    QLabel* volume_val_ = nullptr;
    QLabel* prev_val_ = nullptr;

    // Stat cells and their title labels for theme refresh
    QVector<QWidget*> stat_cells_;
    QVector<QLabel*> stat_labels_;
    QVector<QLabel*> stat_values_;

    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
