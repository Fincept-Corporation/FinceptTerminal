#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QComboBox>
#include <QHash>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

/// Stock Screener Widget — screens a broad basket by volume, change%, or price.
/// Fetches live data via yfinance batch_quotes and sorts/filters client-side.
///
/// Subscribes to `market:quote:<sym>` on the DataHub for the fixed
/// screener basket. Each delivery updates `row_cache_`;
/// `rebuild_all_quotes()` rebuilds `all_quotes_` and re-applies the current
/// filter so the sorted/top-20 view stays live.
class ScreenerWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit ScreenerWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void apply_styles();
    void refresh_data();
    void apply_filter();
    void render_rows(const QVector<services::QuoteData>& sorted);

    void hub_subscribe_all();
    void hub_unsubscribe_all();
    /// Recompute `all_quotes_` from `row_cache_` then `apply_filter()`.
    void rebuild_all_quotes();

    QComboBox* filter_combo_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QVector<services::QuoteData> all_quotes_;

    // Widgets needing theme-aware restyling
    QWidget* filter_bar_ = nullptr;
    QLabel* filter_lbl_ = nullptr;
    QLabel* count_lbl_ = nullptr;
    QWidget* header_ = nullptr;
    QVector<QLabel*> header_labels_;
    QScrollArea* scroll_ = nullptr;

    QHash<QString, services::QuoteData> row_cache_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
