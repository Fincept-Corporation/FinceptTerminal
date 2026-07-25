#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QGridLayout>
#include <QLabel>
#include <QStringList>

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

    /// Per-instance config — the tracked symbol. The widget registry already
    /// seeds the constructor from `cfg["symbol"]`; these overrides close the
    /// loop so a symbol picked via the gear icon is written back to the
    /// persisted dashboard layout.
    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    QDialog* make_config_dialog(QWidget* parent) override;
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;

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
    // English source keys for stat_labels_, in the same order, so
    // retranslateUi() can re-run tr() without rebuilding the grid.
    QStringList stat_label_keys_;

    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
