#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QFrame>
#include <QLabel>

namespace fincept::screens::widgets {

/// Market sentiment widget — derives sentiment from VIX level and market breadth
/// (ratio of advancing vs declining among a basket of liquid stocks).
/// All data sourced from yfinance.
class MarketSentimentWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit MarketSentimentWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;

  private:
    void apply_styles();
    void refresh_data();
    void populate(const QVector<services::QuoteData>& quotes);

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
};

} // namespace fincept::screens::widgets
