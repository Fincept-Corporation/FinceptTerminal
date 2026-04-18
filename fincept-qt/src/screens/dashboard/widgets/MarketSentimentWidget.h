#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QFrame>
#include <QHash>
#include <QLabel>

namespace fincept::screens::widgets {

/// Market sentiment widget — derives sentiment from VIX level and market breadth
/// (ratio of advancing vs declining among a basket of liquid stocks).
/// All data sourced from yfinance.
///
/// Subscribes to `market:quote:<sym>` on the DataHub for the sentiment
/// basket and re-derives sentiment from the cache on every delivery.
class MarketSentimentWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit MarketSentimentWidget(QWidget* parent = nullptr);

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
    void rebuild_from_cache();

    QLabel* score_label_ = nullptr;
    QLabel* verdict_label_ = nullptr;
    QLabel* bull_label_ = nullptr;
    QLabel* bear_label_ = nullptr;
    QLabel* neutral_label_ = nullptr;
    QFrame* bull_bar_ = nullptr;
    QFrame* bear_bar_ = nullptr;
    QFrame* neutral_bar_ = nullptr;
    QLabel* vix_label_ = nullptr;
    QLabel* breadth_label_ = nullptr;

    // Widgets needing theme-aware restyling
    QWidget* banner_ = nullptr;
    QLabel* arrow_label_ = nullptr;
    QFrame* separator_ = nullptr;
    QLabel* vix_title_label_ = nullptr;
    QLabel* breadth_title_label_ = nullptr;

    QHash<QString, services::QuoteData> row_cache_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
